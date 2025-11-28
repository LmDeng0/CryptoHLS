// #ifndef CRYPTOHLS_IRANALYZER_H
// #define CRYPTOHLS_IRANALYZER_H

// #include "Core.h"
// #include <string>

// class IRAnalyzer {
// public:
//     static FunctionCost analyzeFile(const std::string& ir_file_path, const std::string& top_func_name);
// };

// #endif // CRYPTOHLS_IRANALYZER_H



#ifndef CRYPTOHLS_IR_ANALYZER_H
#define CRYPTOHLS_IR_ANALYZER_H

#include "Core.h"
#include <string>
#include <memory>

// 前向声明LLVM的核心类
namespace llvm {
    class Module;
}

class IRAnalyzer {
public:
    // [关键修改] 静态方法现在接收一个已经解析好的LLVM Module
    static FunctionCost analyzeModule(llvm::Module* module, const std::string& topFunctionName);
};

#endif // CRYPTOHLS_IR_ANALYZER_H