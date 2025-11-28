// #include "IRGenerator.h"
// #include "llvm/Support/FileSystem.h"
// #include "llvm/Support/Path.h"
// #include "llvm/Support/Program.h"
// #include "llvm/Support/raw_ostream.h"
// #include <cstdlib>
// #include <vector>
// #include <optional>

// using namespace llvm;

// namespace CryptoHLS {

// IRGenerator::IRGenerator(const std::string& sourceFile, 
//                          const std::string& outputDir,
//                          const std::vector<std::string>& extraArgs)
//     : sourceFile(sourceFile), outputDir(outputDir), extraArgs(extraArgs) {
    
//     if (!sys::fs::exists(outputDir)) {
//         if (std::error_code ec = sys::fs::create_directories(outputDir)) {
//             errs() << "Error creating output directory: " << ec.message() << "\n";
//         }
//     }
    
//     SmallString<128> irPath(outputDir);
//     sys::path::append(irPath, sys::path::filename(sourceFile));
//     sys::path::replace_extension(irPath, "ll");
//     this->irFilePath = std::string(irPath.str());
    
//     SmallString<128> bcPath(outputDir);
//     sys::path::append(bcPath, sys::path::filename(sourceFile));
//     sys::path::replace_extension(bcPath, "bc");
//     this->bcFilePath = std::string(bcPath.str());
// }

// bool IRGenerator::generate() {
//     if (!runClangCommand()) {
//         errs() << "Error: Failed to generate LLVM IR\n";
//         return false;
//     }
    
//     if (!runLLVMASCommand()) {
//         errs() << "Warning: Failed to generate LLVM bitcode\n";
//     }
    
//     return true;
// }

// bool IRGenerator::runClangCommand() {
//     auto clangPathOrErr = sys::findProgramByName("clang++");
//     if (!clangPathOrErr) {
//         errs() << "Error: clang++ not found in PATH\n";
//         return false;
//     }
//     std::string clangPath = *clangPathOrErr;
    
//     std::vector<StringRef> args;
//     args.push_back(clangPath);
    
//     // [关键修正] 智能地过滤和重构编译参数
//     for (size_t i = 0; i < extraArgs.size(); ++i) {
//         StringRef arg = extraArgs[i];
//         // 跳过原始命令中的编译器本身、输入/输出文件和-c标志
//         if (i == 0 || arg == sourceFile || arg == "-c") {
//             continue;
//         }
//         if (arg == "-o") {
//             // 跳过 -o 和它后面的参数
//             i++;
//             continue;
//         }
//         args.push_back(arg);
//     }

//     // 添加我们自己的、用于生成IR的核心参数
//     args.push_back("-S");
//     args.push_back("-emit-llvm");
//     args.push_back("-o");
//     args.push_back(irFilePath);
//     args.push_back("-O2"); // 保留优化级别
//     args.push_back("-fno-slp-vectorize"); // 保留领域特定的优化禁用
//     args.push_back("-g"); // 保留调试信息
//     args.push_back(sourceFile); // 最后添加源文件
    
//     errs() << " Running IR Generation Command: ";
//     for (const auto& arg : args) {
//         errs() << arg << " ";
//     }
//     errs() << "\n";
    
//     std::string errMsg;
//     int result = sys::ExecuteAndWait(clangPath, args, std::nullopt, {}, 0, 0, &errMsg);
    
//     if (result!= 0) {
//         errs() << "Error running clang++: " << errMsg << "\n";
//         return false;
//     }
    
//     return true;
// }

// bool IRGenerator::runLLVMASCommand() {
//     auto llvmAsPathOrErr = sys::findProgramByName("llvm-as");
//     if (!llvmAsPathOrErr) {
//         errs() << "Warning: llvm-as not found in PATH\n";
//         return false;
//     }
//     std::string llvmAsPath = *llvmAsPathOrErr;
    
//     std::vector<StringRef> args = { llvmAsPath, irFilePath, "-o", bcFilePath };
    
//     std::string errMsg;
//     int result = sys::ExecuteAndWait(llvmAsPath, args, std::nullopt, {}, 0, 0, &errMsg);
    
//     if (result!= 0) {
//         errs() << "Error running llvm-as: " << errMsg << "\n";
//         return false;
//     }
    
//     return true;
// }

// std::string IRGenerator::getIRFilePath() const {
//     return irFilePath;
// }

// std::string IRGenerator::getBitcodeFilePath() const {
//     return bcFilePath;
// }

// } // namespace CryptoHLS



// #include "IRGenerator.h"
// #include "llvm/Support/FileSystem.h"
// #include "llvm/Support/Path.h"
// #include "llvm/Support/Program.h"
// #include "llvm/Support/raw_ostream.h"
// #include <cstdlib>
// #include <vector>
// #include <optional>

// using namespace llvm;

// namespace CryptoHLS {

// IRGenerator::IRGenerator(const std::string& sourceFile, 
//                          const std::string& outputDir,
//                          const std::vector<std::string>& extraArgs)
//     : sourceFile(sourceFile), outputDir(outputDir), extraArgs(extraArgs) {
    
//     if (!sys::fs::exists(outputDir)) {
//         if (std::error_code ec = sys::fs::create_directories(outputDir)) {
//             errs() << "Error creating output directory: " << ec.message() << "\n";
//         }
//     }
    
//     // [修正] 明确指定SmallString的模板参数大小，以解决之前的编译问题
//     SmallString<128> irPath(outputDir);
//     sys::path::append(irPath, sys::path::filename(sourceFile));
//     sys::path::replace_extension(irPath, "ll");
//     this->irFilePath = std::string(irPath.str());
    
//     SmallString<128> bcPath(outputDir);
//     sys::path::append(bcPath, sys::path::filename(sourceFile));
//     sys::path::replace_extension(bcPath, "bc");
//     this->bcFilePath = std::string(bcPath.str());
// }

// bool IRGenerator::generate() {
//     if (!runClangCommand()) {
//         errs() << "Error: Failed to generate LLVM IR\n";
//         return false;
//     }
    
//     if (!runLLVMASCommand()) {
//         errs() << "Warning: Failed to generate LLVM bitcode\n";
//     }
    
//     return true;
// }

// bool IRGenerator::runClangCommand() {
//     auto clangPathOrErr = sys::findProgramByName("clang++");
//     if (!clangPathOrErr) {
//         errs() << "Error: clang++ not found in PATH\n";
//         return false;
//     }
//     std::string clangPath = *clangPathOrErr;
    
//     std::vector<StringRef> args;
//     args.push_back(clangPath);
    
//     // [关键修正] 智能地过滤和重构编译参数
//     bool seenDashDash = false;
//     for (size_t i = 0; i < extraArgs.size(); ++i) {
//         StringRef arg = extraArgs[i];

//         if (seenDashDash) continue; // 忽略 "--" 之后的所有内容

//         // 第一个参数是编译器，跳过
//         if (i == 0) continue;

//         // 过滤掉定义编译动作或输出的标志
//         if (arg == "-c" || arg == sourceFile) {
//             continue;
//         }
//         if (arg == "-o") {
//             // 跳过 "-o" 和它后面的参数
//             i++;
//             continue;
//         }
//         if (arg == "--") {
//             seenDashDash = true;
//             continue;
//         }

//         // 保留所有其他参数 (例如 -I, -D, -std, 等)
//         args.push_back(arg);
//     }

//     // 添加我们自己的、用于生成IR的核心参数
//     args.push_back("-S");
//     args.push_back("-emit-llvm");
//     args.push_back("-o");
//     args.push_back(irFilePath);
//     args.push_back("-O2"); // 保留优化级别
//     args.push_back("-fno-slp-vectorize"); // 保留领域特定的优化禁用
//     args.push_back("-g"); // 保留调试信息
//     args.push_back(sourceFile); // 最后添加源文件
    
//     errs() << " Running IR Generation Command: ";
//     for (const auto& arg : args) {
//         errs() << arg << " ";
//     }
//     errs() << "\n";
    
//     std::string errMsg;
//     int result = sys::ExecuteAndWait(clangPath, args, std::nullopt, {}, 0, 0, &errMsg);
    
//     if (result!= 0) {
//         errs() << "Error running clang++: " << errMsg << "\n";
//         return false;
//     }
    
//     return true;
// }

// bool IRGenerator::runLLVMASCommand() {
//     auto llvmAsPathOrErr = sys::findProgramByName("llvm-as");
//     if (!llvmAsPathOrErr) {
//         errs() << "Warning: llvm-as not found in PATH\n";
//         return false;
//     }
//     std::string llvmAsPath = *llvmAsPathOrErr;
    
//     std::vector<StringRef> args = { llvmAsPath, irFilePath, "-o", bcFilePath };
    
//     std::string errMsg;
//     int result = sys::ExecuteAndWait(llvmAsPath, args, std::nullopt, {}, 0, 0, &errMsg);
    
//     if (result!= 0) {
//         errs() << "Error running llvm-as: " << errMsg << "\n";
//         return false;
//     }
    
//     return true;
// }

// std::string IRGenerator::getIRFilePath() const {
//     return irFilePath;
// }

// std::string IRGenerator::getBitcodeFilePath() const {
//     return bcFilePath;
// }

// } // namespace CryptoHLS


// // IRGenerator.cpp

// #include "IRGenerator.h"
// #include "llvm/Support/FileSystem.h"
// #include "llvm/Support/Path.h"
// #include "llvm/Support/Program.h"
// #include "llvm/Support/raw_ostream.h"

// #include <vector>

// using namespace llvm;

// namespace CryptoHLS {

// // [重构] 构造函数接口已更新
// IRGenerator::IRGenerator(const std::vector<std::string>& originalCommand,
//                          const std::string& sourceFile,
//                          const std::string& outputDir)
//     : originalCommand(originalCommand), sourceFile(sourceFile), outputDir(outputDir) {

//     // 创建输出目录的逻辑保持不变
//     if (!sys::fs::exists(outputDir)) {
//         if (std::error_code ec = sys::fs::create_directories(outputDir)) {
//             errs() << "Error creating output directory: " << ec.message() << "\n";
//         }
//     }
    
//     // 生成输出文件路径的逻辑保持不变
//     SmallString<128> irPath(outputDir);
//     sys::path::append(irPath, sys::path::filename(sourceFile));
//     sys::path::replace_extension(irPath, "ll");
//     this->irFilePath = std::string(irPath.str());
    
//     SmallString<128> bcPath(outputDir);
//     sys::path::append(bcPath, sys::path::filename(sourceFile));
//     sys::path::replace_extension(bcPath, "bc");
//     this->bcFilePath = std::string(bcPath.str());
// }

// // generate() 的逻辑保持不变，它只是一个协调者
// bool IRGenerator::generate() {
//     if (!runClangCommand()) {
//         errs() << "Error: Failed to generate LLVM IR\n";
//         return false;
//     }
    
//     if (!runLLVMASCommand()) {
//         errs() << "Warning: Failed to generate LLVM bitcode\n";
//     }
    
//     return true;
// }

// // [重构] 这是本次修改的核心
// bool IRGenerator::runClangCommand() {
//     // 1. 从原始指令中获取编译器路径，而不是从系统中搜索
//     //    这保证了我们使用的是与项目构建时完全相同的编译器
//     if (originalCommand.empty()) {
//         errs() << "Error: Received an empty compile command.\n";
//         return false;
//     }
//     StringRef compilerPath = originalCommand[0];

//     // 2. 基于原始指令，构建新的、用于生成IR的指令列表
//     std::vector<std::string> newArgs;
//     newArgs.push_back(std::string(compilerPath)); // 第一个参数是编译器本身

//     // 遍历原始指令，并过滤掉冲突的参数
//     for (size_t i = 1; i < originalCommand.size(); ++i) {
//         StringRef arg = originalCommand[i];

//         // 过滤掉 "-c" (只编译不链接) 和 "-o" (输出文件)
//         if (arg == "-c") {
//             continue;
//         }
//         if (arg == "-o") {
//             i++; // 跳过 "-o" 和它后面的文件名
//             continue;
//         }
        
//         // 保留所有其他有用的参数，如 -I, -D, -std, -isystem 等
//         newArgs.push_back(std::string(arg));
//     }

//     // 3. 添加我们自己用于生成IR的核心参数
//     newArgs.push_back("-S");             // 生成文本格式的汇编/IR
//     newArgs.push_back("-emit-llvm");     // 明确指出汇编就是LLVM IR
//     newArgs.push_back("-o");             // 指定输出文件
//     newArgs.push_back(this->irFilePath); // 输出到我们计算好的路径

//     // 将 std::vector<std::string> 转换为 llvm::sys::ExecuteAndWait 需要的格式
//     std::vector<StringRef> finalArgs;
//     for (const auto& s : newArgs) {
//         finalArgs.push_back(s);
//     }

//     // 打印最终执行的命令，用于调试
//     errs() << "Running Modified IR Generation Command: ";
//     for (const auto& arg : finalArgs) {
//         errs() << arg << " ";
//     }
//     errs() << "\n";
    
//     // 4. 执行命令
//     std::string errMsg;
//     int result = sys::ExecuteAndWait(compilerPath, finalArgs, std::nullopt, {}, 0, 0, &errMsg);
    
//     if (result != 0) {
//         errs() << "Error running modified clang command: " << errMsg << "\n";
//         return false;
//     }
    
//     return true;
// }

// // runLLVMASCommand() 的逻辑保持不变
// bool IRGenerator::runLLVMASCommand() {
//     auto llvmAsPathOrErr = sys::findProgramByName("llvm-as");
//     if (!llvmAsPathOrErr) {
//         errs() << "Warning: llvm-as not found in PATH\n";
//         return false;
//     }
//     std::string llvmAsPath = *llvmAsPathOrErr;
    
//     std::vector<StringRef> args = { llvmAsPath, irFilePath, "-o", bcFilePath };
    
//     std::string errMsg;
//     int result = sys::ExecuteAndWait(llvmAsPath, args, std::nullopt, {}, 0, 0, &errMsg);
    
//     if (result != 0) {
//         errs() << "Error running llvm-as: " << errMsg << "\n";
//         return false;
//     }
    
//     return true;
// }

// // Getters 保持不变
// std::string IRGenerator::getIRFilePath() const {
//     return irFilePath;
// }

// std::string IRGenerator::getBitcodeFilePath() const {
//     return bcFilePath;
// }

// } // namespace CryptoHLS



// IRGenerator.cpp

#include "IRGenerator.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/Program.h"
#include "llvm/Support/raw_ostream.h"

#include <vector>

using namespace llvm;

namespace CryptoHLS {

// [最终重构] 更新构造函数
IRGenerator::IRGenerator(const std::string& sourceFile,
                         const std::string& outputDir,
                         const std::vector<std::string>& systemIncludes)
    : sourceFile(sourceFile), outputDir(outputDir), systemIncludes(systemIncludes) {
    
    if (!sys::fs::exists(outputDir)) {
        if (std::error_code ec = sys::fs::create_directories(outputDir)) {
            errs() << "Error creating output directory: " << ec.message() << "\n";
        }
    }
    
    SmallString<128> irPath(outputDir);
    sys::path::append(irPath, sys::path::filename(sourceFile));
    sys::path::replace_extension(irPath, "ll");
    this->irFilePath = std::string(irPath.str());
    
    SmallString<128> bcPath(outputDir);
    sys::path::append(bcPath, sys::path::filename(sourceFile));
    sys::path::replace_extension(bcPath, "bc");
    this->bcFilePath = std::string(bcPath.str());
}

bool IRGenerator::generate() {
    if (!runClangCommand()) {
        errs() << "Error: Failed to generate LLVM IR\n";
        return false;
    }
    if (!runLLVMASCommand()) {
        errs() << "Warning: Failed to generate LLVM bitcode\n";
    }
    return true;
}

// [最终重构] 完全重写此函数，以构建一个干净的外部命令
bool IRGenerator::runClangCommand() {
    auto clangPathOrErr = sys::findProgramByName("clang++");
    if (!clangPathOrErr) {
        errs() << "Error: clang++ not found in PATH\n";
        return false;
    }
    std::string clangPath = *clangPathOrErr;
    
    // 1. 从零开始构建参数列表
    std::vector<std::string> newArgs;
    newArgs.push_back(clangPath); // 第一个参数是编译器本身

    // 2. 添加所有必需的系统头文件路径
    for (const auto& path : this->systemIncludes) {
        newArgs.push_back("-isystem");
        newArgs.push_back(path);
    }

    // 3. 添加其他固定的编译选项
    newArgs.push_back("-std=c++17");
    newArgs.push_back("-g"); // 保留调试信息

    // 4. 添加IR生成所需的核心参数
    newArgs.push_back("-S");
    newArgs.push_back("-emit-llvm");
    newArgs.push_back("-o");
    newArgs.push_back(this->irFilePath);

    // 5. 最后添加要编译的源文件
    newArgs.push_back(this->sourceFile);

    // 转换为 LLVM API 需要的格式
    std::vector<StringRef> finalArgs;
    for (const auto& s : newArgs) {
        finalArgs.push_back(s);
    }
    
    errs() << "Running Clean IR Generation Command: ";
    for (const auto& arg : finalArgs) { errs() << arg << " "; }
    errs() << "\n";
    
    std::string errMsg;
    int result = sys::ExecuteAndWait(clangPath, finalArgs, std::nullopt, {}, 0, 0, &errMsg);
    
    if (result != 0) {
        errs() << "Error running clang++: " << errMsg << "\n";
        return false;
    }
    
    return true;
}

// runLLVMASCommand() 不需要修改
bool IRGenerator::runLLVMASCommand() {
    auto llvmAsPathOrErr = sys::findProgramByName("llvm-as");
    if (!llvmAsPathOrErr) {
        errs() << "Warning: llvm-as not found in PATH\n";
        return false;
    }
    std::string llvmAsPath = *llvmAsPathOrErr;
    std::vector<StringRef> args = { llvmAsPath, irFilePath, "-o", bcFilePath };
    std::string errMsg;
    int result = sys::ExecuteAndWait(llvmAsPath, args, std::nullopt, {}, 0, 0, &errMsg);
    if (result != 0) {
        errs() << "Error running llvm-as: " << errMsg << "\n";
        return false;
    }
    return true;
}

std::string IRGenerator::getIRFilePath() const { return irFilePath; }
std::string IRGenerator::getBitcodeFilePath() const { return bcFilePath; }

} // namespace CryptoHLS