// #ifndef CRYPTOHLS_ANALYSIS_PASSES_H
// #define CRYPTOHLS_ANALYSIS_PASSES_H

// #include "llvm/Pass.h"
// #include "llvm/IR/Module.h"

// namespace CryptoHLS {

// // Pass 1: 密码学语义分析
// struct CryptoSemanticAnalysisPass : public llvm::ModulePass {
//     static char ID;
//     CryptoSemanticAnalysisPass() : llvm::ModulePass(ID) {}

//     bool runOnModule(llvm::Module &M) override;
// };

// // Pass 2: 安全扫描 (污点分析)
// struct SecurityScanPass : public llvm::ModulePass {
//     static char ID;
//     SecurityScanPass() : llvm::ModulePass(ID) {}

//     bool runOnModule(llvm::Module &M) override;
// };

// } // namespace CryptoHLS

// #endif // CRYPTOHLS_ANALYSIS_PASSES_H


#ifndef CRYPTOHLS_ANALYSIS_PASSES_H
#define CRYPTOHLS_ANALYSIS_PASSES_H

#include "llvm/Pass.h"
#include "llvm/IR/Module.h"

namespace CryptoHLS {

// 密码学语义分析Pass
struct CryptoSemanticAnalysisPass : public llvm::ModulePass {
    static char ID;
    CryptoSemanticAnalysisPass() : llvm::ModulePass(ID) {}

    bool runOnModule(llvm::Module &M) override;
};

// 安全扫描Pass
struct SecurityScanPass : public llvm::ModulePass {
    static char ID;
    SecurityScanPass() : llvm::ModulePass(ID) {}

    bool runOnModule(llvm::Module &M) override;
};

} // namespace CryptoHLS

#endif // CRYPTOHLS_ANALYSIS_PASSES_H