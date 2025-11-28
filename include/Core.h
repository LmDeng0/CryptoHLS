// #ifndef CRYPTOHLS_CORE_H
// #define CRYPTOHLS_CORE_H

// #include <string>
// #include <vector>
// #include <map>

// // 1. 核心数据结构
// struct PragmaCommand {
//     unsigned int line;
//     std::string type;
//     std::map<std::string, std::string> params;
// };
// using Solution = std::vector<PragmaCommand>;

// struct OptimizationOption {
//     std::string type;
//     std::map<std::string, std::vector<std::string>> param_ranges;
// };

// struct OptimizationPoint {
//     std::string node_type;
//     unsigned int line;
//     std::string name;
//     std::vector<OptimizationOption> options;
// };
// using DesignSpace = std::vector<OptimizationPoint>;

// struct FunctionCost {
//     double area_score = 0.0;
//     double performance_score = 0.0;
// };

// #endif // CRYPTOHLS_CORE_H


// #ifndef CRYPTOHLS_CORE_H
// #define CRYPTOHLS_CORE_H

// #include <vector>
// #include <map>
// #include <string>

// // 优化选项
// struct OptimizationOption {
//     std::string type;  // 如 "PIPELINE", "UNROLL" 等
//     std::map<std::string, std::vector<std::string>> param_ranges; // 参数及其可选值
// };

// // 优化点
// struct OptimizationPoint {
//     std::string node_type;  // 节点类型，如"ForLoop", "Array"等
//     unsigned line;          // 源代码行号
//     std::string name;       // 节点名称（如变量名、函数名等）
//     std::string function;   // 所属函数
//     std::string type;       // 优化类型
//     std::vector<OptimizationOption> options; // 可用的优化选项
// };

// // 设计命令（最终选择的优化方案中的一项）
// struct PragmaCommand {
//     unsigned line;          // 源代码行号
//     std::string type;       // 如 "PIPELINE", "UNROLL" 等
//     std::string function;   // 所属函数 - 新增成员
//     std::map<std::string, std::string> params; // 参数键值对
// };

// using DesignSpace = std::vector<OptimizationPoint>;
// using Solution = std::vector<PragmaCommand>;

// // 函数成本模型
// struct FunctionCost {
//     double performance_score = 0.0;  // 性能得分（越高越好）
//     double area_score = 0.0;         // 面积得分（越低越好）
// };

// #endif // CRYPTOHLS_CORE_H



#ifndef CRYPTOHLS_CORE_H
#define CRYPTOHLS_CORE_H

#include <vector>
#include <map>
#include <string>

// 优化选项
struct OptimizationOption {
    std::string type;   // 如 "PIPELINE", "UNROLL" 等
    std::map<std::string, std::vector<std::string>> param_ranges; // 参数及其可选值
};

// 优化点
struct OptimizationPoint {
    std::string node_type;  // 节点类型，如"ForLoop", "Array"等
    unsigned line;          // 源代码行号
    std::string name;       // 节点名称（如变量名、函数名等）
    std::string function;   // 所属函数
    std::string type;       // 优化类型
    std::vector<OptimizationOption> options; // 可用的优化选项
};

// 设计命令（最终选择的优化方案中的一项）
struct PragmaCommand {
    unsigned line;          // 源代码行号
    std::string type;       // 如 "PIPELINE", "UNROLL" 等
    std::string name;       // <-- 新增成员：用于记录指令作用的对象名（如变量名）
    std::string function;   // 所属函数
    std::map<std::string, std::string> params; // 参数键值对

    // 增加一个辅助函数，方便判断
    bool empty() const {
        return type.empty();
    }
};

using DesignSpace = std::vector<OptimizationPoint>;
using Solution = std::vector<PragmaCommand>;

// 函数成本模型
struct FunctionCost {
    double performance_score = 0.0;  // 性能得分（越低越好）
    double area_score = 0.0;         // 面积得分（越低越好）
};

#endif // CRYPTOHLS_CORE_H
