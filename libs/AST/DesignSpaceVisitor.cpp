// #include "DesignSpaceVisitor.h"
// #include "clang/AST/Decl.h"
// #include "clang/AST/Type.h"
// #include "clang/Basic/SourceManager.h"
// #include "llvm/Support/raw_ostream.h"

// using namespace clang;

// DesignSpaceVisitor::DesignSpaceVisitor(ASTContext *Context, std::string &TopFunctionName)
//     : Context(Context), TopFunctionName(TopFunctionName) {}

// const DesignSpace& DesignSpaceVisitor::getDesignSpace() const {
//     return TmpDesignSpace;
// }

// bool DesignSpaceVisitor::VisitFunctionDecl(FunctionDecl *FD) {
//     if (!FD || !FD->hasBody()) {
//         return true;
//     }

//     std::string FuncName = FD->getNameInfo().getName().getAsString();
    
//     if (FuncName == TopFunctionName) {
//         SourceManager &SM = Context->getSourceManager();
        
//         // 初始化当前函数的调用关系
//         functionCalls[FuncName] = {};
        
//         // 添加DATAFLOW优化点
//         OptimizationPoint dataflow_point;
//         dataflow_point.node_type = "FunctionBody";
//         dataflow_point.line = SM.getSpellingLineNumber(FD->getBeginLoc());
//         dataflow_point.name = FuncName + "_body";
//         dataflow_point.function = FuncName; // 记录所属函数
//         dataflow_point.type = "DATAFLOW";   // 优化类型
        
//         OptimizationOption dataflow_option;
//         dataflow_option.type = "DATAFLOW";
//         dataflow_point.options.push_back(dataflow_option);
//         TmpDesignSpace.push_back(dataflow_point);
        
//         // 添加接口优化点
//         for (const auto *Param : FD->parameters()) {
//             OptimizationPoint interface_point;
//             interface_point.node_type = "FunctionParameter";
//             interface_point.line = SM.getSpellingLineNumber(Param->getBeginLoc());
//             interface_point.name = Param->getNameAsString();
//             interface_point.function = FuncName; // 记录所属函数
//             interface_point.type = "INTERFACE";  // 优化类型
            
//             OptimizationOption interface_option;
//             interface_option.type = "INTERFACE";
//             interface_option.param_ranges["mode"] = {"s_axilite", "m_axi", "axis"};
//             interface_point.options.push_back(interface_option);
//             TmpDesignSpace.push_back(interface_point);
//         }

//         inTargetFunction = true;
//         TraverseStmt(FD->getBody());
//         inTargetFunction = false;
        
//         // 检查函数调用关系，移除不安全的DATAFLOW选项
//         for (auto& point : TmpDesignSpace) {
//             if (point.function == FuncName && point.type == "DATAFLOW") {
//                 if (!functionCalls[FuncName].empty()) {
//                     // 如果函数调用了其他函数，移除DATAFLOW选项
//                     point.options.erase(
//                         std::remove_if(point.options.begin(), point.options.end(),
//                             [](const OptimizationOption& opt) {
//                                 return opt.type == "DATAFLOW";
//                             }),
//                         point.options.end()
//                     );
//                 }
//             }
//         }
        
//         return false;
//     }
//     return true;
// }

// bool DesignSpaceVisitor::VisitForStmt(ForStmt *FS) {
//     if (inTargetFunction) {
//         SourceManager &SM = Context->getSourceManager();
//         unsigned line = SM.getSpellingLineNumber(FS->getBeginLoc());
        
//         OptimizationPoint point;
//         point.node_type = "ForLoop";
//         point.line = line;
//         point.name = "loop_at_line_" + std::to_string(line);
//         point.function = TopFunctionName; // 记录所属函数
//         point.type = "LOOP_OPT";        // 优化类型

//         OptimizationOption pipeline_option;
//         pipeline_option.type = "PIPELINE";
//         pipeline_option.param_ranges["II"] = {"1", "2"};
//         point.options.push_back(pipeline_option);

//         OptimizationOption unroll_option;
//         unroll_option.type = "UNROLL";
//         unroll_option.param_ranges["factor"] = {"2", "4", "8"};
//         point.options.push_back(unroll_option);
        
//         TmpDesignSpace.push_back(point);
//     }
//     return true;
// }

// bool DesignSpaceVisitor::VisitVarDecl(VarDecl *VD) {
//     if (inTargetFunction && VD->isLocalVarDecl() && VD->getType()->isArrayType()) {
//         SourceManager &SM = Context->getSourceManager();
//         unsigned line = SM.getSpellingLineNumber(VD->getBeginLoc());
        
//         OptimizationPoint point;
//         point.node_type = "Array";
//         point.line = line;
//         point.name = VD->getNameAsString();
//         point.function = TopFunctionName; // 记录所属函数
//         point.type = "ARRAY_OPT";         // 优化类型

//         OptimizationOption partition_option;
//         partition_option.type = "ARRAY_PARTITION";
//         partition_option.param_ranges["type"] = {"cyclic", "block", "complete"};
//         partition_option.param_ranges["factor"] = {"2", "4", "8"};
//         partition_option.param_ranges["dim"] = {"1"};
//         point.options.push_back(partition_option);

//         TmpDesignSpace.push_back(point);
//     }
//     return true;
// }

// bool DesignSpaceVisitor::VisitCallExpr(CallExpr *CE) {
//     if (inTargetFunction) {
//         FunctionDecl *Callee = CE->getDirectCallee();
//         if (Callee && Callee->hasBody() && !Context->getSourceManager().isInSystemHeader(Callee->getBeginLoc())) {
//             SourceManager &SM = Context->getSourceManager();
//             unsigned line = SM.getSpellingLineNumber(CE->getBeginLoc());
//             std::string caller = TopFunctionName;
//             std::string callee = Callee->getNameInfo().getName().getAsString();
            
//             // 记录函数调用关系
//             functionCalls[caller].push_back(callee);
            
//             OptimizationPoint point;
//             point.node_type = "FunctionCall";
//             point.line = line;
//             point.name = callee;
//             point.function = caller; // 记录所属函数
//             point.type = "INLINE_OPT"; // 优化类型
            
//             OptimizationOption inline_option;
//             inline_option.type = "INLINE";
//             inline_option.param_ranges["mode"] = {"on", "off"};
//             point.options.push_back(inline_option);
            
//             TmpDesignSpace.push_back(point);
//         }
//     }
//     return true;
// }



#include "DesignSpaceVisitor.h"
#include "clang/AST/Decl.h"
#include "clang/AST/Type.h"
#include "clang/Basic/SourceManager.h"
#include "llvm/Support/raw_ostream.h"

using namespace clang;

DesignSpaceVisitor::DesignSpaceVisitor(ASTContext *Context, std::string &TopFunctionName)
    : Context(Context), TopFunctionName(TopFunctionName) {}

const DesignSpace& DesignSpaceVisitor::getDesignSpace() const {
    return TmpDesignSpace;
}

bool DesignSpaceVisitor::VisitFunctionDecl(FunctionDecl *FD) {
    if (!FD || !FD->hasBody()) {
        return true;
    }

    std::string FuncName = FD->getNameInfo().getName().getAsString();
    
    if (FuncName == TopFunctionName) {
        SourceManager &SM = Context->getSourceManager();
        
        // 初始化当前函数的调用关系
        functionCalls[FuncName] = {};
        
        // 添加DATAFLOW优化点
        OptimizationPoint dataflow_point;
        dataflow_point.node_type = "FunctionBody";
        dataflow_point.line = SM.getSpellingLineNumber(FD->getBeginLoc());
        dataflow_point.name = FuncName + "_body";
        dataflow_point.function = FuncName; // 记录所属函数
        dataflow_point.type = "DATAFLOW";   // 优化类型
        
        OptimizationOption dataflow_option;
        dataflow_option.type = "DATAFLOW";
        dataflow_point.options.push_back(dataflow_option);
        TmpDesignSpace.push_back(dataflow_point);
        
        // 添加接口优化点
        for (const auto *Param : FD->parameters()) {
            OptimizationPoint interface_point;
            interface_point.node_type = "FunctionParameter";
            interface_point.line = SM.getSpellingLineNumber(Param->getBeginLoc());
            interface_point.name = Param->getNameAsString();
            interface_point.function = FuncName; // 记录所属函数
            interface_point.type = "INTERFACE";  // 优化类型
            
            OptimizationOption interface_option;
            interface_option.type = "INTERFACE";
            interface_option.param_ranges["mode"] = {"s_axilite", "m_axi", "axis"};
            interface_point.options.push_back(interface_option);
            TmpDesignSpace.push_back(interface_point);
        }

        inTargetFunction = true;
        TraverseStmt(FD->getBody());
        inTargetFunction = false;
        
        // 检查函数调用关系，移除不安全的DATAFLOW选项
        for (auto& point : TmpDesignSpace) {
            if (point.function == FuncName && point.type == "DATAFLOW") {
                if (!functionCalls[FuncName].empty()) {
                    // 如果函数调用了其他函数，移除DATAFLOW选项
                    point.options.erase(
                        std::remove_if(point.options.begin(), point.options.end(),
                            [](const OptimizationOption& opt) {
                                return opt.type == "DATAFLOW";
                            }),
                        point.options.end()
                    );
                }
            }
        }
        
        return false;
    }
    return true;
}

bool DesignSpaceVisitor::VisitForStmt(ForStmt *FS) {
    if (inTargetFunction) {
        SourceManager &SM = Context->getSourceManager();
        unsigned line = SM.getSpellingLineNumber(FS->getBeginLoc());
        
        OptimizationPoint point;
        point.node_type = "ForLoop";
        point.line = line;
        point.name = "loop_at_line_" + std::to_string(line);
        point.function = TopFunctionName; // 记录所属函数
        point.type = "LOOP_OPT";        // 优化类型

        OptimizationOption pipeline_option;
        pipeline_option.type = "PIPELINE";
        pipeline_option.param_ranges["II"] = {"1", "2"};
        point.options.push_back(pipeline_option);

        OptimizationOption unroll_option;
        unroll_option.type = "UNROLL";
        unroll_option.param_ranges["factor"] = {"2", "4", "8"};
        point.options.push_back(unroll_option);
        
        TmpDesignSpace.push_back(point);
    }
    return true;
}

bool DesignSpaceVisitor::VisitVarDecl(VarDecl *VD) {
    if (inTargetFunction && VD->isLocalVarDecl() && VD->getType()->isArrayType()) {
        SourceManager &SM = Context->getSourceManager();
        unsigned line = SM.getSpellingLineNumber(VD->getBeginLoc());
        
        OptimizationPoint point;
        point.node_type = "Array";
        point.line = line;
        point.name = VD->getNameAsString();
        point.function = TopFunctionName; // 记录所属函数
        point.type = "ARRAY_OPT";         // 优化类型

        OptimizationOption partition_option;
        partition_option.type = "ARRAY_PARTITION";
        partition_option.param_ranges["type"] = {"cyclic", "block", "complete"};
        partition_option.param_ranges["factor"] = {"2", "4", "8"};
        partition_option.param_ranges["dim"] = {"1"};
        point.options.push_back(partition_option);

        // --- 选项2: STREAM (新增) ---
        // 这是关键的补充，让DSE可以为数组选择STREAM优化
        OptimizationOption stream_option;
        stream_option.type = "STREAM";
        stream_option.param_ranges["depth"] = {"16", "32", "64"};
        point.options.push_back(stream_option);

        TmpDesignSpace.push_back(point);
    }
    return true;
}

bool DesignSpaceVisitor::VisitCallExpr(CallExpr *CE) {
    if (inTargetFunction) {
        FunctionDecl *Callee = CE->getDirectCallee();
        if (Callee && Callee->hasBody() && !Context->getSourceManager().isInSystemHeader(Callee->getBeginLoc())) {
            SourceManager &SM = Context->getSourceManager();
            unsigned line = SM.getSpellingLineNumber(CE->getBeginLoc());
            std::string caller = TopFunctionName;
            std::string callee = Callee->getNameInfo().getName().getAsString();
            
            // 记录函数调用关系
            functionCalls[caller].push_back(callee);
            
            OptimizationPoint point;
            point.node_type = "FunctionCall";
            point.line = line;
            point.name = callee;
            point.function = caller; // 记录所属函数
            point.type = "INLINE_OPT"; // 优化类型
            
            OptimizationOption inline_option;
            inline_option.type = "INLINE";
            inline_option.param_ranges["mode"] = {"on", "off"};
            point.options.push_back(inline_option);
            
            TmpDesignSpace.push_back(point);
        }
    }
    return true;
}