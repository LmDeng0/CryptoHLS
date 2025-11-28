# import torch
# import torch.nn.functional as F
# from torch.nn import Linear, Sequential, ReLU, Embedding, Dropout
# from torch_geometric.nn import SAGEConv, global_mean_pool
# from torch_geometric.loader import DataLoader
# from gnn_dataset import CryptoHLS_Dataset, VOCAB_SIZE
# import numpy as np
# import os

# # --- 模型定义部分保持不变 ---
# class GNN_PPA_Predictor(torch.nn.Module):
#     def __init__(self, num_node_types, embedding_dim, hidden_channels, solution_feature_dim, dropout_rate=0.5):
#         super(GNN_PPA_Predictor, self).__init__()
#         torch.manual_seed(42)
#         self.node_emb = Embedding(num_node_types, embedding_dim)
#         self.conv1 = SAGEConv(embedding_dim, hidden_channels)
#         self.conv2 = SAGEConv(hidden_channels, hidden_channels)
#         self.mlp = Sequential(
#             Linear(hidden_channels + solution_feature_dim, hidden_channels),
#             ReLU(),
#             Dropout(p=dropout_rate),
#             Linear(hidden_channels, hidden_channels // 2),
#             ReLU(),
#             Dropout(p=dropout_rate),
#             Linear(hidden_channels // 2, 2)
#         )
#     def forward(self, data):
#         x, edge_index, batch, solution_feature = data.x, data.edge_index, data.batch, data.solution_feature
#         x = self.node_emb(x.squeeze(-1))
#         x = self.conv1(x, edge_index).relu()
#         x = self.conv2(x, edge_index).relu()
#         graph_embedding = global_mean_pool(x, batch)
#         combined_features = torch.cat([graph_embedding, solution_feature.view(data.num_graphs, -1)], dim=1)
#         output = self.mlp(combined_features)
#         return output

# # --- 数据准备部分保持不变 ---
# print("Loading dataset...")
# dataset = CryptoHLS_Dataset(root='.')
# scaler = dataset.scaler 
# print("Calculating number of node types...")
# num_opcodes = 0
# for data_point in dataset:
#     max_opcode_in_sample = data_point.x.max().item()
#     if max_opcode_in_sample > num_opcodes:
#         num_opcodes = max_opcode_in_sample
# num_total_node_types = num_opcodes + 1 
# print(f"Detected {num_total_node_types} unique node types (opcodes).")
# torch.manual_seed(42)
# dataset = dataset.shuffle()
# train_size = int(0.8 * len(dataset))
# val_size = len(dataset) - train_size
# train_dataset, val_dataset = torch.utils.data.random_split(dataset, [train_size, val_size])
# print(f"Dataset split: {len(train_dataset)} training samples, {len(val_dataset)} validation samples.")
# train_loader = DataLoader(train_dataset, batch_size=32, shuffle=True)
# val_loader = DataLoader(val_dataset, batch_size=32, shuffle=False)

# # --- 训练和验证循环 (修改超参数) ---
# device = torch.device('cuda' if torch.cuda.is_available() else 'cpu')
# print(f"Using device: {device}")

# model = GNN_PPA_Predictor(
#     num_node_types=num_total_node_types, 
#     embedding_dim=32,
#     hidden_channels=128, 
#     solution_feature_dim=VOCAB_SIZE,
#     dropout_rate=0.25 # [核心修改] 降低Dropout率
# ).to(device)

# # [核心修改] 降低学习率
# optimizer = torch.optim.Adam(model.parameters(), lr=0.0005) 
# loss_fn = torch.nn.MSELoss()

# # 训练和验证函数 train() 和 validate() 保持不变
# def train():
#     model.train()
#     total_loss = 0
#     for data in train_loader:
#         data = data.to(device)
#         optimizer.zero_grad()
#         out = model(data)
#         loss = loss_fn(out, data.y)
#         loss.backward()
#         optimizer.step()
#         total_loss += loss.item() * data.num_graphs
#     return total_loss / len(train_loader.dataset)

# def validate():
#     model.eval()
#     all_preds, all_targets = [], []
#     with torch.no_grad():
#         for data in val_loader:
#             data = data.to(device)
#             out = model(data)
#             all_preds.append(out.cpu().numpy())
#             all_targets.append(data.y.cpu().numpy())
#     all_preds = np.concatenate(all_preds)
#     all_targets = np.concatenate(all_targets)
#     preds_original_scale = scaler.inverse_transform(all_preds)
#     targets_original_scale = scaler.inverse_transform(all_targets)
#     epsilon = 1e-8
#     mape = 100 * np.mean(np.abs((preds_original_scale - targets_original_scale) / (targets_original_scale + epsilon)))
#     return mape

# # --- 开始训练！ ---
# print("\n--- Starting Final Tuning Training ---")
# # [核心修改] 增加训练周期
# for epoch in range(1, 301): 
#     train_loss = train()
#     val_mape = validate()
#     if epoch % 10 == 0:
#         print(f'Epoch: {epoch:03d}, Training Loss (MSE): {train_loss:.4f}, Validation Error (MAPE): {val_mape:.2f}%')

# print("--- Training Finished ---")
# torch.save(model.state_dict(), 'ppa_predictor_model_tuned.pth')
# print(f"\nModel saved to ppa_predictor_model_tuned.pth")


# import torch
# import torch.nn.functional as F
# from torch.nn import Linear, Sequential, ReLU, Embedding, Dropout
# from torch_geometric.nn import SAGEConv, global_mean_pool
# from torch_geometric.loader import DataLoader
# from gnn_dataset import CryptoHLS_Dataset, VOCAB_SIZE
# import numpy as np
# import os

# # --- 模型定义部分保持不变 ---
# class GNN_PPA_Predictor(torch.nn.Module):
#     def __init__(self, num_node_types, embedding_dim, hidden_channels, solution_feature_dim, dropout_rate=0.5):
#         super(GNN_PPA_Predictor, self).__init__()
#         # ... (内部逻辑不变) ...
#         torch.manual_seed(42)
#         self.node_emb = Embedding(num_node_types, embedding_dim)
#         self.conv1 = SAGEConv(embedding_dim, hidden_channels)
#         self.conv2 = SAGEConv(hidden_channels, hidden_channels)
#         self.mlp = Sequential(
#             Linear(hidden_channels + solution_feature_dim, hidden_channels), ReLU(), Dropout(p=dropout_rate),
#             Linear(hidden_channels, hidden_channels // 2), ReLU(), Dropout(p=dropout_rate),
#             Linear(hidden_channels // 2, 2)
#         )
#     def forward(self, data):
#         # ... (内部逻辑不变) ...
#         x, edge_index, batch, solution_feature = data.x, data.edge_index, data.batch, data.solution_feature
#         x = self.node_emb(x.squeeze(-1))
#         x = self.conv1(x, edge_index).relu()
#         x = self.conv2(x, edge_index).relu()
#         graph_embedding = global_mean_pool(x, batch)
#         combined_features = torch.cat([graph_embedding, solution_feature.view(data.num_graphs, -1)], dim=1)
#         output = self.mlp(combined_features)
#         return output

# # --- 准备训练 ---
# print("Loading dataset...")
# dataset = CryptoHLS_Dataset(root='.')

# print("Calculating number of node types...")
# num_opcodes = 0
# for data_point in dataset:
#     max_opcode_in_sample = data_point.x.max().item()
#     if max_opcode_in_sample > num_opcodes:
#         num_opcodes = max_opcode_in_sample
# num_total_node_types = num_opcodes + 1 
# print(f"Detected {num_total_node_types} unique node types (opcodes).")

# torch.manual_seed(42)
# dataset = dataset.shuffle()
# train_size = int(0.8 * len(dataset))
# val_size = len(dataset) - train_size
# train_dataset, val_dataset = torch.utils.data.random_split(dataset, [train_size, val_size])
# print(f"Dataset split: {len(train_dataset)} training samples, {len(val_dataset)} validation samples.")

# # [核心修改] 在划分数据集后，只用训练集的索引来fit scaler
# dataset.fit_scaler_on_split(train_dataset.indices)
# scaler = dataset.scaler # 获取已经fit好的scaler

# train_loader = DataLoader(train_dataset, batch_size=32, shuffle=True)
# val_loader = DataLoader(val_dataset, batch_size=32, shuffle=False)

# # --- 训练和验证循环 ---
# device = torch.device('cuda' if torch.cuda.is_available() else 'cpu')
# print(f"Using device: {device}")

# model = GNN_PPA_Predictor(
#     num_node_types=num_total_node_types, embedding_dim=32,
#     hidden_channels=128, solution_feature_dim=VOCAB_SIZE,
#     dropout_rate=0.25
# ).to(device)
# optimizer = torch.optim.Adam(model.parameters(), lr=0.0005)
# loss_fn = torch.nn.MSELoss()

# def train():
#     model.train()
#     total_loss = 0
#     for data in train_loader:
#         data = data.to(device)
#         optimizer.zero_grad()
#         out = model(data)
        
#         # [核心修改] 在计算损失前，动态地对标签进行归一化
#         y_scaled = torch.from_numpy(scaler.transform(data.y.cpu().numpy())).to(device)
        
#         loss = loss_fn(out, y_scaled)
#         loss.backward()
#         optimizer.step()
#         total_loss += loss.item() * data.num_graphs
#     return total_loss / len(train_loader.dataset)

# def validate():
#     model.eval()
#     all_preds, all_targets = [], []
#     with torch.no_grad():
#         for data in val_loader:
#             data = data.to(device)
#             out = model(data)
#             all_preds.append(out.cpu().numpy())
#             # 注意：data.y现在是原始值，所以我们不需要在这里做任何改变
#             all_targets.append(data.y.cpu().numpy())
            
#     all_preds = np.concatenate(all_preds)
#     all_targets = np.concatenate(all_targets)
    
#     preds_original_scale = scaler.inverse_transform(all_preds)
#     targets_original_scale = all_targets # 它们已经是原始尺度了
    
#     epsilon = 1e-8
#     mape = 100 * np.mean(np.abs((preds_original_scale - targets_original_scale) / (targets_original_scale + epsilon)))
#     return mape

# # --- 开始训练！ ---
# print("\n--- Starting Training with Correct Scaling ---")
# for epoch in range(1, 101): # 先训练100个周期观察效果
#     train_loss = train()
#     val_mape = validate()
#     if epoch % 10 == 0:
#         print(f'Epoch: {epoch:03d}, Training Loss (MSE): {train_loss:.4f}, Validation Error (MAPE): {val_mape:.2f}%')

# print("--- Training Finished ---")
# torch.save(model.state_dict(), 'ppa_predictor_model_final.pth')
# print(f"\nModel saved to ppa_predictor_model_final.pth")



import torch
import torch.nn.functional as F
from torch.nn import Linear, Sequential, ReLU, Embedding, Dropout
from torch_geometric.nn import SAGEConv, global_mean_pool
from torch_geometric.loader import DataLoader
from gnn_dataset import CryptoHLS_Dataset, VOCAB_SIZE
import numpy as np
import os
import copy

# --- 模型定义部分保持不变 ---
class GNN_PPA_Predictor(torch.nn.Module):
    def __init__(self, num_node_types, embedding_dim, hidden_channels, solution_feature_dim, dropout_rate=0.5):
        super(GNN_PPA_Predictor, self).__init__()
        torch.manual_seed(42)
        self.node_emb = Embedding(num_node_types, embedding_dim)
        self.conv1 = SAGEConv(embedding_dim, hidden_channels)
        self.conv2 = SAGEConv(hidden_channels, hidden_channels)
        self.mlp = Sequential(
            Linear(hidden_channels + solution_feature_dim, hidden_channels), ReLU(), Dropout(p=dropout_rate),
            Linear(hidden_channels, hidden_channels // 2), ReLU(), Dropout(p=dropout_rate),
            Linear(hidden_channels // 2, 2)
        )
    def forward(self, data):
        x, edge_index, batch, solution_feature = data.x, data.edge_index, data.batch, data.solution_feature
        x = self.node_emb(x.squeeze(-1))
        x = self.conv1(x, edge_index).relu()
        x = self.conv2(x, edge_index).relu()
        graph_embedding = global_mean_pool(x, batch)
        combined_features = torch.cat([graph_embedding, solution_feature.view(data.num_graphs, -1)], dim=1)
        # 模型输出的是对数尺度归一化后的值
        output = self.mlp(combined_features)
        return output

# --- 数据准备部分保持不变 ---
print("Loading dataset...")
dataset = CryptoHLS_Dataset(root='.')

print("Calculating number of node types...")
num_opcodes = 0
# 使用 dataset.len() 和 dataset.get() 更高效
for i in range(len(dataset)):
    data_point = dataset.get(i)
    max_opcode_in_sample = data_point.x.max().item()
    if max_opcode_in_sample > num_opcodes:
        num_opcodes = max_opcode_in_sample
num_total_node_types = num_opcodes + 1 
print(f"Detected {num_total_node_types} unique node types (opcodes).")

torch.manual_seed(42)
# dataset.shuffle() returns a shuffled dataset, so we re-assign it
dataset = dataset.shuffle()
train_size = int(0.8 * len(dataset))
val_size = len(dataset) - train_size
train_dataset, val_dataset = torch.utils.data.random_split(dataset, [train_size, val_size])
print(f"Dataset split: {len(train_dataset)} training samples, {len(val_dataset)} validation samples.")

dataset.fit_scaler_on_split(train_dataset.indices)
scaler = dataset.scaler

train_loader = DataLoader(train_dataset, batch_size=32, shuffle=True)
val_loader = DataLoader(val_dataset, batch_size=32, shuffle=False)

# --- 训练和验证循环 ---
device = torch.device('cuda' if torch.cuda.is_available() else 'cpu')
print(f"Using device: {device}")

model = GNN_PPA_Predictor(
    num_node_types=num_total_node_types, embedding_dim=32,
    hidden_channels=128, solution_feature_dim=VOCAB_SIZE,
    dropout_rate=0.25
).to(device)

optimizer = torch.optim.Adam(model.parameters(), lr=0.0001)
# --- [核心修改 2] ---
# 使用 L1Loss (MAE)，它对异常值不那么敏感
loss_fn = torch.nn.L1Loss()

def train():
    model.train()
    total_loss = 0
    for data in train_loader:
        data = data.to(device)
        optimizer.zero_grad()
        out = model(data)
        
        # --- [核心修改 3] ---
        # 1. 对标签进行对数变换
        y_log = np.log1p(data.y.cpu().numpy())
        # 2. 对变换后的值进行归一化
        y_scaled = torch.from_numpy(scaler.transform(y_log)).to(device, dtype=torch.float)
        
        loss = loss_fn(out, y_scaled)
        loss.backward()
        optimizer.step()
        total_loss += loss.item() * data.num_graphs
    return total_loss / len(train_loader.dataset)

def validate():
    model.eval()
    all_preds_scaled, all_targets = [], []
    with torch.no_grad():
        for data in val_loader:
            data = data.to(device)
            out = model(data)
            all_preds_scaled.append(out.cpu().numpy())
            all_targets.append(data.y.cpu().numpy())
            
    all_preds_scaled = np.concatenate(all_preds_scaled)
    all_targets = np.concatenate(all_targets)
    
    # --- [核心修改 4] ---
    # 1. 对预测值进行反归一化，得到对数尺度的预测
    preds_log_scale = scaler.inverse_transform(all_preds_scaled)
    # 2. 对对数尺度的预测进行指数还原，得到原始尺度的预测
    preds_original_scale = np.expm1(preds_log_scale)

    targets_original_scale = all_targets
    
    epsilon = 1e-8
    # 确保预测值不为负，避免计算错误
    preds_original_scale[preds_original_scale < 0] = 0
    mape = 100 * np.mean(np.abs((preds_original_scale - targets_original_scale) / (targets_original_scale + epsilon)))
    return mape

# --- 训练循环 (使用我们之前实现的Early Stopping) ---
print("\n--- Starting Training with L1Loss and Log-Transform ---")
best_val_mape = float('inf')
best_model_state = None
patience = 30 # 如果验证误差连续30个周期没有改善，就停止
patience_counter = 0

for epoch in range(1, 301): # 总共训练最多300个周期
    train_loss = train()
    val_mape = validate()
    
    if epoch % 10 == 0:
        print(f'Epoch: {epoch:03d}, Training Loss (Scaled L1): {train_loss:.6f}, Validation Error (MAPE): {val_mape:.2f}%')

    if val_mape < best_val_mape:
        best_val_mape = val_mape
        best_model_state = copy.deepcopy(model.state_dict())
        patience_counter = 0
        print(f'---> New best model found! MAPE: {val_mape:.2f}% at epoch {epoch}')
    else:
        patience_counter += 1
    
    if patience_counter >= patience:
        print(f'---! Early stopping triggered at epoch {epoch}. Best MAPE: {best_val_mape:.2f}% !---')
        break

print("--- Training Finished ---")
torch.save(best_model_state, 'ppa_predictor_model_best.pth')
print(f"\nBest model saved to ppa_predictor_model_best.pth with final Validation MAPE: {best_val_mape:.2f}%")
