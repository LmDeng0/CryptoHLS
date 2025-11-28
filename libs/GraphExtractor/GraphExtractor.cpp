// libs/GraphExtractor/GraphExtractor.cpp

#include "GraphExtractor.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/raw_ostream.h"
#include <cxxabi.h>
#include <map>
#include <fstream>

using namespace llvm;

// 辅助函数：解修饰C++函数名
static std::string demangle(const std::string& mangled_name) {
    int status = 0;
    char* demangled = abi::__cxa_demangle(mangled_name.c_str(), nullptr, nullptr, &status);
    if (status == 0) {
        std::string result(demangled);
        free(demangled);
        size_t paren_pos = result.find('(');
        if (paren_pos != std::string::npos) {
            return result.substr(0, paren_pos);
        }
        return result;
    }
    return mangled_name;
}


GraphExtractor::GraphExtractor(llvm::Module* module) : M(module) {}

GraphData GraphExtractor::extract(const std::string& topFunctionName) {
    GraphData graph;
    Function* TargetFunc = nullptr;

    // 1. 找到目标顶层函数
    for (Function &F : *M) {
        if (demangle(F.getName().str()) == topFunctionName) {
            TargetFunc = &F;
            break;
        }
    }

    if (!TargetFunc) {
        errs() << "[GraphExtractor] ERROR: Top function '" << topFunctionName << "' not found.\n";
        return graph;
    }
    
    outs() << "[GraphExtractor] INFO: Starting graph extraction for function '" << topFunctionName << "'...\n";

    std::map<Instruction*, int> inst_to_id;
    int current_id = 0;

    // 2. 第一次遍历：为每个指令分配ID，并提取节点特征
    for (BasicBlock &BB : *TargetFunc) {
        for (Instruction &I : BB) {
            inst_to_id[&I] = current_id++;
            
            NodeFeature feature;
            feature.opcode = static_cast<int64_t>(I.getOpcode());
            feature.num_operands = I.getNumOperands();
            graph.node_features.push_back(feature);
        }
    }

    // 3. 第二次遍历：根据操作数依赖关系构建边列表 (数据流图)
    for (BasicBlock &BB : *TargetFunc) {
        for (Instruction &I : BB) {
            if (inst_to_id.find(&I) == inst_to_id.end()) continue;
            int sink_id = inst_to_id.at(&I);
            
            for (Value *Operand : I.operands()) {
                if (Instruction *SourceInst = dyn_cast<Instruction>(Operand)) {
                    if (inst_to_id.count(SourceInst)) {
                        int source_id = inst_to_id.at(SourceInst);
                        graph.edge_index.push_back({source_id, sink_id});
                    }
                }
            }
        }
    }

    outs() << "[GraphExtractor] INFO: Extracted a graph with " << graph.node_features.size() 
           << " nodes and " << graph.edge_index.size() << " edges.\n";

    return graph;
}

void GraphExtractor::saveToFile(const GraphData& graph, const std::string& output_prefix) {
    // 将节点特征保存为CSV
    std::ofstream node_file(output_prefix + "_nodes.csv");
    node_file << "opcode,num_operands\n";
    for (const auto& node : graph.node_features) {
        node_file << node.opcode << "," << node.num_operands << "\n";
    }
    node_file.close();

    // 将边列表保存为CSV
    std::ofstream edge_file(output_prefix + "_edges.csv");
    edge_file << "source,target\n";
    for (const auto& edge : graph.edge_index) {
        edge_file << edge.first << "," << edge.second << "\n";
    }
    edge_file.close();

    outs() << "[GraphExtractor] INFO: Saved graph data to " 
           << output_prefix << "_nodes.csv and " << output_prefix << "_edges.csv\n";
}