// libs/GraphExtractor/GraphExtractor.h

#ifndef CRYPTOHLS_GRAPH_EXTRACTOR_H
#define CRYPTOHLS_GRAPH_EXTRACTOR_H

#include <string>
#include <vector>
#include <cstdint>
#include <memory>

// 前向声明LLVM的核心类
namespace llvm {
    class Module;
}

// 定义图节点的特征
// 在真实的GNN应用中，这可以扩展为一个更长的向量
struct NodeFeature {
    int64_t opcode;       // LLVM指令的操作码
    int64_t num_operands; // 操作数的数量
};

// 定义图的完整表示
struct GraphData {
    std::vector<NodeFeature> node_features;      // 节点特征矩阵
    std::vector<std::pair<int, int>> edge_index; // 边列表 (数据流图)
};

class GraphExtractor {
public:
    // 构造函数：接收一个已解析的LLVM Module
    GraphExtractor(llvm::Module* module);

    // 核心方法：为指定的函数提取图数据
    GraphData extract(const std::string& topFunctionName);

    // (可选) 辅助方法：将提取的图数据保存到文件，供Python使用
    void saveToFile(const GraphData& graph, const std::string& output_prefix);

private:
    llvm::Module* M;
};

#endif // CRYPTOHLS_GRAPH_EXTRACTOR_H