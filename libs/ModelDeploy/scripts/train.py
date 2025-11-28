import os
import math
import copy
import random
import argparse
import pickle
from typing import Tuple, Dict

import numpy as np
import torch
import torch.nn.functional as F
from torch import nn
from torch_geometric.loader import DataLoader

from gnn_dataset import CryptoHLS_Dataset


# ----------------- 词表（和 gnn_dataset 中编码保持一致/向后兼容） -----------------
PRAGMA_COMPONENTS = {
    'type':  ['<PAD>', 'PIPELINE', 'UNROLL', 'ARRAY_PARTITION', 'INLINE', 'DATAFLOW', 'INTERFACE', 'STREAM'],
    'key':   ['<PAD>', 'II', 'factor', 'type', 'mode', 'dim', 'variable', 'depth'],
    'value': ['<PAD>', '1', '2', '4', '8', '16', '32', 'block', 'cyclic', 'complete', 'on', 'off', 's_axilite', 'm_axi', 'state']
}
VOCAB_SIZES = {k: len(v) for k, v in PRAGMA_COMPONENTS.items()}


# ----------------- 损失函数：加权 Huber，用于 (latency, interval, LUT) 多任务 -----------------
class WeightedHuberLoss(nn.Module):
    def __init__(self, delta=1.0, weights=(1.0, 0.0, 1.0)):
        """
        delta: Huber 损失阈值
        weights: 对 (latency, interval, LUT) 的维度权重
        """
        super().__init__()
        self.delta = delta
        self.register_buffer("w", torch.tensor(weights, dtype=torch.float32))

    def forward(self, pred: torch.Tensor, target: torch.Tensor) -> torch.Tensor:
        # pred/target: [B, 3] （标准化后的 log 空间）
        abs_error = torch.abs(pred - target)
        delta = self.delta
        quadratic = torch.minimum(abs_error, torch.tensor(delta, device=abs_error.device))
        linear = abs_error - quadratic
        loss = 0.5 * quadratic**2 + delta * linear  # [B,3]
        loss = loss * self.w.to(abs_error.device)
        return loss.mean()


# ----------------- 一些基础模块：残差块 -----------------
class ResidualBlock(nn.Module):
    def __init__(self, in_dim: int, out_dim: int, p_dropout: float = 0.0):
        super().__init__()
        self.net = nn.Sequential(
            nn.Linear(in_dim, out_dim),
            nn.BatchNorm1d(out_dim),
            nn.ReLU(),
            nn.Dropout(p_dropout),
            nn.Linear(out_dim, out_dim),
            nn.BatchNorm1d(out_dim),
        )
        self.relu = nn.ReLU()
        self.shortcut = nn.Linear(in_dim, out_dim) if in_dim != out_dim else nn.Identity()

    def forward(self, x: torch.Tensor) -> torch.Tensor:
        out = self.net(x)
        out = out + self.shortcut(x)
        return self.relu(out)


# ----------------- GNN + Pragma Transformer 主模型 -----------------
from torch_geometric.nn import SAGEConv, GATConv, global_mean_pool

class GNN_PPA_Predictor(nn.Module):
    def __init__(self,
                 num_node_features: int,
                 node_embedding_dim: int = 128,
                 pragma_embedding_dim: int = 64,
                 hidden_channels: int = 128,
                 dropout_rate: float = 0.3):
        super().__init__()
        torch.manual_seed(42)

        # 图侧 encoder
        self.input_proj = nn.Sequential(
            nn.Linear(num_node_features, node_embedding_dim),
            nn.BatchNorm1d(node_embedding_dim),
            nn.ReLU()
        )
        self.conv1 = SAGEConv(node_embedding_dim, hidden_channels)
        self.bn1 = nn.BatchNorm1d(hidden_channels)
        self.conv2 = GATConv(hidden_channels, hidden_channels, heads=2, concat=True)
        self.bn2 = nn.BatchNorm1d(2 * hidden_channels)
        self.res_block1 = ResidualBlock(2 * hidden_channels, hidden_channels, p_dropout=dropout_rate)
        self.res_block2 = ResidualBlock(hidden_channels, hidden_channels, p_dropout=dropout_rate)

        # pragma 侧 encoder：type/key/value 三路嵌入 + 位置编码 + TransformerEncoder
        self.type_emb = nn.Embedding(VOCAB_SIZES['type'],  pragma_embedding_dim)
        self.key_emb  = nn.Embedding(VOCAB_SIZES['key'],   pragma_embedding_dim)
        self.val_emb  = nn.Embedding(VOCAB_SIZES['value'], pragma_embedding_dim)

        encoder_layer = nn.TransformerEncoderLayer(
            d_model=pragma_embedding_dim,
            nhead=2,
            dim_feedforward=pragma_embedding_dim * 4,
            dropout=dropout_rate,
            batch_first=True,
            activation='gelu'
        )
        self.pragma_encoder = nn.TransformerEncoder(encoder_layer, num_layers=1)

        # 输出 MLP：预测 3 维 QoR（log+standardized 空间）
        self.mlp = nn.Sequential(
            ResidualBlock(hidden_channels + pragma_embedding_dim, hidden_channels, p_dropout=dropout_rate),
            nn.Dropout(p=dropout_rate),
            ResidualBlock(hidden_channels, hidden_channels // 2, p_dropout=dropout_rate),
            nn.Dropout(p=dropout_rate),
            nn.Linear(hidden_channels // 2, 3)
        )

    def _positional_encoding(self, L: int, D: int, device) -> torch.Tensor:
        pe = torch.zeros(L, D, device=device)
        position = torch.arange(0, L, device=device, dtype=torch.float).unsqueeze(1)
        div_term = torch.exp(torch.arange(0, D, 2, device=device, dtype=torch.float) * (-math.log(10000.0) / D))
        pe[:, 0::2] = torch.sin(position * div_term)
        pe[:, 1::2] = torch.cos(position * div_term)
        return pe

    def forward(self, data):
        x, edge_index, batch = data.x, data.edge_index, data.batch
        x = x.to(torch.float)

        # --- 图侧 ---
        x = self.input_proj(x)
        x = self.conv1(x, edge_index)
        x = self.bn1(x).relu()
        x = self.conv2(x, edge_index)
        x = self.bn2(x).relu()
        x = self.res_block1(x)
        x = self.res_block2(x)
        graph_embedding = global_mean_pool(x, batch)  # [B, H]

        # --- pragma 序列侧 ---
        type_idx, key_idx, val_idx = data.sol_type_idx, data.sol_key_idx, data.sol_val_idx  # [B, L]
        type_embeds = self.type_emb(type_idx)
        key_embeds  = self.key_emb(key_idx)
        val_embeds  = self.val_emb(val_idx)
        pragma_embeds = type_embeds + key_embeds + val_embeds  # [B, L, D]

        B, L, D = pragma_embeds.size()
        pe = self._positional_encoding(L, D, pragma_embeds.device).unsqueeze(0)
        pragma_embeds = pragma_embeds + pe

        # padding mask
        key_padding_mask = (type_idx == 0) & (key_idx == 0) & (val_idx == 0)  # [B, L]
        pragma_encoded = self.pragma_encoder(pragma_embeds, src_key_padding_mask=key_padding_mask)  # [B, L, D]

        # masked mean pool
        valid_len = (~key_padding_mask).sum(dim=1).clamp(min=1).unsqueeze(-1)  # [B,1]
        masked_sum = (pragma_encoded * (~key_padding_mask).unsqueeze(-1)).sum(dim=1)  # [B,D]
        solution_embedding = masked_sum / valid_len  # [B,D]

        # --- 融合 & 输出 ---
        combined = torch.cat([graph_embedding, solution_embedding], dim=1)
        out = self.mlp(combined)  # [B,3] in standardized log space
        return out


# ----------------- 轻度数据增强（可选） -----------------
def augment_data(data):
    # 边随机丢 10%
    if data.edge_index.size(1) > 0:
        if random.random() < 0.2:
            num_edges = data.edge_index.size(1)
            keep_mask = torch.rand(num_edges, device=data.edge_index.device) > 0.1
            if keep_mask.sum() == 0:
                keep_mask[random.randrange(num_edges)] = True
            data.edge_index = data.edge_index[:, keep_mask]
    # 节点特征加少量噪声
    if random.random() < 0.3:
        data.x = data.x.to(torch.float)
        noise = torch.randn_like(data.x) * 0.05
        data.x = data.x + noise
    return data


# ----------------- 工具函数 -----------------
def set_seed(seed: int = 42):
    random.seed(seed)
    np.random.seed(seed)
    torch.manual_seed(seed)
    torch.cuda.manual_seed_all(seed)


def to_scaled_log_targets(y_raw_np: np.ndarray, scaler, device) -> torch.Tensor:
    # y_raw: [N,3] in original space
    y_log = np.log1p(y_raw_np)
    y_scaled = scaler.transform(y_log)
    return torch.from_numpy(y_scaled).to(device=device, dtype=torch.float32)


@torch.no_grad()
def evaluate(model: nn.Module,
             val_loader: DataLoader,
             scaler,
             device,
             out_csv: str = None) -> Dict[str, float]:
    """
    评估：返回 {mape_mean, mape_latency, mape_interval, mape_lut}
    如果 val_loader 为空，返回 NaN 并打印提示，而不是直接崩掉。
    """
    model.eval()
    preds_scaled = []
    ys_raw = []

    for data in val_loader:
        data = data.to(device)
        out = model(data)  # [B,3] scaled log
        preds_scaled.append(out.cpu().numpy())
        ys_raw.append(data.y.cpu().numpy())

    if len(preds_scaled) == 0:
        print("[Eval] WARNING: val_loader has no batches. Returning NaN metrics.")
        return {
            "mape_mean": float("nan"),
            "mape_latency": float("nan"),
            "mape_interval": float("nan"),
            "mape_lut": float("nan"),
        }

    p_scaled = np.concatenate(preds_scaled, axis=0)     # scaled log
    y_raw = np.concatenate(ys_raw, axis=0)              # original

    # 反标准化 & 反 log1p
    p_log = scaler.inverse_transform(p_scaled)
    p = np.expm1(p_log)
    p = np.maximum(p, 0)

    eps = 1e-6
    mape_latency  = float(np.mean(np.abs(p[:, 0] - y_raw[:, 0]) / (np.abs(y_raw[:, 0]) + eps)) * 100.0)
    mape_interval = float(np.mean(np.abs(p[:, 1] - y_raw[:, 1]) / (np.abs(y_raw[:, 1]) + eps)) * 100.0)
    mape_lut      = float(np.mean(np.abs(p[:, 2] - y_raw[:, 2]) / (np.abs(y_raw[:, 2]) + eps)) * 100.0)
    mape_mean     = float((mape_latency + mape_interval + mape_lut) / 2.0)

    # 可选：把预测写到 CSV
    if out_csv:
        import csv, os
        os.makedirs(os.path.dirname(out_csv), exist_ok=True)
        with open(out_csv, "w", newline="") as f:
            writer = csv.writer(f)
            writer.writerow(["lat_true", "int_true", "lut_true",
                             "lat_pred", "int_pred", "lut_pred"])
            for yt, yp in zip(y_raw, p):
                writer.writerow(list(yt) + list(yp))

    return {
        "mape_mean": mape_mean,
        "mape_latency": mape_latency,
        "mape_interval": mape_interval,
        "mape_lut": mape_lut,
    }


# ----------------- 主训练流程 -----------------
def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=str, default=".")
    parser.add_argument("--epochs", type=int, default=200)
    parser.add_argument("--early_stop", type=int, default=30)
    parser.add_argument("--batch_size", type=int, default=128)
    parser.add_argument("--lr", type=float, default=2e-4)
    parser.add_argument("--save", type=str, default="ppa_predictor_model_enhanced.pth")
    parser.add_argument("--val_csv", type=str, default="")
    args = parser.parse_args()

    set_seed(42)

    # ---- 载入数据集 ----
    print(os.environ.get("CRYPTOHLS_DATASET", ""))
    dataset = CryptoHLS_Dataset(root=args.root)
    total = len(dataset)
    if total == 0:
        print("[train.py] ERROR: dataset is empty. Please collect more HLS samples first.")
        return
    print(f"[train.py] Loaded dataset with {total} samples.")

    # ---- 切分 train / val，保证至少有 1 条用于验证 ----
    dataset = dataset.shuffle()
    if total <= 1:
        train_dataset = dataset
        val_dataset = dataset
    else:
        val_size = max(1, int(round(0.2 * total)))
        train_size = max(1, total - val_size)
        from torch.utils.data import random_split
        train_dataset, val_dataset = random_split(dataset, [train_size, val_size])
    print(f"[train.py] Train size: {len(train_dataset)}, Val size: {len(val_dataset)}")

    # ---- 拟合 scaler（在训练集上） ----
    if hasattr(train_dataset, "indices"):
        train_indices = train_dataset.indices
    else:
        # 没有 indices 属性时，退化为全部样本
        train_indices = list(range(len(dataset)))
    dataset.fit_scaler_on_split(train_indices)
    scaler = dataset.scaler

    # ---- DataLoader，batch_size 不超过数据量 ----
    bs_train = min(args.batch_size, max(1, len(train_dataset)))
    bs_val   = min(args.batch_size, max(1, len(val_dataset)))
    train_loader = DataLoader(train_dataset, batch_size=bs_train, shuffle=True)
    val_loader   = DataLoader(val_dataset, batch_size=bs_val, shuffle=False)

    # ---- 设备 & 模型 ----
    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    print("[train.py] Using device:", device)

    num_node_features = dataset[0].x.size(1)
    print(f"[train.py] Detected {num_node_features} node features.")

    model = GNN_PPA_Predictor(
        num_node_features=num_node_features,
        node_embedding_dim=128,
        pragma_embedding_dim=64,
        hidden_channels=128,
        dropout_rate=0.3
    ).to(device)

    optimizer = torch.optim.AdamW(model.parameters(), lr=args.lr, weight_decay=1e-4)

    # warmup + 余弦退火
    def lr_lambda(epoch):
        if epoch < 10:
            return (epoch + 1) / 10.0
        t = (epoch - 10) / 40.0
        return 0.5 * (1 + math.cos(math.pi * min(1.0, t)))

    scheduler = torch.optim.lr_scheduler.LambdaLR(optimizer, lr_lambda=lr_lambda)
    loss_fn = WeightedHuberLoss(delta=1.0, weights=(1.0, 0.0, 1.0))

    def train_one_epoch(epoch: int) -> float:
        model.train()
        total_loss = 0.0
        for data in train_loader:
            data = data.to(device)
            data = augment_data(data)

            y_np = data.y.cpu().numpy()
            if y_np.ndim == 1:
                if len(y_np) % 3 != 0:
                    raise ValueError(f"data.y length {len(y_np)} not divisible by 3")
                y_np = y_np.reshape(-1, 3)
            elif y_np.ndim == 2 and y_np.shape[1] != 3:
                raise ValueError(f"data.y shape {y_np.shape} unexpected, expected (N,3)")

            y_scaled = to_scaled_log_targets(y_np, scaler, device)

            optimizer.zero_grad()
            out = model(data)
            loss = loss_fn(out, y_scaled)
            loss.backward()
            torch.nn.utils.clip_grad_norm_(model.parameters(), 1.0)
            optimizer.step()
            total_loss += loss.item() * data.num_graphs
        avg_loss = total_loss / len(train_loader.dataset)
        return avg_loss

    # ---- 训练循环 ----
    best_state = None
    best_mape = float("inf")
    patience = args.early_stop
    counter = 0

    for epoch in range(1, args.epochs + 1):
        loss = train_one_epoch(epoch)
        metrics = evaluate(model, val_loader, scaler, device, out_csv=args.val_csv if epoch == args.epochs else None)
        mape_mean = metrics["mape_mean"]

        print(f"Epoch {epoch:3d}, Loss {loss:.4f}, "
              f"Val MAPE {mape_mean:.2f}% "
              f"(lat {metrics['mape_latency']:.2f}%, "
              f"int {metrics['mape_interval']:.2f}%, "
              f"lut {metrics['mape_lut']:.2f}%)")

        scheduler.step()

        # 避免 NaN 影响 early stopping：NaN 视为非常差
        cmp_mape = mape_mean if not math.isnan(mape_mean) else float("inf")
        if cmp_mape < best_mape - 1e-4:
            best_mape = cmp_mape
            best_state = copy.deepcopy(model.state_dict())
            counter = 0
            print(f"[train.py] New best at epoch {epoch}, MAPE {mape_mean:.2f}%")
        else:
            counter += 1
            if counter >= patience:
                print("[train.py] Early stopping triggered.")
                break

    if best_state is not None:
        torch.save(best_state, args.save)
        with open(os.path.splitext(args.save)[0] + "_scaler.pkl", "wb") as f:
            pickle.dump(scaler, f)
        print(f"[train.py] Best model saved to {args.save} with MAPE {best_mape:.2f}%")
    else:
        print("[train.py] WARNING: No best_state recorded (dataset too small or all NaN).")

if __name__ == "__main__":
    main()
