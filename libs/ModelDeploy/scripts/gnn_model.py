# /home/CryptoHLS/libs/ModelDeploy/scripts/gnn_model.py
import torch
import torch.nn as nn
import torch.nn.functional as F

try:
    from torch_geometric.nn import GCNConv, global_mean_pool
except Exception as e:
    raise ImportError("Please install torch-geometric. Error: {}".format(e))


# ----------------------------
# GraphEncoder（简洁可用版）
# 输入维度=节点特征维度（你当前为 6：opcode(1)+结构特征(5)）
# ----------------------------
class GraphEncoder(nn.Module):
    def __init__(self, in_dim: int, hidden: int):
        super().__init__()
        self.conv1 = GCNConv(in_dim, hidden)
        self.conv2 = GCNConv(hidden, hidden)
        self.lin_out = nn.Linear(hidden, hidden)

    def forward(self, batch):
        x, edge_index = batch.x, batch.edge_index
        if getattr(batch, "batch", None) is None:
            batch_idx = x.new_zeros(x.size(0), dtype=torch.long)
        else:
            batch_idx = batch.batch

        h = F.relu(self.conv1(x, edge_index))
        h = self.conv2(h, edge_index) + h   # 残差
        g = global_mean_pool(h, batch_idx)  # [B, hidden]
        g = self.lin_out(g)
        return g


# ----------------------------
# PragmaEncoder（序列三路嵌入 + mean pool）
# 词表规模对齐 gnN_dataset.py 的 VOCABS：type=5, key=6, value=10
# ----------------------------
class PragmaEncoder(nn.Module):
    def __init__(self,
                 type_vocab: int = 5,
                 key_vocab: int = 6,
                 value_vocab: int = 10,
                 emb_dim: int = 64,
                 hidden: int = 128):
        super().__init__()
        self.type_emb = nn.Embedding(type_vocab, emb_dim, padding_idx=0)
        self.key_emb  = nn.Embedding(key_vocab,  emb_dim, padding_idx=0)
        self.val_emb  = nn.Embedding(value_vocab, emb_dim, padding_idx=0)
        self.proj = nn.Linear(emb_dim * 3, hidden)

    def forward(self, batch):
        # 期望 batch.sol_type_idx / sol_key_idx / sol_val_idx 形状 [B, L]
        t = batch.sol_type_idx.long()
        k = batch.sol_key_idx.long()
        v = batch.sol_val_idx.long()
        if t.dim() == 1:  # 兼容单条
            t = t.unsqueeze(0); k = k.unsqueeze(0); v = v.unsqueeze(0)

        te = self.type_emb(t)   # [B,L,emb]
        ke = self.key_emb(k)    # [B,L,emb]
        ve = self.val_emb(v)    # [B,L,emb]
        seq = torch.cat([te, ke, ve], dim=-1)  # [B,L,3*emb]
        seq = seq.mean(dim=1)                  # [B,3*emb]
        out = F.relu(self.proj(seq))           # [B,hidden]
        return out


# ----------------------------
# 多任务不确定性加权损失（Huber/L1）
# ----------------------------
class MultiHeadUncertaintyLoss(nn.Module):
    def __init__(self, n_heads=3, use_huber=True, delta=1.0):
        super().__init__()
        self.log_sigma = nn.Parameter(torch.zeros(n_heads))
        self.use_huber = use_huber
        self.delta = delta

    def forward(self, pred, target):
        # pred/target: [B, H]
        if self.use_huber:
            diff = torch.abs(pred - target)
            l = torch.where(diff < self.delta,
                            0.5 * diff * diff / self.delta,
                            diff - 0.5 * self.delta)
        else:
            l = torch.abs(pred - target)

        loss = 0.0
        for i in range(pred.size(1)):
            s = self.log_sigma[i]
            loss = loss + torch.mean(l[:, i] * torch.exp(-s) + s)
        return loss


# ----------------------------
# 主模型：Graph + Pragma + Kernel Emb + Cond + Local
# 默认输出 3 头（latency, II, LUT）
# ----------------------------
class CryptoGNN(nn.Module):
    def __init__(self,
                 node_in_dim: int = 6,     # 节点特征维度（opcode1 + 结构5）
                 hidden: int = 128,
                 type_vocab: int = 5,
                 key_vocab: int = 6,
                 value_vocab: int = 10,
                 num_kernels: int = 2,
                 kdim: int = 16,
                 cond_dim: int = 8,        # 与 gnn_dataset.py 中 cond_vec 保持一致
                 local_dim: int = 6,       # 与 gnn_dataset.py 中 local_feats 保持一致
                 out_dim: int = 3):        # 目标头数：latency, II, LUT
        super().__init__()
        # 编码器
        self.graph_enc  = GraphEncoder(node_in_dim, hidden)
        self.pragma_enc = PragmaEncoder(type_vocab, key_vocab, value_vocab,
                                        emb_dim=64, hidden=hidden)
        # 额外分支
        self.kernel_emb = nn.Embedding(num_kernels, kdim)
        self.cond_mlp   = nn.Sequential(nn.Linear(cond_dim, kdim), nn.ReLU(inplace=True))
        self.local_mlp  = nn.Sequential(nn.Linear(local_dim, kdim), nn.ReLU(inplace=True))

        # 融合与回归头
        fuse_in = hidden + hidden + kdim + kdim + kdim
        self.mlp = nn.Sequential(
            nn.Linear(fuse_in, hidden),
            nn.ReLU(inplace=True),
            nn.Linear(hidden, out_dim)
        )

        # 多任务不确定性损失
        self.criterion = MultiHeadUncertaintyLoss(n_heads=out_dim, use_huber=True, delta=1.0)

    def forward(self, batch):
        g = self.graph_enc(batch)               # [B, hidden]
        p = self.pragma_enc(batch)              # [B, hidden]

        kid = batch.kernel_id.view(-1).long()   # [B] or [B,1] -> [B]
        k = self.kernel_emb(kid)                # [B, kdim]

        c = self.cond_mlp(batch.cond_vec)       # [B, kdim]
        l = self.local_mlp(batch.local_feats)   # [B, kdim]

        x = torch.cat([g, p, k, c, l], dim=1)   # [B, fuse_in]
        y = self.mlp(x)                         # [B, out_dim]
        return y
