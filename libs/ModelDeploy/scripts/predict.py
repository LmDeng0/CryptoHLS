#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
CryptoHLS GNN predictor (single-sample inference).

被 DSE.cpp 通过命令行调用：

  python3 libs/ModelDeploy/scripts/predict.py \
    --graph-nodes <nodes.csv> \
    --graph-edges <edges.csv> \
    --solution '<JSON of pragma commands>'

输出一行 JSON：
  {"predicted_latency": X, "predicted_interval": Y, "predicted_lut": Z}

模型和 scaler 默认从仓库根目录下加载：
  ppa_predictor_model_enhanced.pth
  ppa_scaler.pkl

也可以用环境变量覆盖：
  CRYPTOHLS_MODEL=/path/to/model.pth
  CRYPTOHLS_SCALER=/path/to/scaler.pkl
"""

import argparse
import json
import math
import os
from pathlib import Path
from typing import Tuple

import numpy as np
import pandas as pd
import torch
from torch_geometric.data import Data

from sklearn.preprocessing import StandardScaler

# 复用我们在 gnn_dataset.py 里写好的编码函数
from gnn_dataset import (
    encode_solution_with_padding,
    add_structural_features,
)

# 复用训练脚本里的模型结构
from gnn_model import GNN_PPA_Predictor

# ---------- 词表（要和 train.py / gnn_dataset 保持一致） ----------
PRAGMA_COMPONENTS = {
    'type':  ['<PAD>', 'PIPELINE', 'UNROLL', 'ARRAY_PARTITION', 'INLINE', 'DATAFLOW', 'INTERFACE', 'STREAM'],
    'key':   ['<PAD>', 'II', 'factor', 'type', 'mode', 'dim', 'variable', 'depth'],
    'value': ['<PAD>', '1', '2', '4', '8', '16', '32', 'block', 'cyclic', 'complete', 'on', 'off', 's_axilite', 'm_axi', 'state']
}
VOCAB_SIZES = {name: len(tokens) for name, tokens in PRAGMA_COMPONENTS.items()}


def load_scaler(path: Path) -> StandardScaler:
    import pickle
    with path.open("rb") as f:
        scaler = pickle.load(f)
    if not isinstance(scaler, StandardScaler):
        raise TypeError(f"Scaler at {path} is not a StandardScaler.")
    return scaler


def build_single_data(graph_nodes_path: Path,
                      graph_edges_path: Path,
                      solution_json: str) -> Data:
    """
    根据 nodes/edges CSV 和 solution JSON，构造一个单图 Data 对象。
    node 特征和结构特征要和 gnn_dataset.process() 对齐。
    """
    nodes_df = pd.read_csv(graph_nodes_path)
    edges_df = pd.read_csv(graph_edges_path)

    # 对齐列名
    if 'source' not in edges_df.columns or 'target' not in edges_df.columns:
        raise ValueError(f"edges csv missing source/target columns: {graph_edges_path}")

    edges_df['source'] = edges_df['source'].astype(int)
    edges_df['target'] = edges_df['target'].astype(int)

    # 结构特征
    structural_feats = add_structural_features(nodes_df, edges_df)

    # 假定第一列是 opcode（和 gnn_dataset 里的逻辑保持一致）
    opcodes = nodes_df.iloc[:, 0].values
    x = np.column_stack([opcodes, structural_feats])
    x = torch.tensor(x, dtype=torch.float32)

    edge_index = torch.tensor(
        edges_df[['source', 'target']].values, dtype=torch.long
    ).t().contiguous()

    # pragma 序列编码
    solution = json.loads(solution_json)
    sol_type, sol_key, sol_val = encode_solution_with_padding(solution)
    sol_type_idx = torch.tensor(sol_type, dtype=torch.long).unsqueeze(0)
    sol_key_idx  = torch.tensor(sol_key,  dtype=torch.long).unsqueeze(0)
    sol_val_idx  = torch.tensor(sol_val,  dtype=torch.long).unsqueeze(0)

    # 预测时不需要 y
    data = Data(
        x=x,
        edge_index=edge_index,
        sol_type_idx=sol_type_idx,
        sol_key_idx=sol_key_idx,
        sol_val_idx=sol_val_idx,
    )
    # 单图 batch 编号全 0
    data.batch = torch.zeros(x.size(0), dtype=torch.long)
    return data


def inverse_transform_outputs(
    preds_scaled: np.ndarray,
    scaler: StandardScaler
) -> np.ndarray:
    """
    preds_scaled: [1,3]，在“标准化后的 log 空间”
    返回: [1,3]，在原始空间（latency, interval, lut）
    """
    preds_log = scaler.inverse_transform(preds_scaled)
    preds = np.expm1(preds_log)
    preds = np.maximum(preds, 0.0)
    return preds


def main():
    # ----------- 解析参数 -----------
    ap = argparse.ArgumentParser()
    ap.add_argument("--graph-nodes", required=True)
    ap.add_argument("--graph-edges", required=True)
    ap.add_argument("--solution", required=True)
    args = ap.parse_args()

    # ----------- 定位 repo 根目录 -----------
    script_path = Path(__file__).resolve()
    scripts_dir = script_path.parent          # .../libs/ModelDeploy/scripts
    repo_root   = scripts_dir.parents[2]      # .../CryptoHLS

    # ----------- 模型 & scaler 路径 -----------
    model_path = os.environ.get(
        "CRYPTOHLS_MODEL",
        str(repo_root / "ppa_predictor_model_enhanced.pth")
    )
    scaler_path = os.environ.get(
        "CRYPTOHLS_SCALER",
        str(repo_root / "ppa_scaler.pkl")
    )

    model_path = Path(model_path).resolve()
    scaler_path = Path(scaler_path).resolve()

    if not model_path.exists():
        raise FileNotFoundError(f"Model file not found: {model_path}")
    if not scaler_path.exists():
        raise FileNotFoundError(f"Scaler file not found: {scaler_path}")

    # ----------- 设备 -----------
    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")

    # ----------- 加载 scaler -----------
    scaler = load_scaler(scaler_path)

    # ----------- 构造 Data 样本 -----------
    g_nodes = Path(args.graph_nodes).resolve()
    g_edges = Path(args.graph_edges).resolve()
    data = build_single_data(g_nodes, g_edges, args.solution)
    num_node_features = data.x.size(1)

    # ----------- 构造并加载模型 -----------
    model = GNN_PPA_Predictor(
        num_node_features=num_node_features,
        node_embedding_dim=128,
        pragma_embedding_dim=64,
        hidden_channels=128,
        dropout_rate=0.0,  # 推理时可以关掉 dropout
    )
    state = torch.load(model_path, map_location="cpu")
    model.load_state_dict(state)
    model = model.to(device)
    model.eval()

    # ----------- 前向推理 -----------
    with torch.no_grad():
        data = data.to(device)
        out_scaled = model(data)  # [1,3] in scaled log space
        preds_scaled = out_scaled.cpu().numpy()
        preds = inverse_transform_outputs(preds_scaled, scaler)[0]  # [3]

    latency, interval, lut = preds.tolist()

    # 为避免 Interval 爆炸，可以稍微裁剪一下（可根据需要调）
    latency  = float(latency)
    interval = float(interval)
    lut      = float(lut)

    # ----------- 输出 JSON（DSE.cpp 通过 parsePredictOutput 解析） -----------
    result = {
        "predicted_latency":  latency,
        "predicted_interval": interval,
        "predicted_lut":      lut,
    }
    print(json.dumps(result))


if __name__ == "__main__":
    main()
