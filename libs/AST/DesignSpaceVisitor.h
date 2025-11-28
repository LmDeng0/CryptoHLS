// // #ifndef CRYPTOHLS_DESIGN_SPACE_VISITOR_H
// // #define CRYPTOHLS_DESIGN_SPACE_VISITOR_H

// // #include "clang/AST/RecursiveASTVisitor.h"
// // #include "clang/AST/ASTContext.h"
// // #include "Core.h" // 确保包含了定义 DesignSpace 和 OptimizationPoint 的头文件
// // #include <string>
// // #include <vector>

// // class DesignSpaceVisitor : public clang::RecursiveASTVisitor<DesignSpaceVisitor> {
// // public:
// //     // 1. 修正构造函数：现在它接收 TopFunctionName 的引用
// //     DesignSpaceVisitor(clang::ASTContext *Context, std::string &TopFunctionName);

// //     // 2. 确保这些访问函数只声明一次
// //     bool VisitFunctionDecl(clang::FunctionDecl *FD);
// //     bool VisitForStmt(clang::ForStmt *FS);
// //     bool VisitVarDecl(clang::VarDecl *VD);

// //     const DesignSpace& getDesignSpace() const;

// // private:
// //     clang::ASTContext *Context;
// //     DesignSpace TmpDesignSpace;
// //     std::string &TopFunctionName; // 引用成员
// //     bool inTargetFunction = false; // 我们的诊断探针
// // };

// // #endif // CRYPTOHLS_DESIGN_SPACE_VISITOR_H



// #ifndef CRYPTOHLS_DESIGN_SPACE_VISITOR_H
// #define CRYPTOHLS_DESIGN_SPACE_VISITOR_H

// #include "Core.h"
// #include "clang/AST/RecursiveASTVisitor.h"
// #include "clang/AST/ASTContext.h"
// #include <string>
// #include <vector>

// class DesignSpaceVisitor : public clang::RecursiveASTVisitor<DesignSpaceVisitor> {
// public:
//     DesignSpaceVisitor(clang::ASTContext *Context, std::string &TopFunctionName);

//     const DesignSpace& getDesignSpace() const;

//     // 访问函数声明 (用于识别顶层函数, DATAFLOW 和 INTERFACE)
//     bool VisitFunctionDecl(clang::FunctionDecl *FD);
    
//     // 访问 for 循环 (保持不变)
//     bool VisitForStmt(clang::ForStmt *FS);
    
//     // 访问变量声明 (保持不变)
//     bool VisitVarDecl(clang::VarDecl *VD);

//     // [新增] 访问函数调用 (用于识别 INLINE)
//     bool VisitCallExpr(clang::CallExpr *CE);

// private:
//     clang::ASTContext *Context;
//     std::string &TopFunctionName;
//     DesignSpace TmpDesignSpace;
//     bool inTargetFunction = false;
// };

// #endif // CRYPTOHLS_DESIGN_SPACE_VISITOR_H


// #ifndef CRYPTOHLS_PRAGMA_INSERTER_H
// #define CRYPTOHLS_PRAGMA_INSERTER_H

// #include "Core.h"
// #include "clang/AST/RecursiveASTVisitor.h"
// #include "clang/Rewrite/Core/Rewriter.h"

// class PragmaInserterVisitor : public clang::RecursiveASTVisitor<PragmaInserterVisitor> {
// public:
//     PragmaInserterVisitor(clang::Rewriter &R, const Solution &S);

//     // [关键修正] 补上缺失的函数声明
//     bool VisitFunctionDecl(clang::FunctionDecl *FD);
//     bool VisitParmVarDecl(clang::ParmVarDecl *PVD);
//     bool VisitForStmt(clang::ForStmt *FS);
//     bool VisitVarDecl(clang::VarDecl *VD);

// private:
//     clang::Rewriter &TheRewriter;
//     const Solution &BestSolution;
// };

// #endif // CRYPTOHLS_PRAGMA_INSERTER_H


// #ifndef CRYPTOHLS_DESIGN_SPACE_VISITOR_H
// #define CRYPTOHLS_DESIGN_SPACE_VISITOR_H

// #include "Core.h"
// #include "clang/AST/RecursiveASTVisitor.h"
// #include "clang/AST/ASTContext.h"
// #include <string>
// #include <vector>

// class DesignSpaceVisitor : public clang::RecursiveASTVisitor<DesignSpaceVisitor> {
// public:
//     DesignSpaceVisitor(clang::ASTContext *Context, std::string &TopFunctionName);

//     const DesignSpace& getDesignSpace() const;

//     bool VisitFunctionDecl(clang::FunctionDecl *FD);
//     bool VisitForStmt(clang::ForStmt *FS);
//     bool VisitVarDecl(clang::VarDecl *VD);
//     bool VisitCallExpr(clang::CallExpr *CE);

// private:
//     clang::ASTContext *Context;
//     std::string &TopFunctionName;
//     DesignSpace TmpDesignSpace;
//     bool inTargetFunction = false;
// }; // <-- [关键修正] 确保这里有一个分号

// #endif // CRYPTOHLS_DESIGN_SPACE_VISITOR_H

// #ifndef CRYPTOHLS_DESIGN_SPACE_VISITOR_H
// #define CRYPTOHLS_DESIGN_SPACE_VISITOR_H

// #include "Core.h"
// #include "clang/AST/RecursiveASTVisitor.h"
// #include "clang/AST/ASTContext.h"
// #include <string>
// #include <vector>

// class DesignSpaceVisitor : public clang::RecursiveASTVisitor<DesignSpaceVisitor> {
// public:
//     DesignSpaceVisitor(clang::ASTContext *Context, std::string &TopFunctionName);

//     const DesignSpace& getDesignSpace() const;

//     bool VisitFunctionDecl(clang::FunctionDecl *FD);
//     bool VisitForStmt(clang::ForStmt *FS);
//     bool VisitVarDecl(clang::VarDecl *VD);
//     bool VisitCallExpr(clang::CallExpr *CE);

// private:
//     clang::ASTContext *Context;
//     std::string &TopFunctionName;
//     DesignSpace TmpDesignSpace;
//     bool inTargetFunction = false;
// }; // <-- [关键修正] 确保这里有一个分号

// #endif // CRYPTOHLS_DESIGN_SPACE_VISITOR_H



#ifndef CRYPTOHLS_DESIGN_SPACE_VISITOR_H
#define CRYPTOHLS_DESIGN_SPACE_VISITOR_H

#include "Core.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include <map>
#include <vector>
#include <string>

class DesignSpaceVisitor : public clang::RecursiveASTVisitor<DesignSpaceVisitor> {
public:
    DesignSpaceVisitor(clang::ASTContext *Context, std::string &TopFunctionName);
    
    const DesignSpace& getDesignSpace() const;
    
    bool VisitFunctionDecl(clang::FunctionDecl *FD);
    bool VisitForStmt(clang::ForStmt *FS);
    bool VisitVarDecl(clang::VarDecl *VD);
    bool VisitCallExpr(clang::CallExpr *CE);
    
private:
    clang::ASTContext *Context;
    std::string &TopFunctionName;
    DesignSpace TmpDesignSpace;
    bool inTargetFunction = false;
    std::map<std::string, std::vector<std::string>> functionCalls; // 记录函数调用关系
};

#endif // CRYPTOHLS_DESIGN_SPACE_VISITOR_H