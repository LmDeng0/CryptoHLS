# #!/usr/bin/env python3
# # -*- coding: utf-8 -*-

# import os
# import sys
# import json
# import csv
# import argparse
# import pickle
# import math
# import numpy as np
# import torch
# import torch.nn.functional as F
# from torch.nn import Linear, Sequential, ReLU, Embedding, Dropout, BatchNorm1d, Module
# from torch_geometric.data import Data
# from torch_geometric.nn import SAGEConv, GATConv, global_mean_pool

# # ========================
# # 1) 词表，与 train.py 保持一致
# # ========================
# PRAGMA_COMPONENTS = {
#     'type': ['<PAD>', 'PIPELINE', 'UNROLL', 'ARRAY_PARTITION', 'INLINE'],
#     'key':  ['<PAD>', 'II', 'factor', 'type', 'mode', 'dim'],
#     'value':['<PAD>', '1', '2', '4', '8', 'block', 'cyclic', 'complete', 'on', 'off']
# }
# VOCAB_SIZES = {name: len(tokens) for name, tokens in PRAGMA_COMPONENTS.items()}
# TYPE2IDX = {t: i for i, t in enumerate(PRAGMA_COMPONENTS['type'])}
# KEY2IDX  = {t: i for i, t in enumerate(PRAGMA_COMPONENTS['key'])}
# VAL2IDX  = {t: i for i, t in enumerate(PRAGMA_COMPONENTS['value'])}

# # ========================
# # 2) 与 train.py 完全一致的模块
# # ========================
# class PragmasAttention(Module):
#     def __init__(self, embed_dim):
#         super().__init__()
#         self.query = Linear(embed_dim, embed_dim)
#         self.key   = Linear(embed_dim, embed_dim)
#         self.value = Linear(embed_dim, embed_dim)

#     def forward(self, pragma_embeds):
#         # pragma_embeds: [B, L, D]
#         Q = self.query(pragma_embeds)
#         K = self.key(pragma_embeds)
#         V = self.value(pragma_embeds)
#         scale = math.sqrt(pragma_embeds.size(-1))
#         attn_scores = torch.matmul(Q, K.transpose(1, 2)) / (scale if scale > 0 else 1.0)
#         attn_weights = F.softmax(attn_scores, dim=-1)
#         weighted_embeds = torch.matmul(attn_weights, V)  # [B, L, D]
#         return torch.mean(weighted_embeds, dim=1)        # [B, D]

# class ResidualBlock(Module):
#     def __init__(self, in_dim, out_dim):
#         super().__init__()
#         self.linear = Linear(in_dim, out_dim)
#         self.bn     = BatchNorm1d(out_dim)
#         self.relu   = ReLU()
#         self.shortcut = Linear(in_dim, out_dim) if in_dim != out_dim else None

#     def forward(self, x):
#         residual = x
#         out = self.linear(x)
#         out = self.bn(out)
#         out = self.relu(out)
#         if self.shortcut is not None:
#             residual = self.shortcut(residual)
#         return out + residual

# class GNN_PPA_Predictor(Module):
#     def __init__(self, num_node_features, node_embedding_dim, pragma_embedding_dim, hidden_channels, dropout_rate=0.4):
#         super().__init__()
#         torch.manual_seed(42)

#         # 输入投影
#         self.input_proj = Sequential(
#             Linear(num_node_features, node_embedding_dim),
#             BatchNorm1d(node_embedding_dim),
#             ReLU()
#         )

#         # GNN骨干
#         self.conv1 = SAGEConv(node_embedding_dim, hidden_channels)
#         self.bn1   = BatchNorm1d(hidden_channels)

#         self.conv2 = GATConv(hidden_channels, hidden_channels, heads=2, concat=True)
#         self.bn2   = BatchNorm1d(2 * hidden_channels)

#         self.res_block1 = ResidualBlock(2 * hidden_channels, hidden_channels)
#         self.res_block2 = ResidualBlock(hidden_channels, hidden_channels)

#         # Pragma嵌入
#         self.type_emb = Embedding(VOCAB_SIZES['type'],  pragma_embedding_dim)
#         self.key_emb  = Embedding(VOCAB_SIZES['key'],   pragma_embedding_dim)
#         self.val_emb  = Embedding(VOCAB_SIZES['value'], pragma_embedding_dim)

#         # 注意力
#         self.attn = PragmasAttention(pragma_embedding_dim)

#         # 预测头
#         self.mlp = Sequential(
#             ResidualBlock(hidden_channels + pragma_embedding_dim, hidden_channels),
#             Dropout(p=dropout_rate),
#             ResidualBlock(hidden_channels, hidden_channels // 2),
#             Dropout(p=dropout_rate),
#             Linear(hidden_channels // 2, 2)  # (latency, LUT) in scaled/log space
#         )

#     def forward(self, data):
#         x, edge_index, batch = data.x, data.edge_index, data.batch
#         x = x.to(torch.float)

#         x = self.input_proj(x)
#         x = self.conv1(x, edge_index)
#         x = self.bn1(x).relu()
#         x = self.conv2(x, edge_index)
#         x = self.bn2(x).relu()
#         x = self.res_block1(x)
#         x = self.res_block2(x)
#         graph_embedding = global_mean_pool(x, batch)  # [B, H]

#         type_idx, key_idx, val_idx = data.sol_type_idx, data.sol_key_idx, data.sol_val_idx  # [B, L]
#         type_embeds = self.type_emb(type_idx)
#         key_embeds  = self.key_emb(key_idx)
#         val_embeds  = self.val_emb(val_idx)
#         pragma_embeds = type_embeds + key_embeds + val_embeds  # [B, L, D]
#         solution_embedding = self.attn(pragma_embeds)          # [B, D]

#         combined = torch.cat([graph_embedding, solution_embedding], dim=1)
#         out = self.mlp(combined)  # [B, 2]  (scaled/log space)
#         return out

# # ========================
# # 3) 读CSV图 & 编码Solution
# # ========================
# def _read_numeric_row(row, numeric_cols):
#     vals = []
#     for c in numeric_cols:
#         v = row.get(c, "")
#         try:
#             vals.append(float(v))
#         except:
#             vals.append(0.0)
#     return vals

# def _first_row(path):
#     with open(path, 'r', newline='') as f:
#         r = csv.DictReader(f)
#         for row in r:
#             return row
#     return None

# def load_graph_from_csv(nodes_csv, edges_csv):
#     # 读节点
#     first = _first_row(nodes_csv)
#     with open(nodes_csv, 'r', newline='') as f:
#         reader = csv.DictReader(f)
#         fieldnames = reader.fieldnames or []
#         blacklist = set(['id', 'node', 'name', 'label', 'index'])

#     numeric_cols = []
#     if first:
#         for col in fieldnames:
#             if col in blacklist:
#                 continue
#             try:
#                 float(first[col])
#                 numeric_cols.append(col)
#             except:
#                 pass
#     if not numeric_cols:
#         # 如果一列也没识别到，就退回到所有非黑名单列尝试解析
#         numeric_cols = [c for c in fieldnames if c not in blacklist]

#     xs = []
#     with open(nodes_csv, 'r', newline='') as f2:
#         reader2 = csv.DictReader(f2)
#         for row in reader2:
#             xs.append(_read_numeric_row(row, numeric_cols))
#     x = torch.tensor(xs, dtype=torch.float32)

#     # 读边
#     with open(edges_csv, 'r', newline='') as f:
#         reader = csv.DictReader(f)
#         fn = set(reader.fieldnames or [])
#         if {'src','dst'}.issubset(fn):
#             s_col, t_col = 'src', 'dst'
#         elif {'source','target'}.issubset(fn):
#             s_col, t_col = 'source','target'
#         else:
#             cols = list(fn)
#             s_col, t_col = cols[0], cols[1]
#         srcs, dsts = [], []
#         for row in reader:
#             try:
#                 srcs.append(int(row[s_col]))
#                 dsts.append(int(row[t_col]))
#             except:
#                 continue
#     edge_index = torch.tensor([srcs, dsts], dtype=torch.long)
#     batch = torch.zeros(x.size(0), dtype=torch.long)
#     return x, edge_index, batch

# def encode_solution(solution_json, max_len=64):
#     try:
#         sol = json.loads(solution_json) if isinstance(solution_json, str) else solution_json
#         if not isinstance(sol, list):
#             sol = []
#     except Exception:
#         sol = []

#     type_ids, key_ids, val_ids = [], [], []
#     for item in sol:
#         t = str(item.get('type', '')).upper()
#         params = item.get('params', {}) or {}
#         if not params:
#             type_ids.append(TYPE2IDX.get(t, 0))
#             key_ids.append(KEY2IDX.get('<PAD>', 0))
#             val_ids.append(VAL2IDX.get('<PAD>', 0))
#             continue
#         for k, v in params.items():
#             k = str(k); v = str(v)
#             type_ids.append(TYPE2IDX.get(t, 0))
#             key_ids.append(KEY2IDX.get(k, 0))
#             val_ids.append(VAL2IDX.get(v, 0))

#     def pad_trunc(arr, L):
#         arr = arr[:L]
#         if len(arr) < L:
#             arr = arr + [0]*(L-len(arr))
#         return arr

#     type_ids = pad_trunc(type_ids, max_len)
#     key_ids  = pad_trunc(key_ids,  max_len)
#     val_ids  = pad_trunc(val_ids,  max_len)

#     type_idx = torch.tensor([type_ids], dtype=torch.long)
#     key_idx  = torch.tensor([key_ids],  dtype=torch.long)
#     val_idx  = torch.tensor([val_ids],  dtype=torch.long)
#     return type_idx, key_idx, val_idx

# # ========================
# # 4) 模型/Scaler加载（带特征维度对齐）
# # ========================
# def resolve_model_path(cli_model: str | None) -> str:
#     if cli_model:
#         return cli_model
#     env_model = os.environ.get("CRYPTOHLS_MODEL_PATH", "")
#     if env_model:
#         return env_model
#     here = os.path.dirname(os.path.abspath(__file__))
#     return os.path.join(here, "ppa_predictor_model_enhanced.pth")

# def load_state(model_path, device):
#     return torch.load(model_path, map_location=device)

# def build_model_from_state(state, device):
#     # 从 checkpoint 里反推出训练时的输入维度
#     if "input_proj.0.weight" not in state:
#         raise RuntimeError("Checkpoint missing key: input_proj.0.weight")
#     expected_in_features = state["input_proj.0.weight"].shape[1]

#     model = GNN_PPA_Predictor(
#         num_node_features=expected_in_features,
#         node_embedding_dim=128,
#         pragma_embedding_dim=64,
#         hidden_channels=256,
#         dropout_rate=0.4
#     ).to(device)
#     model.load_state_dict(state)
#     model.eval()
#     return model, expected_in_features

# def try_load_scaler():
#     path = os.environ.get("CRYPTOHLS_SCALER_PATH", "")
#     candidates = []
#     if path:
#         candidates.append(path)
#     here = os.path.dirname(os.path.abspath(__file__))
#     candidates.append(os.path.join(here, "ppa_scaler.pkl"))
#     candidates.append("ppa_scaler.pkl")

#     for p in candidates:
#         if os.path.isfile(p):
#             try:
#                 with open(p, "rb") as f:
#                     return pickle.load(f)
#             except Exception:
#                 pass
#     return None

# def align_features(x: torch.Tensor, expected_in_features: int) -> torch.Tensor:
#     """将 x 的列数对齐到 expected_in_features：不足右侧零填充，超出则截断。"""
#     cur = x.size(1)
#     if cur == expected_in_features:
#         return x
#     if cur < expected_in_features:
#         pad_cols = expected_in_features - cur
#         pad = torch.zeros((x.size(0), pad_cols), dtype=x.dtype)
#         return torch.cat([x, pad], dim=1)
#     else:
#         return x[:, :expected_in_features]

# # ========================
# # 5) 主流程
# # ========================
# def main():
#     parser = argparse.ArgumentParser()
#     parser.add_argument("--graph-nodes", required=True, help="Path to nodes CSV")
#     parser.add_argument("--graph-edges", required=True, help="Path to edges CSV")
#     parser.add_argument("--solution",      default="[]",   help="JSON array string for pragma solution")
#     parser.add_argument("--model",         default=None,   help="Path to model .pth (optional)")
#     args = parser.parse_args()

#     force_cpu = os.environ.get("CRYPTOHLS_FORCE_CPU", "0") == "1"
#     device = torch.device("cpu" if force_cpu or not torch.cuda.is_available() else "cuda")

#     # 先解析图（拿到原始特征）
#     x, edge_index, batch = load_graph_from_csv(args.graph_nodes, args.graph_edges)

#     # 加载 state，并用其中的 in_features 构建模型
#     model_path = resolve_model_path(args.model)
#     state = load_state(model_path, device)
#     model, expected_in_features = build_model_from_state(state, device)

#     # 将特征对齐到训练时的维度
#     x = align_features(x, expected_in_features)

#     # 编码 solution
#     type_idx, key_idx, val_idx = encode_solution(args.solution, max_len=64)

#     # 组装Data（单图，batch size=1）
#     data = Data(
#         x=x, edge_index=edge_index, batch=batch,
#         sol_type_idx=type_idx, sol_key_idx=key_idx, sol_val_idx=val_idx
#     ).to(device)

#     # 推理
#     with torch.no_grad():
#         pred_scaled = model(data)           # [1, 2]  —— 模型在训练时学习的是“标准化后的 log1p(y)”
#         pred_scaled_np = pred_scaled.cpu().numpy()

#     # 逆变换
#     scaler = try_load_scaler()
#     if scaler is not None:
#         pred_log = scaler.inverse_transform(pred_scaled_np)  # [1,2]
#         pred = np.expm1(pred_log)
#     else:
#         pred = np.expm1(pred_scaled_np)

#     pred = np.maximum(pred, 0.0)
#     latency = float(pred[0, 0])
#     lut     = float(pred[0, 1])

#     if os.environ.get("CRYPTOHLS_LOG_GNN", "0") == "1":
#         print(f"[GNN] latency={latency:.0f}, lut={lut:.0f}")

#     print(json.dumps({"predicted_latency": latency, "predicted_lut": lut}))

# if __name__ == "__main__":
#     try:
#         main()
#     except Exception as e:
#         print(json.dumps({
#             "predicted_latency": 10000.0,
#             "predicted_lut": 1_000_000.0,
#             "mode": "fallback",
#             "err": f"{e}"
#         }))
#         sys.exit(0)


import os
import sys
import json
import csv
import argparse
import pickle
import math
import numpy as np
import torch
import torch.nn.functional as F
from torch.nn import Linear, Sequential, ReLU, Embedding, Dropout, BatchNorm1d, Module
from torch_geometric.data import Data
from torch_geometric.nn import SAGEConv, GATConv, global_mean_pool

# ========================
# 1) 词表，与 train.py 保持一致
# ========================
PRAGMA_COMPONENTS = {
    'type': ['<PAD>', 'PIPELINE', 'UNROLL', 'ARRAY_PARTITION', 'INLINE'],
    'key':  ['<PAD>', 'II', 'factor', 'type', 'mode', 'dim'],
    'value':['<PAD>', '1', '2', '4', '8', 'block', 'cyclic', 'complete', 'on', 'off']
}
VOCAB_SIZES = {name: len(tokens) for name, tokens in PRAGMA_COMPONENTS.items()}
TYPE2IDX = {t: i for i, t in enumerate(PRAGMA_COMPONENTS['type'])}
KEY2IDX  = {t: i for i, t in enumerate(PRAGMA_COMPONENTS['key'])}
VAL2IDX  = {t: i for i, t in enumerate(PRAGMA_COMPONENTS['value'])}

# ========================
# 2) 与 train.py 完全一致的模块
# ========================
class PragmasAttention(Module):
    def __init__(self, embed_dim):
        super().__init__()
        self.query = Linear(embed_dim, embed_dim)
        self.key   = Linear(embed_dim, embed_dim)
        self.value = Linear(embed_dim, embed_dim)

    def forward(self, pragma_embeds):
        Q = self.query(pragma_embeds)
        K = self.key(pragma_embeds)
        V = self.value(pragma_embeds)
        scale = math.sqrt(pragma_embeds.size(-1))
        attn_scores = torch.matmul(Q, K.transpose(1, 2)) / (scale if scale > 0 else 1.0)
        attn_weights = F.softmax(attn_scores, dim=-1)
        weighted_embeds = torch.matmul(attn_weights, V)
        return torch.mean(weighted_embeds, dim=1)

class ResidualBlock(Module):
    def __init__(self, in_dim, out_dim):
        super().__init__()
        self.linear = Linear(in_dim, out_dim)
        self.bn     = BatchNorm1d(out_dim)
        self.relu   = ReLU()
        self.shortcut = Linear(in_dim, out_dim) if in_dim != out_dim else None

    def forward(self, x):
        residual = x
        out = self.linear(x)
        out = self.bn(out)
        out = self.relu(out)
        if self.shortcut is not None:
            residual = self.shortcut(residual)
        return out + residual

class GNN_PPA_Predictor(Module):
    def __init__(self, num_node_features, node_embedding_dim, pragma_embedding_dim, hidden_channels, dropout_rate=0.4):
        super().__init__()
        torch.manual_seed(42)

        self.input_proj = Sequential(
            Linear(num_node_features, node_embedding_dim),
            BatchNorm1d(node_embedding_dim),
            ReLU()
        )
        self.conv1 = SAGEConv(node_embedding_dim, hidden_channels)
        self.bn1   = BatchNorm1d(hidden_channels)
        self.conv2 = GATConv(hidden_channels, hidden_channels, heads=2, concat=True)
        self.bn2   = BatchNorm1d(2 * hidden_channels)
        self.res_block1 = ResidualBlock(2 * hidden_channels, hidden_channels)
        self.res_block2 = ResidualBlock(hidden_channels, hidden_channels)

        self.type_emb = Embedding(VOCAB_SIZES['type'],  pragma_embedding_dim)
        self.key_emb  = Embedding(VOCAB_SIZES['key'],   pragma_embedding_dim)
        self.val_emb  = Embedding(VOCAB_SIZES['value'], pragma_embedding_dim)

        self.attn = PragmasAttention(pragma_embedding_dim)

        # 输出改为 3 (latency, interval, lut)
        self.mlp = Sequential(
            ResidualBlock(hidden_channels + pragma_embedding_dim, hidden_channels),
            Dropout(p=dropout_rate),
            ResidualBlock(hidden_channels, hidden_channels // 2),
            Dropout(p=dropout_rate),
            Linear(hidden_channels // 2, 3)
        )

    def forward(self, data):
        x, edge_index, batch = data.x, data.edge_index, data.batch
        x = x.to(torch.float)

        x = self.input_proj(x)
        x = self.conv1(x, edge_index)
        x = self.bn1(x).relu()
        x = self.conv2(x, edge_index)
        x = self.bn2(x).relu()
        x = self.res_block1(x)
        x = self.res_block2(x)
        graph_embedding = global_mean_pool(x, batch)

        type_idx, key_idx, val_idx = data.sol_type_idx, data.sol_key_idx, data.sol_val_idx
        type_embeds = self.type_emb(type_idx)
        key_embeds  = self.key_emb(key_idx)
        val_embeds  = self.val_emb(val_idx)
        pragma_embeds = type_embeds + key_embeds + val_embeds
        solution_embedding = self.attn(pragma_embeds)

        combined = torch.cat([graph_embedding, solution_embedding], dim=1)
        out = self.mlp(combined)  # [B, 3]
        return out

# ========================
# 3) 读CSV图 & 编码Solution
# ========================
def _read_numeric_row(row, numeric_cols):
    vals = []
    for c in numeric_cols:
        v = row.get(c, "")
        try:
            vals.append(float(v))
        except:
            vals.append(0.0)
    return vals

def _first_row(path):
    with open(path, 'r', newline='') as f:
        r = csv.DictReader(f)
        for row in r:
            return row
    return None

def load_graph_from_csv(nodes_csv, edges_csv):
    first = _first_row(nodes_csv)
    with open(nodes_csv, 'r', newline='') as f:
        reader = csv.DictReader(f)
        fieldnames = reader.fieldnames or []
        blacklist = set(['id', 'node', 'name', 'label', 'index'])
    numeric_cols = []
    if first:
        for col in fieldnames:
            if col in blacklist:
                continue
            try:
                float(first[col])
                numeric_cols.append(col)
            except:
                pass
    if not numeric_cols:
        numeric_cols = [c for c in fieldnames if c not in blacklist]

    xs = []
    with open(nodes_csv, 'r', newline='') as f2:
        reader2 = csv.DictReader(f2)
        for row in reader2:
            xs.append(_read_numeric_row(row, numeric_cols))
    x = torch.tensor(xs, dtype=torch.float32)

    with open(edges_csv, 'r', newline='') as f:
        reader = csv.DictReader(f)
        fn = set(reader.fieldnames or [])
        if {'src','dst'}.issubset(fn):
            s_col, t_col = 'src', 'dst'
        elif {'source','target'}.issubset(fn):
            s_col, t_col = 'source','target'
        else:
            cols = list(fn)
            s_col, t_col = cols[0], cols[1]
        srcs, dsts = [], []
        for row in reader:
            try:
                srcs.append(int(row[s_col]))
                dsts.append(int(row[t_col]))
            except:
                continue
    edge_index = torch.tensor([srcs, dsts], dtype=torch.long)
    batch = torch.zeros(x.size(0), dtype=torch.long)
    return x, edge_index, batch

def encode_solution(solution_json, max_len=64):
    try:
        sol = json.loads(solution_json) if isinstance(solution_json, str) else solution_json
        if not isinstance(sol, list):
            sol = []
    except Exception:
        sol = []
    type_ids, key_ids, val_ids = [], [], []
    for item in sol:
        t = str(item.get('type', '')).upper()
        params = item.get('params', {}) or {}
        if not params:
            type_ids.append(TYPE2IDX.get(t, 0))
            key_ids.append(KEY2IDX.get('<PAD>', 0))
            val_ids.append(VAL2IDX.get('<PAD>', 0))
            continue
        for k, v in params.items():
            type_ids.append(TYPE2IDX.get(t, 0))
            key_ids.append(KEY2IDX.get(str(k), 0))
            val_ids.append(VAL2IDX.get(str(v), 0))

    def pad_trunc(arr, L):
        arr = arr[:L]
        if len(arr) < L:
            arr = arr + [0]*(L-len(arr))
        return arr

    type_idx = torch.tensor([pad_trunc(type_ids, max_len)], dtype=torch.long)
    key_idx  = torch.tensor([pad_trunc(key_ids,  max_len)], dtype=torch.long)
    val_idx  = torch.tensor([pad_trunc(val_ids,  max_len)], dtype=torch.long)
    return type_idx, key_idx, val_idx

# ========================
# 4) 模型/Scaler加载
# ========================
def resolve_model_path(cli_model: str | None) -> str:
    if cli_model:
        return cli_model
    env_model = os.environ.get("CRYPTOHLS_MODEL_PATH", "")
    if env_model:
        return env_model
    here = os.path.dirname(os.path.abspath(__file__))
    return os.path.join(here, "ppa_predictor_model_enhanced.pth")

def load_state(model_path, device):
    return torch.load(model_path, map_location=device)

def build_model_from_state(state, device):
    if "input_proj.0.weight" not in state:
        raise RuntimeError("Checkpoint missing key: input_proj.0.weight")
    expected_in_features = state["input_proj.0.weight"].shape[1]
    model = GNN_PPA_Predictor(
        num_node_features=expected_in_features,
        node_embedding_dim=128,
        pragma_embedding_dim=64,
        hidden_channels=256
    ).to(device)
    model.load_state_dict(state)
    model.eval()
    return model, expected_in_features

def try_load_scaler():
    path = os.environ.get("CRYPTOHLS_SCALER_PATH", "")
    candidates = [path] if path else []
    here = os.path.dirname(os.path.abspath(__file__))
    candidates.append(os.path.join(here, "ppa_scaler.pkl"))
    candidates.append("ppa_scaler.pkl")
    for p in candidates:
        if os.path.isfile(p):
            try:
                with open(p, "rb") as f:
                    return pickle.load(f)
            except:
                pass
    return None

def align_features(x: torch.Tensor, expected_in_features: int) -> torch.Tensor:
    cur = x.size(1)
    if cur == expected_in_features:
        return x
    if cur < expected_in_features:
        pad = torch.zeros((x.size(0), expected_in_features - cur), dtype=x.dtype)
        return torch.cat([x, pad], dim=1)
    else:
        return x[:, :expected_in_features]

# ========================
# 5) 主流程
# ========================
def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--graph-nodes", required=True)
    parser.add_argument("--graph-edges", required=True)
    parser.add_argument("--solution", default="[]")
    parser.add_argument("--model", default=None)
    args = parser.parse_args()

    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    x, edge_index, batch = load_graph_from_csv(args.graph_nodes, args.graph_edges)
    model_path = resolve_model_path(args.model)
    state = load_state(model_path, device)
    model, expected_in_features = build_model_from_state(state, device)
    x = align_features(x, expected_in_features)
    type_idx, key_idx, val_idx = encode_solution(args.solution, max_len=64)

    data = Data(x=x, edge_index=edge_index, batch=batch,
                sol_type_idx=type_idx, sol_key_idx=key_idx, sol_val_idx=val_idx).to(device)

    with torch.no_grad():
        pred_scaled = model(data)        # [1, 3]
        pred_scaled_np = pred_scaled.cpu().numpy()

    scaler = try_load_scaler()
    if scaler is not None:
        pred_log = scaler.inverse_transform(pred_scaled_np)  # [1,3]
        pred = np.expm1(pred_log)
    else:
        pred = np.expm1(pred_scaled_np)

    pred = np.maximum(pred, 0.0)
    latency  = float(pred[0, 0])
    interval = float(pred[0, 1])
    lut      = float(pred[0, 2])

    if os.environ.get("CRYPTOHLS_LOG_GNN", "0") == "1":
        print(f"[GNN] latency={latency:.0f}, interval={interval:.0f}, lut={lut:.0f}")

    print(json.dumps({
        "predicted_latency": latency,
        "predicted_interval": interval,
        "predicted_lut": lut
    }))

if __name__ == "__main__":
    try:
        main()
    except Exception as e:
        print(json.dumps({
            "predicted_latency": 10000.0,
            "predicted_interval": 1.0,
            "predicted_lut": 1_000_000.0,
            "mode": "fallback",
            "err": f"{e}"
        }))
        sys.exit(0)
