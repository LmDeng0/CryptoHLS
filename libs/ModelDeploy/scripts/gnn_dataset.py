import os
import json
import pandas as pd
import torch
from torch_geometric.data import Dataset, Data
from sklearn.preprocessing import StandardScaler
import numpy as np
import networkx as nx

# ============================
# PRAGMA 组件与编码（保持兼容）
# ============================
PRAGMA_COMPONENTS = {
    'type': ['<PAD>', 'PIPELINE', 'UNROLL', 'ARRAY_PARTITION', 'INLINE'],
    'key':  ['<PAD>', 'II', 'factor', 'type', 'mode', 'dim'],
    'value': ['<PAD>', '1', '2', '4', '8', 'block', 'cyclic', 'complete', 'on', 'off']
}
VOCABS = {name: {tok: i for i, tok in enumerate(toks)}
          for name, toks in PRAGMA_COMPONENTS.items()}

MAX_PRAGMAS = 20

def encode_solution_with_padding(solution_json):
    """
    将 solution 序列编码为 (type_idx, key_idx, value_idx)，长度对齐到 MAX_PRAGMAS
    """
    type_indices, key_indices, value_indices = [], [], []
    for cmd in (solution_json or []):
        ctype = cmd.get('type', '')
        params = cmd.get('params', {})
        if ctype in VOCABS['type']:
            # 无参数的 pragma（例如 INLINE）
            if not params:
                type_indices.append(VOCABS['type'].get(ctype, 0))
                key_indices.append(VOCABS['key']['<PAD>'])
                value_indices.append(VOCABS['value']['<PAD>'])
            else:
                for k, v in params.items():
                    type_indices.append(VOCABS['type'].get(ctype, 0))
                    key_indices.append(VOCABS['key'].get(str(k), 0))
                    value_indices.append(VOCABS['value'].get(str(v), 0))
    # 对齐
    num = len(type_indices)
    pad = MAX_PRAGMAS - num
    if pad > 0:
        type_indices.extend([VOCABS['type']['<PAD>']] * pad)
        key_indices.extend([VOCABS['key']['<PAD>']] * pad)
        value_indices.extend([VOCABS['value']['<PAD>']] * pad)
    elif pad < 0:
        type_indices = type_indices[:MAX_PRAGMAS]
        key_indices = key_indices[:MAX_PRAGMAS]
        value_indices = value_indices[:MAX_PRAGMAS]
    return type_indices, key_indices, value_indices


# ============================
# 结构特征（图统计）
# ============================
def add_structural_features(nodes_df, edges_df):
    """
    为每个节点添加 5 维结构特征：
    [度, 聚类系数, 从节点0的最短路径深度, 介数中心性, 连通分量编号]
    """
    G = nx.DiGraph()
    num_nodes = len(nodes_df)
    G.add_nodes_from(range(num_nodes))

    # 安全转换边
    if 'source' in edges_df.columns and 'target' in edges_df.columns:
        for _, row in edges_df.iterrows():
            try:
                s = int(row['source']); t = int(row['target'])
                if 0 <= s < num_nodes and 0 <= t < num_nodes:
                    G.add_edge(s, t)
            except Exception:
                continue

    # 度（有向图下这里使用无向度，更稳）
    degrees = np.array([G.degree(i) for i in range(num_nodes)], dtype=float)

    # 聚类系数（转无向）
    try:
        clustering_values = nx.clustering(G.to_undirected())
        clustering = np.array([clustering_values.get(i, 0.0) for i in range(num_nodes)], dtype=float)
    except Exception:
        clustering = np.zeros(num_nodes, dtype=float)

    # 从0号节点（通常是入口）到各节点的“路径深度”（不可达置0）
    path_depth = np.zeros(num_nodes, dtype=float)
    try:
        lengths = nx.single_source_shortest_path_length(G, 0)
        for node, dist in lengths.items():
            if 0 <= node < num_nodes:
                path_depth[node] = float(dist)
    except Exception:
        pass

    # 介数中心性（可能较慢，节点多时可替换为近似或跳过）
    try:
        betw = nx.betweenness_centrality(G, normalized=True)
        betweenness = np.array([betw.get(i, 0.0) for i in range(num_nodes)], dtype=float)
    except Exception:
        betweenness = np.zeros(num_nodes, dtype=float)

    # 连通分量编号（无向）
    connected_components = np.zeros(num_nodes, dtype=float)
    try:
        comp_values = {}
        for idx, comp in enumerate(nx.connected_components(G.to_undirected())):
            for node in comp:
                comp_values[node] = idx
        for i in range(num_nodes):
            connected_components[i] = float(comp_values.get(i, 0))
    except Exception:
        connected_components[:] = 0.0

    structural_feats = np.stack(
        [degrees, clustering, path_depth, betweenness, connected_components], axis=1
    )
    return structural_feats


# ============================
# Kernel 与 条件向量
# ============================
KERNEL_VOCAB = {"aes": 0, "sha1": 1}

def kernel_to_id(name: str) -> int:
    n = (name or "").lower()
    for k in KERNEL_VOCAB:
        if k in n:
            return KERNEL_VOCAB[k]
    return 0  # 默认 aes

def infer_kernel_name(sample: dict) -> str:
    top = sample.get("top") or sample.get("top_function")
    if top:
        return top
    sols = sample.get("solution", [])
    fns = [s.get("function") for s in sols if s.get("function")]
    return fns[0] if fns else "aes"


# ============================
# 从 solution 构造“局部对齐”最小特征
# ============================
def build_local_feats_from_solution(solution_json):
    """
    最小代价的“pragma→局部池化特征”（6维）：
    [has_pipeline, target_ii, has_unroll, unroll_factor, has_partition, partition_factor]
    若缺失则用 0。
    """
    has_pipeline = 0.0
    target_ii = 0.0
    has_unroll = 0.0
    unroll_factor = 0.0
    has_partition = 0.0
    partition_factor = 0.0

    for cmd in (solution_json or []):
        ctype = (cmd.get("type") or "").upper()
        params = cmd.get("params", {}) or {}

        if ctype == "PIPELINE":
            has_pipeline = 1.0
            # II 可能在 key: II
            ii = params.get("II")
            if ii is not None:
                try:
                    target_ii = float(ii)
                except Exception:
                    pass

        elif ctype == "UNROLL":
            has_unroll = 1.0
            fac = params.get("factor")
            if fac is not None:
                try:
                    unroll_factor = float(fac)
                except Exception:
                    pass

        elif ctype == "ARRAY_PARTITION":
            has_partition = 1.0
            fac = params.get("factor")
            if fac is not None:
                try:
                    partition_factor = float(fac)
                except Exception:
                    pass

    return torch.tensor([[has_pipeline, target_ii, has_unroll, unroll_factor, has_partition, partition_factor]],
                        dtype=torch.float)  # [1,6]


# ============================
# 数据集实现
# ============================
class CryptoHLS_Dataset(Dataset):
    """
    读取 *.jsonl 清单，加载图（nodes/edges CSV），
    生成 Data(x, edge_index, y, 以及：
      - sol_type_idx/sol_key_idx/sol_val_idx（pragma 序列编码）
      - kernel_id（合训所需）
      - cond_vec（条件向量：estimated_period_ns + kernel one-hot，预留 8 维）
      - local_feats（最小对齐的局部池化特征，6 维）
    )
    """
    def __init__(self, root, transform=None, pre_transform=None):
        # 你现有的相对路径（保持一致，可按需替换为合并后的 mk_hls_dataset.jsonl）
        self.dataset_manifest_path = os.path.join("CRYPTOHLS_DATASET", "/home/CryptoHLS/aes_hls_dataset.jsonl")
        self.graph_data_dir = os.environ.get("CRYPTOHLS_GRAPH_DIR", "/home/CryptoHLS/build/build")
        print(self.dataset_manifest_path)

        if not os.path.exists(self.dataset_manifest_path):
            raise FileNotFoundError(f"Dataset manifest not found at: {self.dataset_manifest_path}")

        self.manifest = []
        with open(self.dataset_manifest_path, 'r', encoding='utf-8') as f:
            for line in f:
                if line.strip():
                    self.manifest.append(json.loads(line))

        # 用于标签缩放（log1p），与原版保持一致
        self.scaler = StandardScaler()

        super().__init__(root, transform, pre_transform)

        # 若还没有处理产物则执行一次处理
        if not self.processed_file_names:
            self.process()

    # ========== 缩放器：仅在训练划分上拟合 ==========
    def fit_scaler_on_split(self, indices):
        """
        在训练集上拟合 StandardScaler（log1p 之后）。
        放宽条件：只要求 latency > 0 且 lut > 0，interval 可以为 0。
        如果一个有效样本都没有，则在 dummy 数据上做一次 fit，避免 NotFittedError。
        """
        print("Fitting PPA scaler on training split...")
        all_ppa = []
        for i in indices:
            entry = self.manifest[i]
            real = entry.get("real_ppa", {})
            if not real:
                continue
            # 尽量兼容不同 key 命名
            lat = real.get("latency_avg", real.get("latency", real.get("latency_min", 0)))
            itv = real.get("interval_avg", real.get("interval", real.get("interval_min", 0)))
            lut = real.get("lut", 0)

            try:
                lat = float(lat)
                itv = float(itv)
                lut = float(lut)
            except Exception:
                continue

            # 允许 interval 为 0，但 latency 和 lut 必须是正的
            if lat <= 0 or lut <= 0:
                continue

            all_ppa.append([lat, itv, lut])

        if not all_ppa:
            print("Warning: No valid PPA data found for scaler. Using dummy scaler (identity-like).")
            # 防止 NotFittedError：在一个简单样本上做一次 fit
            import numpy as np
            dummy = np.array([[1.0, 0.0, 1.0]], dtype=np.float32)
            self.scaler.fit(dummy)
            return

        import numpy as np
        all_ppa_arr = np.array(all_ppa, dtype=np.float32)
        all_ppa_log = np.log1p(all_ppa_arr)
        self.scaler.fit(all_ppa_log)
        print("Scaler fitted successfully on log-transformed data.")

    @property

    def raw_file_names(self):
        # 仅用于 torch_geometric 的存在性检查
        return ['sha1_hls_dataset.jsonl']

    @property
    def _processed_ok(self):
        import os
        return all(os.path.exists(os.path.join(self.processed_dir, f'data_{i}.pt'))
                   for i in range(len(self.manifest)))

    def processed_file_names(self):
        return [f'data_{i}.pt' for i in range(len(self.manifest))]

    # ========== 主处理流程 ==========
    def process(self):
        print("Starting data processing with enhanced structural + aligned local features...")
        os.makedirs(self.processed_dir, exist_ok=True)
        processed_count = 0

        for idx, entry in enumerate(self.manifest):
            # CSV 路径
            nodes_rel = entry.get('graph_nodes_file')
            edges_rel = entry.get('graph_edges_file')
            if not nodes_rel or not edges_rel:
                print(f"Warning: Sample {idx} missing graph files.")
                continue

            nodes_path = os.path.join(self.graph_data_dir, nodes_rel)
            edges_path = os.path.join(self.graph_data_dir, edges_rel)

            try:
                nodes_df = pd.read_csv(nodes_path)
                edges_df = pd.read_csv(edges_path)
                if 'source' in edges_df.columns and 'target' in edges_df.columns:
                    edges_df['source'] = edges_df['source'].astype(int)
                    edges_df['target'] = edges_df['target'].astype(int)
            except Exception as e:
                print(f"Warning: Could not process sample {idx}: {e}")
                continue

            # 结构特征
            try:
                structural_feats = add_structural_features(nodes_df, edges_df)
            except Exception as e:
                print(f"Error calculating structural features for sample {idx}: {e}")
                structural_feats = np.zeros((len(nodes_df), 5), dtype=float)

            # 节点初始特征（第一列作为 opcode / type id）
            try:
                opcodes = nodes_df.iloc[:, 0].values.astype(float)
            except Exception:
                opcodes = np.zeros((len(nodes_df),), dtype=float)

            x = np.column_stack([opcodes, structural_feats])  # [N, 1+5]
            x = torch.tensor(x, dtype=torch.float)

            # 边
            if len(edges_df) > 0 and 'source' in edges_df.columns and 'target' in edges_df.columns:
                edge_index = torch.tensor(edges_df[['source', 'target']].values, dtype=torch.long).t().contiguous()
            else:
                # 无边则给一个空图（避免报错）
                edge_index = torch.empty((2, 0), dtype=torch.long)

            # pragma 序列编码
            sol = entry.get('solution', []) or []
            sol_type, sol_key, sol_val = encode_solution_with_padding(sol)
            sol_type_idx = torch.tensor(sol_type, dtype=torch.long).unsqueeze(0)  # [1, L]
            sol_key_idx  = torch.tensor(sol_key,  dtype=torch.long).unsqueeze(0)  # [1, L]
            sol_val_idx  = torch.tensor(sol_val,  dtype=torch.long).unsqueeze(0)  # [1, L]

            # 目标标签（与原版字段保持一致）
            ppa = entry.get('real_ppa', {}) or {}
            latency = float(ppa.get('latency_avg', 0.0))
            interval = float(ppa.get('interval_avg', 0.0))
            lut = float(ppa.get('lut', 0.0))
            y = torch.tensor([[latency, interval, lut]], dtype=torch.float)  # [1,3]

            # kernel_id
            kname = infer_kernel_name(entry)
            kid = kernel_to_id(kname)
            kernel_id = torch.tensor([kid], dtype=torch.long)  # [1]

            # 条件向量 cond_vec（预留8维：0位 est_period_ns，1/2位 kernel one-hot，其余留0）
            estp = 0.0
            try:
                estp = float(ppa.get('estimated_period_ns', entry.get('estimated_period_ns', 0.0)) or 0.0)
            except Exception:
                estp = 0.0
            cond_vec = torch.zeros((1, 8), dtype=torch.float)
            cond_vec[0, 0] = estp
            # kernel one-hot at [1],[2]
            if kid == KERNEL_VOCAB["aes"]:
                cond_vec[0, 1] = 1.0
            elif kid == KERNEL_VOCAB["sha1"]:
                cond_vec[0, 2] = 1.0

            # 最小对齐的局部池化特征（6维）
            local_feats = build_local_feats_from_solution(sol)  # [1,6]

            data = Data(
                x=x,
                edge_index=edge_index,
                y=y,
                sol_type_idx=sol_type_idx,
                sol_key_idx=sol_key_idx,
                sol_val_idx=sol_val_idx,
                kernel_id=kernel_id,
                cond_vec=cond_vec,
                local_feats=local_feats
            )

            torch.save(data, os.path.join(self.processed_dir, f'data_{idx}.pt'))
            processed_count += 1

        print(f"Data processing finished. Successfully processed {processed_count} / {len(self.manifest)} samples.")

    # ========== Dataset 读接口 ==========
    def len(self):
        return len(self.manifest)

    def get(self, idx):
        data = torch.load(os.path.join(self.processed_dir, f'data_{idx}.pt'), weights_only=False)
        return data
