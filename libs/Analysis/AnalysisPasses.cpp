// #include "AnalysisPasses.h"
// #include "llvm/IR/Function.h"
// #include "llvm/IR/Instructions.h"
// #include "llvm/IR/Constants.h"
// #include "llvm/IR/Metadata.h"
// #include "llvm/Support/raw_ostream.h"
// #include <set>

// using namespace llvm;

// namespace CryptoHLS {

// char CryptoSemanticAnalysisPass::ID = 0;
// char SecurityScanPass::ID = 0;

// // =================================================================
// // Pass 1: 密码学语义分析实现
// // =================================================================
// bool CryptoSemanticAnalysisPass::runOnModule(Module &M) {
//     bool modified = false;
//     LLVMContext &Ctx = M.getContext();

//     for (Function &F : M) {
//         if (F.isDeclaration()) continue;

//         unsigned bitwiseOpCount = 0;
//         bool hasLargeConstantArray = false;

//         // 特征1: 统计位运算指令数量
//         for (BasicBlock &BB : F) {
//             for (Instruction &I : BB) {
//                 if (isa<BinaryOperator>(I)) {
//                     unsigned opcode = I.getOpcode();
//                     if (opcode == Instruction::Xor || opcode == Instruction::And || opcode == Instruction::Or || opcode == Instruction::Shl ||
//                         opcode == Instruction::LShr) {
//                         bitwiseOpCount++;
//                     }
//                 }
//             }
//         }

//         // 特征2: 检查是否存在大型常量数组 (潜在的S-Box)
//         for (GlobalVariable &GV : M.globals()) {
//             if (GV.isConstant() && GV.hasInitializer()) {
//                 if (ArrayType *AT = dyn_cast<ArrayType>(GV.getInitializer()->getType())) {
//                     if (AT->getNumElements() >= 16) { // 例如，S-Box至少有16个元素
//                         hasLargeConstantArray = true;
//                         break;
//                     }
//                 }
//             }
//         }

//         // 启发式规则: 如果一个函数有大量的位运算，或者代码中存在S-Box，
//         // 我们就认为它是密码学相关的。
//         if (bitwiseOpCount > 50 || hasLargeConstantArray) {
//             errs() << " Identified cryptographic function: " << F.getName() << "\n";
            
//             // 为函数附加元数据标签
//             MDNode *N = MDNode::get(Ctx, MDString::get(Ctx, "true"));
//             F.setMetadata("crypto.semantic", N);
//             modified = true;
//         }
//     }
//     return modified;
// }

// // =================================================================
// // Pass 2: 安全扫描 (污点分析) 实现
// // =================================================================
// bool SecurityScanPass::runOnModule(Module &M) {
//     std::set<Value*> taintedValues;

//     // 步骤1: 识别污点源
//     // 我们将所有被标记为密码学函数的参数视为“敏感数据源”
//     for (Function &F : M) {
//         if (F.getMetadata("crypto.semantic")) {
//             errs() << " Analyzing sensitive function: " << F.getName() << "\n";
//             for (Argument &Arg : F.args()) {
//                 taintedValues.insert(&Arg);
//             }
//         }
//     }

//     if (taintedValues.empty()) {
//         errs() << " No sensitive data sources found. Skipping.\n";
//         return false;
//     }

//     // 步骤2: 污点传播 (使用Worklist算法)
//     std::vector<Value*> worklist;
//     worklist.assign(taintedValues.begin(), taintedValues.end());

//     while (!worklist.empty()) {
//         Value *V = worklist.back();
//         worklist.pop_back();

//         for (User *U : V->users()) {
//             if (Instruction *I = dyn_cast<Instruction>(U)) {
//                 // 如果一个指令使用了被污染的值，那么这个指令的结果也被污染
//                 if (taintedValues.find(I) == taintedValues.end()) {
//                     taintedValues.insert(I);
//                     worklist.push_back(I);

//                     // 步骤3: 检查污点汇聚点 (Vulnerability Sinks)
//                     // 检查1: 敏感数据是否影响了分支条件
//                     if (BranchInst *BI = dyn_cast<BranchInst>(I)) {
//                         if (BI->isConditional() && taintedValues.count(BI->getCondition())) {
//                             errs() << "  Potential Timing Attack: Secret-dependent branch found at instruction:\n"
//                                    << "    " << *BI << "\n";
//                         }
//                     }
//                     // 检查2: 敏感数据是否影响了内存访问地址
//                     else if (StoreInst *SI = dyn_cast<StoreInst>(I)) {
//                         if (taintedValues.count(SI->getPointerOperand())) {
//                              errs() << "  Potential Cache/Power Attack: Secret-dependent store found at instruction:\n"
//                                    << "    " << *SI << "\n";
//                         }
//                     }
//                     else if (LoadInst *LI = dyn_cast<LoadInst>(I)) {
//                          if (taintedValues.count(LI->getPointerOperand())) {
//                              errs() << "  Potential Cache/Power Attack: Secret-dependent load found at instruction:\n"
//                                    << "    " << *LI << "\n";
//                         }
//                     }
//                 }
//             }
//         }
//     }

//     return false; // 这个Pass只做分析，不修改Module
// }

// } // namespace CryptoHLS

// // 注册Pass，使其可以被PassManager使用
// static RegisterPass<CryptoHLS::CryptoSemanticAnalysisPass> X("crypto-semantic", "Cryptographic Semantic Analysis Pass", false, false);
// static RegisterPass<CryptoHLS::SecurityScanPass> Y("security-scan", "Security Vulnerability Scan Pass", false, false);




#include "AnalysisPasses.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Constants.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace CryptoHLS {

char CryptoSemanticAnalysisPass::ID = 0;
char SecurityScanPass::ID = 0;

// 密码学语义分析Pass的实现
bool CryptoSemanticAnalysisPass::runOnModule(Module &M) {
    bool modified = false;
    LLVMContext& context = M.getContext();

    for (Function &F : M) {
        for (BasicBlock &BB : F) {
            for (Instruction &I : BB) {
                // 示例：识别一个典型的S-Box查找操作
                // 即：从一个大的常量数组中加载一个元素
                if (auto *LI = dyn_cast<LoadInst>(&I)) {
                    if (auto *GEP = dyn_cast<GetElementPtrInst>(LI->getPointerOperand())) {
                        if (auto *GlobalArray = dyn_cast<GlobalVariable>(GEP->getPointerOperand())) {
                            if (GlobalArray->isConstant() && GlobalArray->getInitializer()->getType()->isArrayTy()) {
                                ArrayType* AT = cast<ArrayType>(GlobalArray->getInitializer()->getType());
                                if (AT->getNumElements() >= 256) { // AES S-Box大小的典型特征
                                    // [验证点] 打印识别日志
                                    outs() << "[CryptoSemanticAnalysis] Found potential S-Box lookup at: " << I << "\n";
                                    // 为该指令附加元数据
                                    I.setMetadata("crypto.op", MDNode::get(context, MDString::get(context, "SBOX_LOOKUP")));
                                    modified = true;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    return modified; // 如果附加了元数据，返回true
}

// 安全扫描Pass的实现
bool SecurityScanPass::runOnModule(Module &M) {
    // 在这里，我们将实现一个简化的污点分析
    // 目标：找到被标记为"crypto.op"的指令，并检查其结果是否影响了分支

    for (Function &F : M) {
        // 简单演示：我们假设任何带有 "crypto.op" 元数据的指令都是污点源
        std::vector<Instruction*> TaintedInstructions;
        for (BasicBlock &BB : F) {
            for (Instruction &I : BB) {
                if (I.hasMetadata("crypto.op")) {
                    TaintedInstructions.push_back(&I);
                }
            }
        }

        // 检查污点是否影响了分支
        for (BasicBlock &BB : F) {
            if (auto *BI = dyn_cast<BranchInst>(BB.getTerminator())) {
                if (BI->isConditional()) {
                    Value* Condition = BI->getCondition();
                    // 这是一个非常简化的检查，实际的污点分析会更复杂
                    for (Instruction* TaintedInst : TaintedInstructions) {
                        if (Condition == TaintedInst) {
                            // [验证点] 打印安全警告
                            errs() << "[SecurityScan] WARNING: Potential side-channel leak!\n";
                            errs() << "  >> Tainted instruction result: " << *TaintedInst << "\n";
                            errs() << "  >> Directly affects branch condition at: " << *BI << "\n";
                        }
                    }
                }
            }
        }
    }
    return false; // 这个Pass只进行分析，不修改代码
}

} // namespace CryptoHLS