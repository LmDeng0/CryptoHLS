// #include "PragmaInserter.h"
// #include "clang/AST/Decl.h"
// #include "clang/Basic/SourceManager.h"
// #include <sstream>
// #include <set>
// #include <cctype>

// using namespace clang;

// PragmaInserterVisitor::PragmaInserterVisitor(Rewriter &R, const Solution &S)
//     : TheRewriter(R), BestSolution(S) {}

// // 检查函数是否包含函数调用
// bool hasFunctionCalls(FunctionDecl *FD) {
//     if (!FD || !FD->hasBody()) return false;
    
//     for (auto stmt = FD->getBody()->child_begin(); stmt != FD->getBody()->child_end(); ++stmt) {
//         if (isa<CallExpr>(*stmt)) {
//             return true;
//         }
//         // 递归检查子语句
//         if (isa<CompoundStmt>(*stmt)) {
//             for (auto child : cast<CompoundStmt>(*stmt)->body()) {
//                 if (isa<CallExpr>(child)) {
//                     return true;
//                 }
//             }
//         }
//     }
//     return false;
// }

// bool PragmaInserterVisitor::VisitFunctionDecl(FunctionDecl *FD) {
//     if (!FD->hasBody() || !isa<CompoundStmt>(FD->getBody())) {
//         return true;
//     }

//     SourceManager &SM = TheRewriter.getSourceMgr();
//     std::stringstream function_pragmas;
//     unsigned func_line = SM.getSpellingLineNumber(FD->getBeginLoc());

//     // 收集接口Pragma
//     for (const auto* Param : FD->parameters()) {
//         unsigned param_line = SM.getSpellingLineNumber(Param->getBeginLoc());
//         for (const auto& cmd : BestSolution) {
//             if (cmd.line == param_line && cmd.type == "INTERFACE") {
//                 function_pragmas << "    #pragma HLS " << cmd.type << " port=" << Param->getNameAsString();
//                 for (const auto &param : cmd.params) { 
//                     function_pragmas << " " << param.first << "=" << param.second; 
//                 }
//                 function_pragmas << "\n";
//             }
//         }
//     }
    
//     // 收集DATAFLOW Pragma（如果存在）
//     bool hasDataflowPragma = false;
//     for (const auto& cmd : BestSolution) {
//         if (cmd.line == func_line && cmd.type == "DATAFLOW") {
//             // 关键安全约束：如果函数包含函数调用，跳过DATAFLOW插入
//             if (!hasFunctionCalls(FD)) {
//                 function_pragmas << "    #pragma HLS " << cmd.type << "\n";
//                 hasDataflowPragma = true;
//             }
//         }
//     }

//     // 插入Pragma
//     if (!function_pragmas.str().empty()) {
//         CompoundStmt *Body = cast<CompoundStmt>(FD->getBody());
//         SourceLocation BraceLoc = Body->getLBracLoc();
//         SourceLocation AfterBraceLoc = BraceLoc.getLocWithOffset(1);
//         TheRewriter.InsertText(AfterBraceLoc, "\n" + function_pragmas.str(), true, true);
//     }

//     return true;
// }

// bool PragmaInserterVisitor::VisitDeclStmt(DeclStmt *DS) {
//     if (!DS->isSingleDecl()) return true;
//     const VarDecl *VD = dyn_cast<VarDecl>(DS->getSingleDecl());
//     if (!VD || !VD->isLocalVarDecl() || !VD->getType()->isArrayType()) return true;

//     SourceManager &SM = TheRewriter.getSourceMgr();
//     unsigned line = SM.getSpellingLineNumber(DS->getBeginLoc());

//     for (const auto &cmd : BestSolution) {
//         if (cmd.line == line && (cmd.type == "ARRAY_PARTITION" || cmd.type == "STREAM")) {
//             // 安全约束：跳过state数组的STREAM指令
//             if (cmd.type == "STREAM" && VD->getNameAsString() == "state") {
//                 continue;
//             }
            
//             std::stringstream pragma_stream;
//             pragma_stream << "\n    #pragma HLS " << cmd.type;
            
//             // 为ARRAY_PARTITION添加variable参数
//             if(cmd.type == "ARRAY_PARTITION"){
//                 pragma_stream << " variable=" << VD->getNameAsString();
//             }

//             for (const auto &param : cmd.params) {
//                 pragma_stream << " " << param.first << "=" << param.second;
//             }
            
//             SourceLocation end_loc = DS->getEndLoc();
//             SourceLocation after_semicolon = Lexer::findLocationAfterToken(
//                 end_loc, tok::semi, SM, TheRewriter.getLangOpts(), false);

//             if (after_semicolon.isValid()) {
//                 TheRewriter.InsertText(after_semicolon, pragma_stream.str(), true, true);
//             }
//         }
//     }
//     return true;
// }

// bool PragmaInserterVisitor::VisitForStmt(ForStmt *FS) {
//     SourceManager &SM = TheRewriter.getSourceMgr();
//     unsigned line = SM.getSpellingLineNumber(FS->getBeginLoc());
//     for (const auto &cmd : BestSolution) {
//         if (cmd.line == line && (cmd.type == "PIPELINE" || cmd.type == "UNROLL")) {
//             std::stringstream pragma_stream;
//             pragma_stream << "    #pragma HLS " << cmd.type;
//             for (const auto &param : cmd.params) { 
//                 pragma_stream << " " << param.first << "=" << param.second; 
//             }
//             pragma_stream << "\n";
//             TheRewriter.InsertTextBefore(FS->getBeginLoc(), pragma_stream.str());
//         }
//     }
//     return true;
// }

// bool PragmaInserterVisitor::VisitCallExpr(CallExpr *CE) {
//     if (FunctionDecl *FD = CE->getDirectCallee()) {
//         SourceManager &SM = TheRewriter.getSourceMgr();
//         unsigned line = SM.getSpellingLineNumber(CE->getBeginLoc());
        
//         for (const auto &cmd : BestSolution) {
//             if (cmd.line == line && cmd.type == "INLINE") {
//                 // 只插入"on"模式的INLINE指令
//                 auto modeIt = cmd.params.find("mode");
//                 if (modeIt != cmd.params.end() && modeIt->second == "on") {
//                     std::stringstream pragma_stream;
//                     pragma_stream << "    #pragma HLS " << cmd.type;
                    
//                     TheRewriter.InsertTextBefore(CE->getBeginLoc(), pragma_stream.str() + "\n");
//                 }
//             }
//         }
//     }
//     return true;
// }



#include "PragmaInserter.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/AST/Decl.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Lex/Lexer.h"
#include <sstream>
#include <set>
#include <cctype>
#include <regex>

using namespace clang;

// ----------------- 小工具 -----------------

// 根据 SourceLocation 的列号生成相同缩进（列号从 1 起）
static std::string makeIndent(SourceManager &SM, SourceLocation Loc) {
    unsigned col = SM.getSpellingColumnNumber(Loc);
    if (col == 0) col = 1;
    return std::string(col - 1, ' ');
}

// 计算一个语句行文本的前导空白（有时列号不准时可使用）
static std::string leadingSpaces(const std::string &s) {
    size_t i = 0;
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) ++i;
    return s.substr(0, i);
}

class CallExprVisitor : public RecursiveASTVisitor<CallExprVisitor> {
public:
    bool hasCalls = false;
    bool VisitCallExpr(CallExpr *CE) {
        if (CE->getDirectCallee()) hasCalls = true;
        return true;
    }
};

static bool hasFunctionCalls_Robust(FunctionDecl *FD) {
    if (!FD || !FD->hasBody()) return false;
    CallExprVisitor v;
    v.TraverseStmt(FD->getBody());
    return v.hasCalls;
}

// ----------------- 访问器实现 -----------------

PragmaInserterVisitor::PragmaInserterVisitor(Rewriter &R, const Solution &S)
    : TheRewriter(R), BestSolution(S) {}

bool PragmaInserterVisitor::VisitFunctionDecl(FunctionDecl *FD) {
    if (!FD->hasBody() || !isa<CompoundStmt>(FD->getBody())) return true;

    SourceManager &SM = TheRewriter.getSourceMgr();
    std::stringstream function_pragmas;
    unsigned func_line = SM.getSpellingLineNumber(FD->getBeginLoc());
    std::string func_name = FD->getNameAsString();

    // 1) INTERFACE：按参数行号匹配
    for (const auto *Param : FD->parameters()) {
        unsigned param_line = SM.getSpellingLineNumber(Param->getBeginLoc());
        for (const auto &cmd : BestSolution) {
            if (cmd.line == param_line && cmd.type == "INTERFACE") {
                function_pragmas << "    #pragma HLS " << cmd.type
                                 << " port=" << Param->getNameAsString();
                for (const auto &p : cmd.params)
                    function_pragmas << " " << p.first << "=" << p.second;
                function_pragmas << "\n";
            }
        }
    }

    // 2) DATAFLOW：只在函数体内无调用时插入（按函数声明行匹配）
    for (const auto &cmd : BestSolution) {
        if (cmd.type == "DATAFLOW" &&
            (cmd.line == func_line || (!cmd.function.empty() && cmd.function == func_name))) {
            if (!hasFunctionCalls_Robust(FD)) {
                function_pragmas << "    #pragma HLS DATAFLOW\n";
            }
        }
    }

    // 3) INLINE：插入到**被内联函数体**起始（左花括号后一行）
    for (const auto &cmd : BestSolution) {
        if (cmd.type == "INLINE" &&
            (cmd.function.empty() ? (cmd.line == func_line) : (cmd.function == func_name))) {

            // 默认仅在 "mode=on" 或未指定 mode 时插入
            auto it = cmd.params.find("mode");
            if (it == cmd.params.end() || it->second == "on") {
                function_pragmas << "    #pragma HLS INLINE\n";
            }
        }
    }

    // 实际插入（在 '{' 后一行）
    if (!function_pragmas.str().empty()) {
        auto *Body = cast<CompoundStmt>(FD->getBody());
        SourceLocation LBrace = Body->getLBracLoc();
        SourceLocation AfterBrace = LBrace.getLocWithOffset(1);
        TheRewriter.InsertText(AfterBrace, "\n" + function_pragmas.str(), true, true);
    }

    return true;
}

bool PragmaInserterVisitor::VisitDeclStmt(DeclStmt *DS) {
    if (!DS->isSingleDecl()) return true;
    const VarDecl *VD = dyn_cast<VarDecl>(DS->getSingleDecl());
    if (!VD || !VD->isLocalVarDecl() || !VD->getType()->isArrayType()) return true;

    SourceManager &SM = TheRewriter.getSourceMgr();
    unsigned line = SM.getSpellingLineNumber(DS->getBeginLoc());

    for (const auto &cmd : BestSolution) {
        if (cmd.line == line && (cmd.type == "ARRAY_PARTITION" || cmd.type == "STREAM")) {
            // 安全约束：跳过 state 数组的 STREAM
            if (cmd.type == "STREAM" && VD->getNameAsString() == "state") continue;

            std::stringstream ss;
            // 使用变量声明的列号作为缩进
            std::string indent = makeIndent(SM, DS->getBeginLoc());
            ss << "\n" << indent << "#pragma HLS " << cmd.type;

            // ARRAY_PARTITION 需要 variable=
            if (cmd.type == "ARRAY_PARTITION") {
                ss << " variable=" << VD->getNameAsString();
            }
            for (const auto &p : cmd.params)
                ss << " " << p.first << "=" << p.second;

            SourceLocation endLoc = DS->getEndLoc();
            SourceLocation afterSemi = Lexer::findLocationAfterToken(
                endLoc, tok::semi, SM, TheRewriter.getLangOpts(), false);
            if (afterSemi.isValid()) {
                TheRewriter.InsertText(afterSemi, ss.str(), true, true);
            }
        }
    }
    return true;
}

bool PragmaInserterVisitor::VisitForStmt(ForStmt *FS) {
    SourceManager &SM = TheRewriter.getSourceMgr();
    unsigned line = SM.getSpellingLineNumber(FS->getBeginLoc());

    for (const auto &cmd : BestSolution) {
        if (cmd.line == line && (cmd.type == "PIPELINE" || cmd.type == "UNROLL")) {
            std::stringstream ss;
            std::string indent = makeIndent(SM, FS->getBeginLoc());
            ss << indent << "#pragma HLS " << cmd.type;
            for (const auto &p : cmd.params)
                ss << " " << p.first << "=" << p.second;
            ss << "\n";
            TheRewriter.InsertTextBefore(FS->getBeginLoc(), ss.str());
        }
    }
    return true;
}

// 以前把 INLINE 插在调用点；现在不再需要。
// 若仍保留该访问信息，可保留为空实现避免重复插入。
bool PragmaInserterVisitor::VisitCallExpr(CallExpr *CE) {
    SourceManager &SM = TheRewriter.getSourceMgr();
    unsigned call_line = SM.getSpellingLineNumber(CE->getBeginLoc());

    // 如果没有直达被调函数就跳过
    FunctionDecl *Callee = CE->getDirectCallee();
    if (!Callee || !Callee->hasBody() || !isa<CompoundStmt>(Callee->getBody()))
        return true;

    // 找到与“调用点行号”匹配的 INLINE 命令
    for (const auto &cmd : BestSolution) {
        if (cmd.type != "INLINE") continue;
        if (cmd.line != call_line) continue;

        // 只处理 mode=on（或未指定）
        auto it = cmd.params.find("mode");
        if (it != cmd.params.end() && it->second != "on") continue;

        // 在被调函数体内插入 #pragma HLS INLINE （左花括号后一行）
        CompoundStmt *Body = cast<CompoundStmt>(Callee->getBody());
        SourceLocation LBrace = Body->getLBracLoc();
        SourceLocation AfterBrace = LBrace.getLocWithOffset(1);

        // 使用函数体的缩进（在 '{' 的行缩进基础上再缩四个空格）
        std::string indent = "    ";
        std::string pragma = "\n" + indent + "#pragma HLS INLINE\n";
        TheRewriter.InsertText(AfterBrace, pragma, true, true);
    }
    return true;
}
