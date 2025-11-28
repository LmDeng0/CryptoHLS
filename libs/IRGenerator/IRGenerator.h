// #ifndef CRYPTOHLS_IR_GENERATOR_H
// #define CRYPTOHLS_IR_GENERATOR_H

// #include <string>
// #include <vector>

// namespace CryptoHLS {

// class IRGenerator {
// public:
//     IRGenerator(const std::string& sourceFile, 
//                 const std::string& outputDir,
//                 const std::vector<std::string>& extraArgs);

//     bool generate();

//     std::string getIRFilePath() const;
//     std::string getBitcodeFilePath() const;

// private:
//     bool runClangCommand();
//     bool runLLVMASCommand();

//     std::string sourceFile;
//     std::string outputDir;
//     std::vector<std::string> extraArgs;
//     std::string irFilePath;
//     std::string bcFilePath;
// };

// } // namespace CryptoHLS

// #endif // CRYPTOHLS_IR_GENERATOR_H


// IRGenerator.h

// #ifndef CRYPTOHLS_IRGENERATOR_H
// #define CRYPTOHLS_IRGENERATOR_H

// #include <string>
// #include <vector>

// namespace CryptoHLS {

// class IRGenerator {
// public:
//     // [重构] 构造函数现在接收完整的原始编译指令，以及源文件和输出目录信息
//     IRGenerator(const std::vector<std::string>& originalCommand,
//                 const std::string& sourceFile,
//                 const std::string& outputDir);

//     // 生成 LLVM IR (.ll) 和 Bitcode (.bc)
//     bool generate();

//     // 获取生成文件的路径
//     std::string getIRFilePath() const;
//     std::string getBitcodeFilePath() const;

// private:
//     bool runClangCommand();
//     bool runLLVMASCommand();

//     std::vector<std::string> originalCommand; // 保存原始编译指令
//     std::string sourceFile;
//     std::string outputDir;
//     std::string irFilePath;
//     std::string bcFilePath;
// };

// } // namespace CryptoHLS

// #endif // CRYPTOHLS_IRGENERATOR_H


// IRGenerator.h

#ifndef CRYPTOHLS_IRGENERATOR_H
#define CRYPTOHLS_IRGENERATOR_H

#include <string>
#include <vector>

namespace CryptoHLS {

class IRGenerator {
public:
    // [最终重构] 构造函数现在接收源文件、输出目录和干净的系统头文件路径列表
    IRGenerator(const std::string& sourceFile,
                const std::string& outputDir,
                const std::vector<std::string>& systemIncludes);

    bool generate();
    std::string getIRFilePath() const;
    std::string getBitcodeFilePath() const;

private:
    bool runClangCommand();
    bool runLLVMASCommand();

    std::string sourceFile;
    std::string outputDir;
    std::vector<std::string> systemIncludes; // 保存系统头文件路径
    std::string irFilePath;
    std::string bcFilePath;
};

} // namespace CryptoHLS

#endif // CRYPTOHLS_IRGENERATOR_H