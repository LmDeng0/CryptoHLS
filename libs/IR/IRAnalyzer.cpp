// #include "IRAnalyzer.h"
// #include "llvm/IR/Module.h"
// #include "llvm/IR/Function.h"
// #include "llvm/IR/Instruction.h"
// #include "llvm/IRReader/IRReader.h"
// #include "llvm/Support/SourceMgr.h"
// #include "llvm/Support/raw_ostream.h"
// #include <cxxabi.h>

// // Demangle function
// std::string demangle(const char* mangled_name) {
//     int status = 0;
//     char* demangled = abi::__cxa_demangle(mangled_name, nullptr, nullptr, &status);
//     if (status == 0 && demangled) {
//         std::string result(demangled);
//         std::free(demangled);
//         size_t pos = result.find('(');
//         if (pos != std::string::npos) return result.substr(0, pos);
//         return result;
//     }
//     return std::string(mangled_name);
// }

// FunctionCost IRAnalyzer::analyzeFile(const std::string& ir_file_path, const std::string& top_func_name) {
//     llvm::LLVMContext Context;
//     llvm::SMDiagnostic Err;
//     std::unique_ptr<llvm::Module> TheModule = llvm::parseIRFile(ir_file_path, Err, Context);
    
//     if (!TheModule) {
//         llvm::errs() << "Error: Failed to parse IR file: " << ir_file_path << "\n";
//         Err.print("analysis-tool", llvm::errs());
//         return {};
//     }

//     llvm::Function* TopFunc = nullptr;
//     for (auto &F : *TheModule) {
//         if (demangle(F.getName().str().c_str()) == top_func_name) {
//             TopFunc = &F;
//             break;
//         }
//     }
//     if (!TopFunc) {
//         llvm::errs() << "Error: Top function '" << top_func_name << "' not found in IR.\n";
//         return {};
//     }

//     FunctionCost cost;
//     for (auto &BB : *TopFunc) { for (auto &I : BB) {
//         std::string op = I.getOpcodeName();
//         if (op=="add"||op=="sub"||op=="fadd"||op=="fsub") { cost.area_score += 1; cost.performance_score += 1;}
//         else if (op=="mul"||op=="fmul") { cost.area_score += 10; cost.performance_score += 5;}
//         else if (op=="sdiv"||op=="udiv"||op=="fdiv") { cost.area_score += 20; cost.performance_score += 10;}
//         else if (op=="load"||op=="store"||op=="alloca"||op=="getelementptr") { cost.area_score += 2; cost.performance_score += 3;}
//     }}
//     return cost;
// }


#include "IRAnalyzer.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"
#include "llvm/Support/raw_ostream.h"
#include <cxxabi.h> // 引入C++ ABI库用于名称解修饰

// 辅助函数，用于解修饰C++函数名
static std::string demangle(const std::string& mangled_name) {
    int status = 0;
    char* demangled = abi::__cxa_demangle(mangled_name.c_str(), nullptr, nullptr, &status);
    if (status == 0) {
        std::string result(demangled);
        free(demangled);
        // 我们只需要函数名本身，去掉括号和参数
        size_t paren_pos = result.find('(');
        if (paren_pos!= std::string::npos) {
            return result.substr(0, paren_pos);
        }
        return result;
    }
    return mangled_name; // 如果解修饰失败，返回原始名称
}

FunctionCost IRAnalyzer::analyzeModule(llvm::Module* module, const std::string& topFunctionName) {
    FunctionCost cost = {0, 0};

    if (!module) {
        return cost;
    }

    // [关键修正] 遍历模块中的所有函数
    for (const llvm::Function& F : *module) {
        if (F.isDeclaration()) continue; // 跳过函数声明

        std::string mangledName = F.getName().str();
        std::string demangledName = demangle(mangledName);

        // 与用户指定的顶层函数名进行比较
        if (demangledName == topFunctionName) {
            int instruction_count = 0;
            for (const llvm::BasicBlock& B : F) {
                instruction_count += B.size(); // 使用.size() 是更安全、更推荐的方式
            }
            
            // 使用一个更合理的启发式规则
            cost.performance_score = instruction_count;
            cost.area_score = instruction_count / 2;
            
            // 找到后即可返回
            return cost;
        }
    }

    llvm::errs() << "Warning: Top function '" << topFunctionName << "' not found in LLVM IR module.\n";
    return cost; // 如果没找到，返回零成本
}