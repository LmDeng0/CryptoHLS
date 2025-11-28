// #ifndef CRYPTOHLS_PRAGMA_INSERTER_H
// #define CRYPTOHLS_PRAGMA_INSERTER_H

// #include "Core.h"
// #include "clang/AST/RecursiveASTVisitor.h"
// #include "clang/Rewrite/Core/Rewriter.h"
// #include "clang/Lex/Lexer.h" // 引入 Lexer

// class PragmaInserterVisitor : public clang::RecursiveASTVisitor<PragmaInserterVisitor> {
// public:
//     PragmaInserterVisitor(clang::Rewriter &R, const Solution &S);

//     bool VisitFunctionDecl(clang::FunctionDecl *FD);
//     bool VisitDeclStmt(clang::DeclStmt *DS);
//     bool VisitForStmt(clang::ForStmt *FS);
//     bool VisitCallExpr(clang::CallExpr *CE);

// private:
//     clang::Rewriter &TheRewriter;
//     const Solution &BestSolution;
// };

// #endif // CRYPTOHLS_PRAGMA_INSERTER_H



#ifndef CRYPTOHLS_PRAGMA_INSERTER_H
#define CRYPTOHLS_PRAGMA_INSERTER_H

#include "Core.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Lex/Lexer.h"
#include "clang/AST/Decl.h" // 添加缺失的包含

class PragmaInserterVisitor : public clang::RecursiveASTVisitor<PragmaInserterVisitor> {
public:
    PragmaInserterVisitor(clang::Rewriter &R, const Solution &S);

    bool VisitFunctionDecl(clang::FunctionDecl *FD);
    bool VisitDeclStmt(clang::DeclStmt *DS);
    bool VisitForStmt(clang::ForStmt *FS);
    bool VisitCallExpr(clang::CallExpr *CE);

private:
    bool arrayAccessedInMultipleTasks(const clang::VarDecl* VD);
    clang::Rewriter &TheRewriter;
    const Solution &BestSolution;
};

#endif // CRYPTOHLS_PRAGMA_INSERTER_H