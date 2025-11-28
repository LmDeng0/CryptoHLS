# # /home/CryptoHLS/libs/ModelDeploy/scripts/gnn_model.py
# import torch
# import torch.nn as nn
# import torch.nn.functional as F

# try:
#     from torch_geometric.nn import GCNConv, global_mean_pool
# except Exception as e:
#     raise ImportError("Please install torch-geometric. Error: {}".format(e))


# # ----------------------------
# # GraphEncoder（简洁可用版）
# # 输入维度=节点特征维度（你当前为 6：opcode(1)+结构特征(5)）
# # ----------------------------
# class GraphEncoder(nn.Module):
#     def __init__(self, in_dim: int, hidden: int):
#         super().__init__()
#         self.conv1 = GCNConv(in_dim, hidden)
#         self.conv2 = GCNConv(hidden, hidden)
#         self.lin_out = nn.Linear(hidden, hidden)

#     def forward(self, batch):
#         x, edge_index = batch.x, batch.edge_index
#         if getattr(batch, "batch", None) is None:
#             batch_idx = x.new_zeros(x.size(0), dtype=torch.long)
#         else:
#             batch_idx = batch.batch

#         h = F.relu(self.conv1(x, edge_index))
#         h = self.conv2(h, edge_index) + h   # 残差
#         g = global_mean_pool(h, batch_idx)  # [B, hidden]
#         g = self.lin_out(g)
#         return g


# # ----------------------------
# # PragmaEncoder（序列三路嵌入 + mean pool）
# # 词表规模对齐 gnN_dataset.py 的 VOCABS：type=5, key=6, value=10
# # ----------------------------
# class PragmaEncoder(nn.Module):
#     def __init__(self,
#                  type_vocab: int = 5,
#                  key_vocab: int = 6,
#                  value_vocab: int = 10,
#                  emb_dim: int = 64,
#                  hidden: int = 128):
#         super().__init__()
#         self.type_emb = nn.Embedding(type_vocab, emb_dim, padding_idx=0)
#         self.key_emb  = nn.Embedding(key_vocab,  emb_dim, padding_idx=0)
#         self.val_emb  = nn.Embedding(value_vocab, emb_dim, padding_idx=0)
#         self.proj = nn.Linear(emb_dim * 3, hidden)

#     def forward(self, batch):
#         # 期望 batch.sol_type_idx / sol_key_idx / sol_val_idx 形状 [B, L]
#         t = batch.sol_type_idx.long()
#         k = batch.sol_key_idx.long()
#         v = batch.sol_val_idx.long()
#         if t.dim() == 1:  # 兼容单条
#             t = t.unsqueeze(0); k = k.unsqueeze(0); v = v.unsqueeze(0)

#         te = self.type_emb(t)   # [B,L,emb]
#         ke = self.key_emb(k)    # [B,L,emb]
#         ve = self.val_emb(v)    # [B,L,emb]
#         seq = torch.cat([te, ke, ve], dim=-1)  # [B,L,3*emb]
#         seq = seq.mean(dim=1)                  # [B,3*emb]
#         out = F.relu(self.proj(seq))           # [B,hidden]
#         return out


# # ----------------------------
# # 多任务不确定性加权损失（Huber/L1）
# # ----------------------------
# class MultiHeadUncertaintyLoss(nn.Module):
#     def __init__(self, n_heads=3, use_huber=True, delta=1.0):
#         super().__init__()
#         self.log_sigma = nn.Parameter(torch.zeros(n_heads))
#         self.use_huber = use_huber
#         self.delta = delta

#     def forward(self, pred, target):
#         # pred/target: [B, H]
#         if self.use_huber:
#             diff = torch.abs(pred - target)
#             l = torch.where(diff < self.delta,
#                             0.5 * diff * diff / self.delta,
#                             diff - 0.5 * self.delta)
#         else:
#             l = torch.abs(pred - target)

#         loss = 0.0
#         for i in range(pred.size(1)):
#             s = self.log_sigma[i]
#             loss = loss + torch.mean(l[:, i] * torch.exp(-s) + s)
#         return loss


# # ----------------------------
# # 主模型：Graph + Pragma + Kernel Emb + Cond + Local
# # 默认输出 3 头（latency, II, LUT）
# # ----------------------------
# class CryptoGNN(nn.Module):
#     def __init__(self,
#                  node_in_dim: int = 6,     # 节点特征维度（opcode1 + 结构5）
#                  hidden: int = 128,
#                  type_vocab: int = 5,
#                  key_vocab: int = 6,
#                  value_vocab: int = 10,
#                  num_kernels: int = 2,
#                  kdim: int = 16,
#                  cond_dim: int = 8,        # 与 gnn_dataset.py 中 cond_vec 保持一致
#                  local_dim: int = 6,       # 与 gnn_dataset.py 中 local_feats 保持一致
#                  out_dim: int = 3):        # 目标头数：latency, II, LUT
#         super().__init__()
#         # 编码器
#         self.graph_enc  = GraphEncoder(node_in_dim, hidden)
#         self.pragma_enc = PragmaEncoder(type_vocab, key_vocab, value_vocab,
#                                         emb_dim=64, hidden=hidden)
#         # 额外分支
#         self.kernel_emb = nn.Embedding(num_kernels, kdim)
#         self.cond_mlp   = nn.Sequential(nn.Linear(cond_dim, kdim), nn.ReLU(inplace=True))
#         self.local_mlp  = nn.Sequential(nn.Linear(local_dim, kdim), nn.ReLU(inplace=True))

#         # 融合与回归头
#         fuse_in = hidden + hidden + kdim + kdim + kdim
#         self.mlp = nn.Sequential(
#             nn.Linear(fuse_in, hidden),
#             nn.ReLU(inplace=True),
#             nn.Linear(hidden, out_dim)
#         )

#         # 多任务不确定性损失
#         self.criterion = MultiHeadUncertaintyLoss(n_heads=out_dim, use_huber=True, delta=1.0)

#     def forward(self, batch):
#         g = self.graph_enc(batch)               # [B, hidden]
#         p = self.pragma_enc(batch)              # [B, hidden]

#         kid = batch.kernel_id.view(-1).long()   # [B] or [B,1] -> [B]
#         k = self.kernel_emb(kid)                # [B, kdim]

#         c = self.cond_mlp(batch.cond_vec)       # [B, kdim]
#         l = self.local_mlp(batch.local_feats)   # [B, kdim]

#         x = torch.cat([g, p, k, c, l], dim=1)   # [B, fuse_in]
#         y = self.mlp(x)                         # [B, out_dim]
#         return y


# gnn_model.py
import math
import torch
from torch import nn
from torch.nn import (
    Linear,
    Sequential,
    ReLU,
    Embedding,
    Dropout,
    BatchNorm1d,
)

from torch_geometric.nn import SAGEConv, GATConv, global_mean_pool

# ============================================================
# 与 train.py 一致的 PRAGMA 词表
# ============================================================
PRAGMA_COMPONENTS = {
    "type": [
        "<PAD>",
        "PIPELINE",
        "UNROLL",
        "ARRAY_PARTITION",
        "INLINE",
        "DATAFLOW",
        "INTERFACE",
        "STREAM",
    ],
    "key": [
        "<PAD>",
        "II",
        "factor",
        "type",
        "mode",
        "dim",
        "variable",
        "depth",
    ],
    "value": [
        "<PAD>",
        "1",
        "2",
        "4",
        "8",
        "16",
        "32",
        "block",
        "cyclic",
        "complete",
        "on",
        "off",
        "s_axilite",
        "m_axi",
        "state",
    ],
}
VOCAB_SIZES = {name: len(tokens) for name, tokens in PRAGMA_COMPONENTS.items()}


# ============================================================
# 损失函数（如果以后需要在 predict 里做不确定性估计也可以复用）
# ============================================================
class WeightedHuberLoss(nn.Module):
    def __init__(self, delta: float = 1.0, weights=(1.0, 1.0, 1.5)):
        """
        delta: Huber 损失阈值
        weights: 对 (latency, interval, LUT) 的维度权重
        """
        super().__init__()
        self.delta = float(delta)
        self.register_buffer(
            "w", torch.tensor(weights, dtype=torch.float32)
        )

    def forward(self, pred: torch.Tensor, target: torch.Tensor) -> torch.Tensor:
        # pred/target: [B, 3] 处于“标准化后的 log 空间”
        abs_error = torch.abs(pred - target)
        delta = self.delta

        quadratic = torch.minimum(
            abs_error,
            torch.tensor(delta, device=abs_error.device, dtype=abs_error.dtype),
        )
        linear = abs_error - quadratic
        loss = 0.5 * quadratic ** 2 + delta * linear  # [B,3]
        loss = loss * self.w.to(abs_error.device)      # 逐维加权
        return loss.mean()


# ============================================================
# 残差块（和 train.py 保持一致）
# ============================================================
class ResidualBlock(nn.Module):
    def __init__(self, in_dim: int, out_dim: int, p_dropout: float = 0.0):
        super().__init__()
        self.net = Sequential(
            Linear(in_dim, out_dim),
            BatchNorm1d(out_dim),
            ReLU(),
            Dropout(p_dropout),
            Linear(out_dim, out_dim),
            BatchNorm1d(out_dim),
        )
        self.relu = ReLU()
        self.shortcut = (
            Linear(in_dim, out_dim) if in_dim != out_dim else nn.Identity()
        )

    def forward(self, x: torch.Tensor) -> torch.Tensor:
        out = self.net(x)
        out = out + self.shortcut(x)
        return self.relu(out)


# ============================================================
# 主模型：图编码器 + pragma Transformer 编码器
# 和 train.py 中的 GNN_PPA_Predictor 一致
# ============================================================
class GNN_PPA_Predictor(nn.Module):
    def __init__(
        self,
        num_node_features: int,
        node_embedding_dim: int = 128,
        pragma_embedding_dim: int = 64,
        hidden_channels: int = 128,
        dropout_rate: float = 0.3,
    ):
        super().__init__()
        torch.manual_seed(42)

        # ------- 图侧 -------
        self.input_proj = Sequential(
            Linear(num_node_features, node_embedding_dim),
            BatchNorm1d(node_embedding_dim),
            ReLU(),
        )

        self.conv1 = SAGEConv(node_embedding_dim, hidden_channels)
        self.bn1 = BatchNorm1d(hidden_channels)

        # GATConv 输出维度 = hidden_channels * heads
        self.conv2 = GATConv(
            hidden_channels,
            hidden_channels,
            heads=2,
            concat=True,
        )
        self.bn2 = BatchNorm1d(2 * hidden_channels)

        self.res_block1 = ResidualBlock(
            2 * hidden_channels, hidden_channels, p_dropout=dropout_rate
        )
        self.res_block2 = ResidualBlock(
            hidden_channels, hidden_channels, p_dropout=dropout_rate
        )

        # ------- pragma 序列侧 -------
        self.type_emb = Embedding(
            VOCAB_SIZES["type"], pragma_embedding_dim
        )
        self.key_emb = Embedding(
            VOCAB_SIZES["key"], pragma_embedding_dim
        )
        self.val_emb = Embedding(
            VOCAB_SIZES["value"], pragma_embedding_dim
        )

        encoder_layer = nn.TransformerEncoderLayer(
            d_model=pragma_embedding_dim,
            nhead=2,
            dim_feedforward=pragma_embedding_dim * 4,
            dropout=dropout_rate,
            batch_first=True,
            activation="gelu",
        )
        self.pragma_encoder = nn.TransformerEncoder(
            encoder_layer, num_layers=1
        )

        # ------- 输出头：3 个目标 -------
        self.mlp = Sequential(
            ResidualBlock(
                hidden_channels + pragma_embedding_dim,
                hidden_channels,
                p_dropout=dropout_rate,
            ),
            Dropout(p=dropout_rate),
            ResidualBlock(
                hidden_channels,
                hidden_channels // 2,
                p_dropout=dropout_rate,
            ),
            Dropout(p=dropout_rate),
            Linear(hidden_channels // 2, 3),
        )

    # 正弦位置编码
    def _positional_encoding(self, L: int, D: int, device):
        pe = torch.zeros(L, D, device=device)
        position = torch.arange(0, L, device=device, dtype=torch.float).unsqueeze(1)
        div_term = torch.exp(
            torch.arange(0, D, 2, device=device, dtype=torch.float)
            * (-math.log(10000.0) / D)
        )
        pe[:, 0::2] = torch.sin(position * div_term)
        pe[:, 1::2] = torch.cos(position * div_term)
        return pe  # [L, D]

    def forward(self, data):
        # ------- 图侧 -------
        x, edge_index, batch = data.x, data.edge_index, data.batch
        x = x.to(torch.float)

        x = self.input_proj(x)
        x = self.conv1(x, edge_index)
        x = self.bn1(x).relu()
        x = self.conv2(x, edge_index)
        x = self.bn2(x).relu()
        x = self.res_block1(x)
        x = self.res_block2(x)

        graph_embedding = global_mean_pool(x, batch)  # [B, H]

        # ------- pragma 序列 -------
        type_idx = data.sol_type_idx  # [B, L]
        key_idx = data.sol_key_idx    # [B, L]
        val_idx = data.sol_val_idx    # [B, L]

        type_embeds = self.type_emb(type_idx)
        key_embeds = self.key_emb(key_idx)
        val_embeds = self.val_emb(val_idx)

        pragma_embeds = type_embeds + key_embeds + val_embeds  # [B, L, D]

        B, L, D = pragma_embeds.size()
        pe = self._positional_encoding(L, D, pragma_embeds.device).unsqueeze(0)
        pragma_embeds = pragma_embeds + pe  # [B, L, D]

        # padding mask：True 表示是 PAD，不参与注意力
        key_padding_mask = (
            (type_idx == 0) & (key_idx == 0) & (val_idx == 0)
        )  # [B, L]

        pragma_encoded = self.pragma_encoder(
            pragma_embeds,
            src_key_padding_mask=key_padding_mask,
        )  # [B, L, D]

        valid_len = (~key_padding_mask).sum(dim=1).clamp(min=1).unsqueeze(-1)  # [B,1]
        masked_sum = (
            pragma_encoded * (~key_padding_mask).unsqueeze(-1)
        ).sum(dim=1)  # [B, D]
        solution_embedding = masked_sum / valid_len  # [B, D]

        # ------- 融合 + 预测 -------
        combined = torch.cat([graph_embedding, solution_embedding], dim=1)
        out = self.mlp(combined)  # [B,3] 处于“标准化后的 log 空间”
        return out


# 一个简易的 builder，predict.py 可以用
def build_model(num_node_features: int, device: str = "cpu") -> GNN_PPA_Predictor:
    model = GNN_PPA_Predictor(
        num_node_features=num_node_features,
        node_embedding_dim=128,
        pragma_embedding_dim=64,
        hidden_channels=128,
        dropout_rate=0.3,
    )
    return model.to(device)
