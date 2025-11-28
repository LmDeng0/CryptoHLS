/*
#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <fstream>
#include <limits>
#include <random>
#include <cstdio>
#include <array>
#include <sstream>

// 核心模块头文件
#include "Core.h"
#include "DesignSpaceVisitor.h"
#include "IRAnalyzer.h"
#include "DSE.h"
#include "PragmaInserter.h"
#include "IRGenerator.h"
#include "AnalysisPasses.h"
#include "GraphExtractor.h"

// Clang/LLVM 核心头文件
#include "clang/AST/ASTConsumer.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "clang/Tooling/CompilationDatabase.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/IR/LegacyPassManager.h"

using namespace clang;
using namespace clang::tooling;
using namespace llvm;

// ... (所有 DesignSpace 和 PragmaInserter 的类定义保持不变) ...
class DesignSpaceConsumer : public ASTConsumer {
public:
    explicit DesignSpaceConsumer(ASTContext *Context, std::string &TopName, DesignSpace &DS)
        : Visitor(Context, TopName), finalDesignSpace(DS) {}
    void HandleTranslationUnit(ASTContext &Context) override {
        Visitor.TraverseDecl(Context.getTranslationUnitDecl());
        finalDesignSpace = Visitor.getDesignSpace();
    }
private:
    DesignSpaceVisitor Visitor;
    DesignSpace &finalDesignSpace;
};
class DesignSpaceAction : public ASTFrontendAction {
public:
    explicit DesignSpaceAction(DesignSpace& DS, std::string& topName)
        : finalDesignSpace(DS), TopFunctionName(topName) {}
protected:
    std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI, StringRef file) override {
        return std::make_unique<DesignSpaceConsumer>(&CI.getASTContext(), TopFunctionName, finalDesignSpace);
    }
private:
    DesignSpace& finalDesignSpace;
    std::string& TopFunctionName;
};
class PragmaInserterConsumer : public ASTConsumer {
public:
    explicit PragmaInserterConsumer(ASTContext *Context, const Solution &S)
        : Visitor(TheRewriter, S) {
        TheRewriter.setSourceMgr(Context->getSourceManager(), Context->getLangOpts());
    }
    void HandleTranslationUnit(ASTContext &Context) override {
        Visitor.TraverseDecl(Context.getTranslationUnitDecl());
    }
    void writeToFile(const std::string &path) {
        const RewriteBuffer *RewriteBuf = TheRewriter.getRewriteBufferFor(TheRewriter.getSourceMgr().getMainFileID());
        if (RewriteBuf) {
            std::error_code EC;
            llvm::raw_fd_ostream out(path, EC);
            out << std::string(RewriteBuf->begin(), RewriteBuf->end());
            llvm::outs() << "[SUCCESS] Generated optimized file: " << path << "\n";
        } else {
            llvm::errs() << "[ERROR] Failed to get rewrite buffer for file.\n";
        }
    }
private:
    Rewriter TheRewriter;
    PragmaInserterVisitor Visitor;
};
class PragmaInserterAction : public ASTFrontendAction {
public:
    explicit PragmaInserterAction(const Solution &S) : BestSolution(S) {}
    void EndSourceFileAction() override {
        const SourceManager& SM = getCompilerInstance().getSourceManager();
        std::string outputFilename = SM.getBufferOrFake(SM.getMainFileID()).getBufferIdentifier().str();
        size_t dot_pos = outputFilename.rfind('.');
        if (dot_pos!= std::string::npos) {
            outputFilename.insert(dot_pos, ".optimized");
        } else {
            outputFilename += ".optimized";
        }
        ConsumerPtr->writeToFile(outputFilename);
    }
protected:
    std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI, StringRef file) override {
        auto Consumer = std::make_unique<PragmaInserterConsumer>(&CI.getASTContext(), BestSolution);
        ConsumerPtr = Consumer.get();
        return Consumer;
    }
private:
    const Solution &BestSolution;
    PragmaInserterConsumer* ConsumerPtr = nullptr;
};
class PragmaInserterActionFactory : public FrontendActionFactory {
public:
    explicit PragmaInserterActionFactory(const Solution &S) : BestSolution(S) {}
    std::unique_ptr<FrontendAction> create() override {
        return std::make_unique<PragmaInserterAction>(BestSolution);
    }
private:
    const Solution &BestSolution;
};


// --- 命令行选项 ---
static cl::OptionCategory AnalysisCategory("CryptoHLS Analysis Engine Options");
static cl::opt<std::string> TopFunctionName("top", cl::desc("Specify the top-level function for synthesis"), cl::Required, cl::cat(AnalysisCategory));
static cl::opt<int> DSEIterations("dse-iterations", cl::desc("Number of DSE iterations for optimization"), cl::init(20), cl::cat(AnalysisCategory));
static cl::opt<double> PerfWeight("perf-weight", cl::desc("Weight for performance score in DSE"), cl::init(0.7), cl::cat(AnalysisCategory));
// [关键新增] 新的命令行标志
static cl::opt<bool> DataCollectionMode("data-collection-mode", cl::desc("Run in data collection mode"), cl::init(false), cl::cat(AnalysisCategory));


// --- 主函数 ---
int main(int argc, const char **argv) {
    auto ExpectedParser = CommonOptionsParser::create(argc, argv, AnalysisCategory, cl::ZeroOrMore);
    if (!ExpectedParser) { errs() << ExpectedParser.takeError(); return 1; }
    CommonOptionsParser& OptionsParser = ExpectedParser.get();
    if (OptionsParser.getSourcePathList().empty()) { errs() << "Error: No input source file specified.\n"; return 1; }
    
    std::string TopFunctionNameValue = TopFunctionName.getValue();
    std::string InputFilename = OptionsParser.getSourcePathList()[0];
    
    FixedCompilationDatabase Compilations(".", OptionsParser.getExtraArgs());
    ClangTool Tool(Compilations, OptionsParser.getSourcePathList());

    // --- 步骤 1: AST分析，构建设计空间 ---
    std::cout << "[INFO] Step 1: Building Design Space from source...\n";
    DesignSpace designSpace;
    auto ActionFactory = newFrontendActionFactory<DesignSpaceAction>(designSpace, TopFunctionNameValue);

    if (Tool.run(ActionFactory.get()) != 0) {
        errs() << "[ERROR] Failed to run DesignSpaceAction.\n";
        return 1;
    }
    std::cout << "[INFO]    => Found " << designSpace.size() << " optimization points.\n";

    // --- 步骤 2: IR生成与分析 ---
    std::cout << "[INFO] Step 2: Generating and analyzing base IR...\n";
    CryptoHLS::IRGenerator ir_generator(InputFilename, "build", OptionsParser.getExtraArgs());
    if (!ir_generator.generate()) { errs() << "[ERROR] Failed to generate IR.\n"; return 1; }
    
    std::string base_ir_file = ir_generator.getIRFilePath();
    LLVMContext context;
    SMDiagnostic err;
    std::unique_ptr<Module> llvm_module = parseIRFile(base_ir_file, err, context);
    if (!llvm_module) { errs() << "[ERROR] Failed to parse IR file.\n"; return 1; }

    std::cout << "[INFO] Step 2.5: Running Security Analysis Passes...\n";
    legacy::PassManager pm;
    pm.add(new CryptoHLS::CryptoSemanticAnalysisPass());
    pm.add(new CryptoHLS::SecurityScanPass());
    pm.run(*llvm_module);
    
    std::cout << "[INFO] Step 2.6: Extracting Graph Representation...\n";
    GraphExtractor graph_extractor(llvm_module.get());
    GraphData code_graph = graph_extractor.extract(TopFunctionNameValue);
    graph_extractor.saveToFile(code_graph, "build/graph_features");

    // [关键修改] 根据模式选择不同的执行路径
    if (DataCollectionMode) {
        // --- 数据收集模式 ---
        std::cout << "\n[INFO] Running in Data Collection Mode.\n";
        std::mt19937 rng(std::random_device{}());
        Solution random_solution = generateRandomSolution(designSpace, rng);
        
        std::cout << "[INFO] Generating command file for random solution...\n";
        std::ofstream cmd_out("build/commands.txt");
        for(const auto& cmd : random_solution) {
            cmd_out << cmd.line << ":" << cmd.type;
            for (const auto& p : cmd.params) { cmd_out << " " << p.first << "=" << p.second; }
            cmd_out << "\n";
        }
        cmd_out.close();
        
        std::cout << "[INFO] Generating optimized C++ file for random solution...\n";
        ClangTool PragmaTool(Compilations, OptionsParser.getSourcePathList());
        PragmaInserterActionFactory PragmaFactory(random_solution);
        if (PragmaTool.run(&PragmaFactory) != 0) {
            errs() << "[ERROR] Failed to generate optimized file for data collection.\n";
        }

    } else {
        // --- 默认的DSE优化模式 ---
        FunctionCost base_cost = IRAnalyzer::analyzeModule(llvm_module.get(), TopFunctionNameValue);
        std::cout << "[INFO] Base performance score: " << base_cost.performance_score << ", Base area score: " << base_cost.area_score << "\n";
        
        CostModel cost_model(base_cost);
        std::mt19937 rng(std::random_device{}());
        Solution best_solution;
        double best_total_score = std::numeric_limits<double>::max();
        
        std::cout << "\n[INFO] Step 3: Starting in-memory DSE...\n";
        for (int i = 0; i < DSEIterations.getValue(); ++i) {
            // ... (DSE 循环保持不变)
            Solution current_solution = generateRandomSolution(designSpace, rng);
            FunctionCost predicted_cost = cost_model.predictCost(current_solution);
            double total_score = PerfWeight.getValue() * predicted_cost.performance_score + (1.0 - PerfWeight.getValue()) * predicted_cost.area_score;
            if (total_score < best_total_score) {
                best_total_score = total_score;
                best_solution = current_solution;
            }
        }
        
        std::cout << "\n[INFO] DSE Finished.\n";
        
        std::cout << "[INFO] Step 5: Generating optimized C++ source file...\n";
        ClangTool PragmaTool(Compilations, OptionsParser.getSourcePathList());
        PragmaInserterActionFactory PragmaFactory(best_solution);
        if (PragmaTool.run(&PragmaFactory)!= 0) {
            errs() << "[ERROR] Failed to run PragmaInserterAction.\n";
        }
    }
    
    std::remove(base_ir_file.c_str());
    std::remove(ir_generator.getBitcodeFilePath().c_str());
    return 0;
}


*/


// #include <iostream>
// #include <string>
// #include <vector>
// #include <memory>
// #include <fstream>
// #include <limits>
// #include <random>
// #include <cstdio>
// #include <array>
// #include <sstream>

// // 核心模块头文件
// #include "Core.h"
// #include "DesignSpaceVisitor.h"
// #include "IRAnalyzer.h"
// #include "DSE.h"
// #include "PragmaInserter.h"
// #include "IRGenerator.h"
// #include "AnalysisPasses.h"
// #include "GraphExtractor.h"

// // Clang/LLVM 核心头文件
// #include "clang/AST/ASTConsumer.h"
// #include "clang/Frontend/CompilerInstance.h"
// #include "clang/Frontend/FrontendActions.h"
// #include "clang/Tooling/CommonOptionsParser.h"
// #include "clang/Tooling/Tooling.h"
// #include "clang/Tooling/CompilationDatabase.h"
// #include "clang/Rewrite/Core/Rewriter.h"
// #include "llvm/Support/CommandLine.h"
// #include "llvm/IR/LLVMContext.h"
// #include "llvm/IR/Module.h"
// #include "llvm/IRReader/IRReader.h"
// #include "llvm/Support/SourceMgr.h"
// #include "llvm/IR/LegacyPassManager.h"

// using namespace clang;
// using namespace clang::tooling;
// using namespace llvm;

// // --- [关键修正] 第一部分: 采用更稳健的AST分析与数据所有权模型 ---

// // DesignSpaceConsumer 保持不变
// class DesignSpaceConsumer : public ASTConsumer {
// public:
//     explicit DesignSpaceConsumer(ASTContext *Context, std::string &TopName) : Visitor(Context, TopName) {}
//     void HandleTranslationUnit(ASTContext &Context) override { Visitor.TraverseDecl(Context.getTranslationUnitDecl()); }
//     const DesignSpace& getResults() const { return Visitor.getDesignSpace(); }
// private:
//     DesignSpaceVisitor Visitor;
// };

// // 前向声明
// class DesignSpaceActionFactory;

// class DesignSpaceAction : public ASTFrontendAction {
// public:
//     explicit DesignSpaceAction(DesignSpaceActionFactory* Factory);
//     void EndSourceFileAction() override;
// protected:
//     std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI, StringRef file) override;
// private:
//     DesignSpaceActionFactory* FactoryPtr;
//     DesignSpaceConsumer* ConsumerPtr = nullptr;
// };

// class DesignSpaceActionFactory : public FrontendActionFactory {
// public:
//     explicit DesignSpaceActionFactory(std::string &TopName) : TopFunctionName(TopName) {}
//     std::unique_ptr<FrontendAction> create() override {
//         return std::make_unique<DesignSpaceAction>(this);
//     }
//     const DesignSpace& getResults() const { return finalDesignSpace; }
//     void setResults(const DesignSpace& results) { finalDesignSpace = results; }
//     std::string& getTopFunctionNameRef() { return TopFunctionName; }
// private:
//     std::string TopFunctionName;
//     DesignSpace finalDesignSpace;
// };

// DesignSpaceAction::DesignSpaceAction(DesignSpaceActionFactory* Factory) : FactoryPtr(Factory) {}

// void DesignSpaceAction::EndSourceFileAction() {
//     if (ConsumerPtr && FactoryPtr) {
//         FactoryPtr->setResults(ConsumerPtr->getResults());
//     }
// }

// std::unique_ptr<ASTConsumer> DesignSpaceAction::CreateASTConsumer(CompilerInstance &CI, StringRef file) {
//     auto Consumer = std::make_unique<DesignSpaceConsumer>(&CI.getASTContext(), FactoryPtr->getTopFunctionNameRef());
//     ConsumerPtr = Consumer.get();
//     return Consumer;
// }


// // --- 第二、三、四部分的代码保持不变 ---
// class PragmaInserterConsumer : public ASTConsumer {
// public:
//     explicit PragmaInserterConsumer(ASTContext *Context, const Solution &S)
//         : Visitor(TheRewriter, S) {
//         TheRewriter.setSourceMgr(Context->getSourceManager(), Context->getLangOpts());
//     }
//     void HandleTranslationUnit(ASTContext &Context) override {
//         Visitor.TraverseDecl(Context.getTranslationUnitDecl());
//     }
//     void writeToFile(const std::string &path) {
//         const RewriteBuffer *RewriteBuf = TheRewriter.getRewriteBufferFor(TheRewriter.getSourceMgr().getMainFileID());
//         if (RewriteBuf) {
//             std::error_code EC;
//             llvm::raw_fd_ostream out(path, EC);
//             out << std::string(RewriteBuf->begin(), RewriteBuf->end());
//             llvm::outs() << " Generated optimized file: " << path << "\n";
//         } else {
//             llvm::errs() << " Failed to get rewrite buffer for file.\n";
//         }
//     }
// private:
//     Rewriter TheRewriter;
//     PragmaInserterVisitor Visitor;
// };
// class PragmaInserterAction : public ASTFrontendAction {
// public:
//     explicit PragmaInserterAction(const Solution &S) : BestSolution(S) {}
//     void EndSourceFileAction() override {
//         const SourceManager& SM = getCompilerInstance().getSourceManager();
//         std::string outputFilename = SM.getBufferOrFake(SM.getMainFileID()).getBufferIdentifier().str();
//         size_t dot_pos = outputFilename.rfind('.');
//         if (dot_pos!= std::string::npos) {
//             outputFilename.insert(dot_pos, ".optimized");
//         } else {
//             outputFilename += ".optimized";
//         }
//         ConsumerPtr->writeToFile(outputFilename);
//     }
// protected:
//     std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI, StringRef file) override {
//         auto Consumer = std::make_unique<PragmaInserterConsumer>(&CI.getASTContext(), BestSolution);
//         ConsumerPtr = Consumer.get();
//         return Consumer;
//     }
// private:
//     const Solution &BestSolution;
//     PragmaInserterConsumer* ConsumerPtr = nullptr;
// };
// class PragmaInserterActionFactory : public FrontendActionFactory {
// public:
//     explicit PragmaInserterActionFactory(const Solution &S) : BestSolution(S) {}
//     std::unique_ptr<FrontendAction> create() override {
//         return std::make_unique<PragmaInserterAction>(BestSolution);
//     }
// private:
//     const Solution &BestSolution;
// };

// static cl::OptionCategory AnalysisCategory("CryptoHLS Analysis Engine Options");
// static cl::opt<std::string> TopFunctionName("top", cl::desc("Specify the top-level function for synthesis"), cl::Required, cl::cat(AnalysisCategory));
// static cl::opt<int> DSEIterations("dse-iterations", cl::desc("Number of DSE iterations for optimization"), cl::init(20), cl::cat(AnalysisCategory));
// static cl::opt<double> PerfWeight("perf-weight", cl::desc("Weight for performance score in DSE"), cl::init(0.7), cl::cat(AnalysisCategory));
// static cl::opt<bool> DataCollectionMode("data-collection-mode", cl::desc("Run in data collection mode"), cl::init(false), cl::cat(AnalysisCategory));

// int main(int argc, const char **argv) {
//     auto ExpectedParser = CommonOptionsParser::create(argc, argv, AnalysisCategory, cl::ZeroOrMore);
//     if (!ExpectedParser) { errs() << ExpectedParser.takeError(); return 1; }
//     CommonOptionsParser& OptionsParser = ExpectedParser.get();
//     if (OptionsParser.getSourcePathList().empty()) { errs() << "Error: No input source file specified.\n"; return 1; }
    
//     std::string TopFunctionNameValue = TopFunctionName.getValue();
//     std::string InputFilename = OptionsParser.getSourcePathList()[0];
//     ClangTool Tool(OptionsParser.getCompilations(), OptionsParser.getSourcePathList());

//     // --- 步骤 1: AST分析，构建设计空间 ---
//     std::cout << "[INFO] Step 1: Building Design Space from source...\n";
//     DesignSpaceActionFactory Factory(TopFunctionNameValue);
//     if (Tool.run(&Factory) != 0) { 
//         errs() << " Failed to run DesignSpaceAction.\n"; 
//         return 1; 
//     }
//     const DesignSpace& designSpace = Factory.getResults();
//     std::cout << "[INFO]    => Found " << designSpace.size() << " optimization points.\n";

//     // --- 步骤 2: IR生成与分析 ---
    
//     std::cout << "[INFO] Step 2: Generating and analyzing base IR...\n";
//     CryptoHLS::IRGenerator ir_generator(InputFilename, "build", OptionsParser.getExtraArgs());
//     if (!ir_generator.generate()) { errs() << "[ERROR] Failed to generate IR.\n"; return 1; }
    
//     std::string base_ir_file = ir_generator.getIRFilePath();
//     LLVMContext context;
//     SMDiagnostic err;
//     std::unique_ptr<llvm::Module> llvm_module = parseIRFile(base_ir_file, err, context);
//     if (!llvm_module) { errs() << "[ERROR] Failed to parse IR file.\n"; return 1; }

//     std::cout << "[INFO] Step 2.5: Running Security Analysis Passes...\n";
//     legacy::PassManager pm;
//     pm.add(new CryptoHLS::CryptoSemanticAnalysisPass());
//     pm.add(new CryptoHLS::SecurityScanPass());
//     pm.run(*llvm_module);
    
//     std::cout << "[INFO] Step 2.6: Extracting Graph Representation...\n";
//     GraphExtractor graph_extractor(llvm_module.get());
//     GraphData code_graph = graph_extractor.extract(TopFunctionNameValue);
//     graph_extractor.saveToFile(code_graph, "build/graph_features");

//     if (DataCollectionMode) {
//         std::cout << "\n[INFO] Running in Data Collection Mode.\n";
//         std::mt19937 rng(std::random_device{}());
//         Solution random_solution = generateRandomSolution(designSpace, rng);
        
//         std::cout << "[INFO] Generating command file for random solution...\n";
//         std::ofstream cmd_out("build/commands.txt");
//         for(const auto& cmd : random_solution) {
//             cmd_out << cmd.line << ":" << cmd.type;
//             for (const auto& p : cmd.params) { cmd_out << " " << p.first << "=" << p.second; }
//             cmd_out << "\n";
//         }
//         cmd_out.close();
        
//         std::cout << "[INFO] Generating optimized C++ file for random solution...\n";
//         ClangTool PragmaTool(OptionsParser.getCompilations(), OptionsParser.getSourcePathList());
//         PragmaInserterActionFactory PragmaFactory(random_solution);
//         if (PragmaTool.run(&PragmaFactory) != 0) {
//             errs() << "[ERROR] Failed to generate optimized file for data collection.\n";
//         }

//     } else {
//         FunctionCost base_cost = IRAnalyzer::analyzeModule(llvm_module.get(), TopFunctionNameValue);
//         std::cout << "[INFO] Base performance score: " << base_cost.performance_score << ", Base area score: " << base_cost.area_score << "\n";
        
//         CostModel cost_model(base_cost);
//         std::mt19937 rng(std::random_device{}());
//         Solution best_solution;
//         double best_total_score = std::numeric_limits<double>::max();
        
//         std::cout << "\n[INFO] Step 3: Starting in-memory DSE...\n";
//         for (int i = 0; i < DSEIterations.getValue(); ++i) {
//             Solution current_solution = generateRandomSolution(designSpace, rng);
//             FunctionCost predicted_cost = cost_model.predictCost(current_solution);
//             double total_score = PerfWeight.getValue() * predicted_cost.performance_score + (1.0 - PerfWeight.getValue()) * predicted_cost.area_score;
//             if (total_score < best_total_score) {
//                 best_total_score = total_score;
//                 best_solution = current_solution;
//             }
//         }
        
//         std::cout << "\n[INFO] DSE Finished.\n";
        
//         std::cout << "[INFO] Step 5: Generating optimized C++ source file...\n";
//         ClangTool PragmaTool(OptionsParser.getCompilations(), OptionsParser.getSourcePathList());
//         PragmaInserterActionFactory PragmaFactory(best_solution);
//         if (PragmaTool.run(&PragmaFactory)!= 0) {
//             errs() << "[ERROR] Failed to run PragmaInserterAction.\n";
//         }
//     }
    
//     std::remove(base_ir_file.c_str());
//     std::remove(ir_generator.getBitcodeFilePath().c_str());
//     return 0;
// }

// #include <iostream>
// #include <string>
// #include <vector>
// #include <memory>
// #include <fstream>
// #include <limits>
// #include <random>
// #include <cstdio> // 用于 popen
// #include <array>
// #include <sstream>

// // 核心模块头文件
// #include "Core.h"
// #include "DesignSpaceVisitor.h"
// #include "IRAnalyzer.h"
// #include "DSE.h"
// #include "PragmaInserter.h"
// #include "IRGenerator.h"
// #include "AnalysisPasses.h"
// #include "GraphExtractor.h"

// // Clang/LLVM 核心头文件
// #include "clang/AST/ASTConsumer.h"
// #include "clang/Frontend/CompilerInstance.h"
// #include "clang/Frontend/FrontendActions.h"
// #include "clang/Tooling/CommonOptionsParser.h"
// #include "clang/Tooling/Tooling.h"
// #include "clang/Tooling/CompilationDatabase.h"
// #include "clang/Rewrite/Core/Rewriter.h"
// #include "llvm/Support/CommandLine.h"
// #include "llvm/IR/LLVMContext.h"
// #include "llvm/IR/Module.h"
// #include "llvm/IRReader/IRReader.h"
// #include "llvm/Support/SourceMgr.h"
// #include "llvm/IR/LegacyPassManager.h"
// #include "llvm/Support/Program.h"
// #include "llvm/Support/FileSystem.h"

// using namespace clang;
// using namespace clang::tooling;
// using namespace llvm;

// // --- 自动探测系统头文件路径的辅助函数 ---
// std::vector<std::string> getSystemIncludePaths() {
//     std::vector<std::string> paths;
//     std::shared_ptr<FILE> pipe(popen("clang++ -E -x c++ - -v < /dev/null 2>&1", "r"), pclose);
//     if (!pipe) return paths;
//     std::array<char, 128> buffer;
//     std::string result;
//     while (fgets(buffer.data(), buffer.size(), pipe.get())!= nullptr) { result += buffer.data(); }
//     std::string start_marker = "#include <...> search starts here:";
//     std::string end_marker = "End of search list.";
//     size_t start_pos = result.find(start_marker);
//     if (start_pos == std::string::npos) return paths;
//     start_pos += start_marker.length();
//     size_t end_pos = result.find(end_marker, start_pos);
//     if (end_pos == std::string::npos) return paths;
//     std::string paths_str = result.substr(start_pos, end_pos - start_pos);
//     std::stringstream ss(paths_str);
//     std::string path;
//     while (std::getline(ss, path)) {
//         size_t first = path.find_first_not_of(" \n\r\t");
//         if (std::string::npos == first) continue;
//         size_t last = path.find_last_not_of(" \n\r\t");
//         std::string clean_path = path.substr(first, (last - first + 1));
//         if (sys::fs::is_directory(clean_path)) {
//             paths.push_back(clean_path);
//         }
//     }
//     return paths;
// }

// // --- 第一部分: AST分析相关的类定义 ---
// class DesignSpaceConsumer : public ASTConsumer {
// public:
//     explicit DesignSpaceConsumer(ASTContext *Context, std::string &TopName, DesignSpace &DS)
//         : Visitor(Context, TopName), finalDesignSpace(DS) {}
//     void HandleTranslationUnit(ASTContext &Context) override {
//         Visitor.TraverseDecl(Context.getTranslationUnitDecl());
//         finalDesignSpace = Visitor.getDesignSpace();
//     }
// private:
//     DesignSpaceVisitor Visitor;
//     DesignSpace &finalDesignSpace;
// };

// class DesignSpaceAction : public ASTFrontendAction {
// public:
//     explicit DesignSpaceAction(DesignSpace& DS, std::string& topName)
//         : finalDesignSpace(DS), TopFunctionName(topName) {}
// protected:
//     std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI, StringRef file) override {
//         return std::make_unique<DesignSpaceConsumer>(&CI.getASTContext(), TopFunctionName, finalDesignSpace);
//     }
// private:
//     DesignSpace& finalDesignSpace;
//     std::string& TopFunctionName;
// };

// // [关键修正] 自定义的 FrontendActionFactory，用于向 Action 传递参数
// class DesignSpaceActionFactory : public FrontendActionFactory {
// public:
//     // 构造函数接收 main 函数中的数据，并保存为成员变量的引用
//     DesignSpaceActionFactory(DesignSpace& DS, std::string& topName)
//         : finalDesignSpace(DS), TopFunctionName(topName) {}

//     // Tool.run() 会调用此方法来创建一个 Action
//     std::unique_ptr<FrontendAction> create() override {
//         // 在这里创建我们的 Action，并把保存的数据传递给它
//         return std::make_unique<DesignSpaceAction>(finalDesignSpace, TopFunctionName);
//     }

// private:
//     DesignSpace& finalDesignSpace;
//     std::string& TopFunctionName;
// };


// // --- 第二部分: 代码生成相关的类定义 (保持不变) ---
// class PragmaInserterConsumer : public ASTConsumer {
// public:
//     explicit PragmaInserterConsumer(ASTContext *Context, const Solution &S)
//         : Visitor(TheRewriter, S) {
//         TheRewriter.setSourceMgr(Context->getSourceManager(), Context->getLangOpts());
//     }
//     void HandleTranslationUnit(ASTContext &Context) override {
//         Visitor.TraverseDecl(Context.getTranslationUnitDecl());
//     }
//     void writeToFile(const std::string &path) {
//         const RewriteBuffer *RewriteBuf = TheRewriter.getRewriteBufferFor(TheRewriter.getSourceMgr().getMainFileID());
//         if (RewriteBuf) {
//             std::error_code EC;
//             llvm::raw_fd_ostream out(path, EC);
//             out << std::string(RewriteBuf->begin(), RewriteBuf->end());
//             llvm::outs() << " Generated optimized file: " << path << "\n";
//         } else {
//             llvm::errs() << " Failed to get rewrite buffer for file.\n";
//         }
//     }
// private:
//     Rewriter TheRewriter;
//     PragmaInserterVisitor Visitor;
// };
// class PragmaInserterAction : public ASTFrontendAction {
// public:
//     explicit PragmaInserterAction(const Solution &S) : BestSolution(S) {}
//     void EndSourceFileAction() override {
//         const SourceManager& SM = getCompilerInstance().getSourceManager();
//         std::string outputFilename = SM.getBufferOrFake(SM.getMainFileID()).getBufferIdentifier().str();
//         size_t dot_pos = outputFilename.rfind('.');
//         if (dot_pos!= std::string::npos) {
//             outputFilename.insert(dot_pos, ".optimized");
//         } else {
//             outputFilename += ".optimized";
//         }
//         ConsumerPtr->writeToFile(outputFilename);
//     }
// protected:
//     std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI, StringRef file) override {
//         auto Consumer = std::make_unique<PragmaInserterConsumer>(&CI.getASTContext(), BestSolution);
//         ConsumerPtr = Consumer.get();
//         return Consumer;
//     }
// private:
//     const Solution &BestSolution;
//     PragmaInserterConsumer* ConsumerPtr = nullptr;
// };
// class PragmaInserterActionFactory : public FrontendActionFactory {
// public:
//     explicit PragmaInserterActionFactory(const Solution &S) : BestSolution(S) {}
//     std::unique_ptr<FrontendAction> create() override {
//         return std::make_unique<PragmaInserterAction>(BestSolution);
//     }
// private:
//     const Solution &BestSolution;
// };


// // --- 第三部分: 命令行选项定义 ---
// static cl::OptionCategory AnalysisCategory("CryptoHLS Analysis Engine Options");
// static cl::opt<std::string> TopFunctionName("top", cl::desc("Specify the top-level function for synthesis"), cl::Required, cl::cat(AnalysisCategory));
// static cl::opt<int> DSEIterations("dse-iterations", cl::desc("Number of DSE iterations for optimization"), cl::init(20), cl::cat(AnalysisCategory));
// static cl::opt<double> PerfWeight("perf-weight", cl::desc("Weight for performance score in DSE"), cl::init(0.7), cl::cat(AnalysisCategory));
// static cl::opt<bool> DataCollectionMode("data-collection-mode", cl::desc("Run in data collection mode"), cl::init(false), cl::cat(AnalysisCategory));


// // --- 第四部分: 主函数 ---
// int main(int argc, const char **argv) {
//     auto ExpectedParser = CommonOptionsParser::create(argc, argv, AnalysisCategory, cl::ZeroOrMore);
//     if (!ExpectedParser) { errs() << ExpectedParser.takeError(); return 1; }
//     CommonOptionsParser& OptionsParser = ExpectedParser.get();
//     if (OptionsParser.getSourcePathList().empty()) { errs() << "Error: No input source file specified.\n"; return 1; }

//     std::string TopFunctionNameValue = TopFunctionName.getValue();
//     // --- [关键修正] ---
//     // getSourcePathList() 返回一个vector，我们取第一个元素
//     std::string InputFilename = OptionsParser.getSourcePathList()[0];
//     // --- [修正结束] ---
    
     
//     // 动态构建编译数据库
//     auto clangPathOrErr = sys::findProgramByName("clang++");
//     if (!clangPathOrErr) {
//         errs() << "Error: clang++ not found in PATH. Cannot proceed.\n";
//         return 1;
//     }
//     std::string clangPath = *clangPathOrErr;

//     std::vector<std::string> command_line;
//     command_line.push_back(clangPath);
//     command_line.push_back("-std=c++17");
//     std::vector<std::string> system_includes = getSystemIncludePaths();
//     for (const auto& path : system_includes) {
//         command_line.push_back("-isystem");
//         command_line.push_back(path);
//     }
     
//     FixedCompilationDatabase Compilations(".", command_line);
//     ClangTool Tool(Compilations, OptionsParser.getSourcePathList());

//     std::cout << "[INFO] Step 1: Building Design Space from source...\n";
//     DesignSpace designSpace;
     
//     DesignSpaceActionFactory ActionFactory(designSpace, TopFunctionNameValue);

//     if (Tool.run(&ActionFactory)!= 0) {
//         errs() << " Failed to run DesignSpaceAction. Source code may have errors.\n";
//         return 1;
//     }
//     std::cout << "[INFO]     => Found " << designSpace.size() << " optimization points.\n";

//     std::cout << "[INFO] Step 2: Generating and analyzing base IR...\n";
//     CryptoHLS::IRGenerator ir_generator(InputFilename, "build", system_includes);
//     if (!ir_generator.generate()) { errs() << " Failed to generate IR.\n"; return 1; }

//     std::string base_ir_file = ir_generator.getIRFilePath();
//     LLVMContext context;
//     SMDiagnostic err;
//     std::unique_ptr<llvm::Module> llvm_module = parseIRFile(base_ir_file, err, context);
//     if (!llvm_module) { errs() << " Failed to parse IR file.\n"; return 1; }

//     std::cout << "\n[INFO] Step 2.5: Running Security Analysis Passes...\n";
//     legacy::PassManager pm;
//     pm.add(new CryptoHLS::CryptoSemanticAnalysisPass());
//     pm.add(new CryptoHLS::SecurityScanPass());
//     pm.run(*llvm_module);

//     std::cout << "[INFO] Step 2.6: Extracting Graph Representation...\n";
//     GraphExtractor graph_extractor(llvm_module.get());
//     GraphData code_graph = graph_extractor.extract(TopFunctionNameValue);
//     std::string graph_nodes_path = "build/graph_features_nodes.csv";
//     std::string graph_edges_path = "build/graph_features_edges.csv";
//     graph_extractor.saveToFile(code_graph, "build/graph_features");


//     if (DataCollectionMode) {
//         /*****************************************************************/
//         /*              [新功能] 数据收集模式实现                        */
//         /*****************************************************************/
//         std::cout << "\n[INFO] Step 3: Starting DSE in Data Collection Mode...\n";
//         std::mt19937 rng(std::random_device{}());

//         for (int i = 0; i < DSEIterations.getValue(); ++i) {
//             std::cout << "\n--- Iteration " << i + 1 << "/" << DSEIterations.getValue() << " ---\n";
            
//             Solution current_solution = generateRandomSolution(designSpace, rng);
//             if (current_solution.empty()) {
//                 std::cout << "[INFO] Skipping empty solution.\n";
//                 continue;
//             }

//             std::string optimized_cpp_path;
//             {
//                 PragmaInserterActionFactory PragmaFactory(current_solution);
//                 ClangTool TempTool(Compilations, OptionsParser.getSourcePathList());
//                 if (TempTool.run(&PragmaFactory)!= 0) {
//                     errs() << " Failed to run PragmaInserterAction for data collection.\n";
//                     continue;
//                 }
//                 size_t dot_pos = InputFilename.rfind('.');
//                 if (dot_pos!= std::string::npos) {
//                     optimized_cpp_path = InputFilename.substr(0, dot_pos) + ".optimized.cpp";
//                 } else {
//                     optimized_cpp_path = InputFilename + ".optimized.cpp";
//                 }
//             }
//              std::cout << "[INFO] Generated temporary optimized file: " << optimized_cpp_path << "\n";

//             std::stringstream ss;
//             ss << "[";
//             for (auto it = current_solution.begin(); it!= current_solution.end(); ++it) {
//                 const auto& cmd = *it;
//                 ss << "{";
//                 ss << "\"line\":" << cmd.line << ",";
//                 ss << "\"type\":\"" << cmd.type << "\",";
//                 ss << "\"params\":{";
                
//                 for (auto p_it = cmd.params.begin(); p_it!= cmd.params.end(); ++p_it) {
//                     ss << "\"" << p_it->first << "\":\"" << p_it->second << "\"";
//                     if (std::next(p_it)!= cmd.params.end()) {
//                         ss << ",";
//                     }
//                 }
                
//                 ss << "}";
//                 ss << "}";
//                 if (std::next(it)!= current_solution.end()) {
//                     ss << ",";
//                 }
//             }
//             ss << "]";
//             std::string solution_json_str = ss.str();

//             std::string command = "python3  ../hls_runner.py ";
//             command += optimized_cpp_path;
//             command += " --top=" + TopFunctionNameValue;
//             command += " --solution='" + solution_json_str + "'";
//             command += " --graph-nodes=" + graph_nodes_path;
//             command += " --graph-edges=" + graph_edges_path;
//             command += " --dataset-file=../hls_dataset.json";

//             std::cout << "[INFO] Executing data collection command:\n" << command << "\n";
//             int ret = system(command.c_str());

//             if (ret!= 0) {
//                 errs() << " hls_runner.py script failed for the current solution.\n";
//             }
//         }
//         std::cout << "\n[INFO] Data collection finished.\n";

//     } else {
//         FunctionCost base_cost = IRAnalyzer::analyzeModule(llvm_module.get(), TopFunctionNameValue);
//         std::cout << "[INFO] Base performance score: " << base_cost.performance_score << ", Base area score: " << base_cost.area_score << "\n";
         
//         CostModel cost_model(base_cost);
//         std::mt19937 rng(std::random_device{}());
//         Solution best_solution;
//         double best_total_score = std::numeric_limits<double>::max();
         
//         std::cout << "\n[INFO] Step 3: Starting in-memory DSE...\n";
//         for (int i = 0; i < DSEIterations.getValue(); ++i) {
//             Solution current_solution = generateRandomSolution(designSpace, rng);
//             FunctionCost predicted_cost = cost_model.predictCost(current_solution);
//             double total_score = PerfWeight.getValue() * predicted_cost.performance_score + (1.0 - PerfWeight.getValue()) * predicted_cost.area_score;
//             if (total_score < best_total_score) {
//                 best_total_score = total_score;
//                 best_solution = current_solution;
//             }
//         }
         
//         std::cout << "\n[INFO] DSE Finished.\n";
         
//         std::cout << "[INFO] Step 5: Generating optimized C++ source file...\n";
//         if (!best_solution.empty()) {
//             PragmaInserterActionFactory PragmaFactory(best_solution);
//             if (Tool.run(&PragmaFactory)!= 0) {
//                 errs() << " Failed to run PragmaInserterAction.\n";
//             }
//         }
//     }

//     std::remove(base_ir_file.c_str());
//     std::remove(ir_generator.getBitcodeFilePath().c_str());
//     return 0;
// }


// #include <iostream>
// #include <string>
// #include <vector>
// #include <memory>
// #include <fstream>
// #include <limits>
// #include <random>
// #include <cstdio> // 用于 popen
// #include <array>
// #include <sstream>
// #include <unistd.h> // 为了 getpid()

// // 核心模块头文件
// #include "Core.h"
// #include "DesignSpaceVisitor.h"
// #include "IRAnalyzer.h"
// #include "DSE.h"
// #include "PragmaInserter.h"
// #include "IRGenerator.h"
// #include "AnalysisPasses.h"
// #include "GraphExtractor.h"

// // Clang/LLVM 核心头文件
// #include "clang/AST/ASTConsumer.h"
// #include "clang/Frontend/CompilerInstance.h"
// #include "clang/Frontend/FrontendActions.h"
// #include "clang/Tooling/CommonOptionsParser.h"
// #include "clang/Tooling/Tooling.h"
// #include "clang/Tooling/CompilationDatabase.h"
// #include "clang/Rewrite/Core/Rewriter.h"
// #include "llvm/Support/CommandLine.h"
// #include "llvm/IR/LLVMContext.h"
// #include "llvm/IR/Module.h"
// #include "llvm/IRReader/IRReader.h"
// #include "llvm/Support/SourceMgr.h"
// #include "llvm/IR/LegacyPassManager.h"
// #include "llvm/Support/Program.h"
// #include "llvm/Support/FileSystem.h"

// using namespace clang;
// using namespace clang::tooling;
// using namespace llvm;

// // --- 自动探测系统头文件路径的辅助函数 (保持不变) ---
// std::vector<std::string> getSystemIncludePaths() {
//     std::vector<std::string> paths;
//     std::shared_ptr<FILE> pipe(popen("clang++ -E -x c++ - -v < /dev/null 2>&1", "r"), pclose);
//     if (!pipe) return paths;
//     std::array<char, 128> buffer;
//     std::string result;
//     while (fgets(buffer.data(), buffer.size(), pipe.get())!= nullptr) { result += buffer.data(); }
//     std::string start_marker = "#include <...> search starts here:";
//     std::string end_marker = "End of search list.";
//     size_t start_pos = result.find(start_marker);
//     if (start_pos == std::string::npos) return paths;
//     start_pos += start_marker.length();
//     size_t end_pos = result.find(end_marker, start_pos);
//     if (end_pos == std::string::npos) return paths;
//     std::string paths_str = result.substr(start_pos, end_pos - start_pos);
//     std::stringstream ss(paths_str);
//     std::string path;
//     while (std::getline(ss, path)) {
//         size_t first = path.find_first_not_of(" \n\r\t");
//         if (std::string::npos == first) continue;
//         size_t last = path.find_last_not_of(" \n\r\t");
//         std::string clean_path = path.substr(first, (last - first + 1));
//         if (sys::fs::is_directory(clean_path)) {
//             paths.push_back(clean_path);
//         }
//     }
//     return paths;
// }

// // --- 第一部分: AST分析相关的类定义 (保持不变) ---
// class DesignSpaceConsumer : public ASTConsumer {
// public:
//     explicit DesignSpaceConsumer(ASTContext *Context, std::string &TopName, DesignSpace &DS)
//         : Visitor(Context, TopName), finalDesignSpace(DS) {}
//     void HandleTranslationUnit(ASTContext &Context) override {
//         Visitor.TraverseDecl(Context.getTranslationUnitDecl());
//         finalDesignSpace = Visitor.getDesignSpace();
//     }
// private:
//     DesignSpaceVisitor Visitor;
//     DesignSpace &finalDesignSpace;
// };
// class DesignSpaceAction : public ASTFrontendAction {
// public:
//     explicit DesignSpaceAction(DesignSpace& DS, std::string& topName)
//         : finalDesignSpace(DS), TopFunctionName(topName) {}
// protected:
//     std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI, StringRef file) override {
//         return std::make_unique<DesignSpaceConsumer>(&CI.getASTContext(), TopFunctionName, finalDesignSpace);
//     }
// private:
//     DesignSpace& finalDesignSpace;
//     std::string& TopFunctionName;
// };
// class DesignSpaceActionFactory : public FrontendActionFactory {
// public:
//     DesignSpaceActionFactory(DesignSpace& DS, std::string& topName)
//         : finalDesignSpace(DS), TopFunctionName(topName) {}
//     std::unique_ptr<FrontendAction> create() override {
//         return std::make_unique<DesignSpaceAction>(finalDesignSpace, TopFunctionName);
//     }
// private:
//     DesignSpace& finalDesignSpace;
//     std::string& TopFunctionName;
// };


// // --- 第二部分: 代码生成相关的类定义 ---
// class PragmaInserterConsumer : public ASTConsumer {
// public:
//     explicit PragmaInserterConsumer(ASTContext *Context, const Solution &S)
//         : Visitor(TheRewriter, S) {
//         TheRewriter.setSourceMgr(Context->getSourceManager(), Context->getLangOpts());
//     }
//     void HandleTranslationUnit(ASTContext &Context) override {
//         Visitor.TraverseDecl(Context.getTranslationUnitDecl());
//     }
//     void writeToFile(const std::string &path) {
//         const RewriteBuffer *RewriteBuf = TheRewriter.getRewriteBufferFor(TheRewriter.getSourceMgr().getMainFileID());
//         if (RewriteBuf) {
//             std::error_code EC;
//             llvm::raw_fd_ostream out(path, EC);
//             if (EC) {
//                 llvm::errs() << "Error opening file " << path << ": " << EC.message() << "\n";
//                 return;
//             }
//             out << std::string(RewriteBuf->begin(), RewriteBuf->end());
//         } else {
//             llvm::errs() << " Failed to get rewrite buffer for file.\n";
//         }
//     }
// private:
//     Rewriter TheRewriter;
//     PragmaInserterVisitor Visitor;
// };
// class PragmaInserterAction : public ASTFrontendAction {
// public:
//     explicit PragmaInserterAction(const Solution &S, const std::string& outPath) 
//         : BestSolution(S), outputPath(outPath) {}
    
//     void EndSourceFileAction() override {
//         if (ConsumerPtr && !outputPath.empty()) {
//             ConsumerPtr->writeToFile(outputPath);
//         }
//     }
// protected:
//     std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI, StringRef file) override {
//         auto Consumer = std::make_unique<PragmaInserterConsumer>(&CI.getASTContext(), BestSolution);
//         ConsumerPtr = Consumer.get();
//         return Consumer;
//     }
// private:
//     const Solution &BestSolution;
//     std::string outputPath;
//     PragmaInserterConsumer* ConsumerPtr = nullptr;
// };
// class PragmaInserterActionFactory : public FrontendActionFactory {
// public:
//     explicit PragmaInserterActionFactory(const Solution &S, const std::string& outPath) 
//         : BestSolution(S), outputPath(outPath) {}
    
//     std::unique_ptr<FrontendAction> create() override {
//         return std::make_unique<PragmaInserterAction>(BestSolution, outputPath);
//     }
// private:
//     const Solution &BestSolution;
//     std::string outputPath;
// };


// // --- 第三部分: 命令行选项定义 ---
// static cl::OptionCategory AnalysisCategory("CryptoHLS Analysis Engine Options");
// static cl::opt<std::string> TopFunctionName("top", cl::desc("Specify the top-level function for synthesis"), cl::Required, cl::cat(AnalysisCategory));
// static cl::opt<int> DSEIterations("dse-iterations", cl::desc("Number of DSE iterations for optimization"), cl::init(20), cl::cat(AnalysisCategory));
// static cl::opt<double> PerfWeight("perf-weight", cl::desc("Weight for performance score in DSE"), cl::init(0.7), cl::cat(AnalysisCategory));
// static cl::opt<bool> DataCollectionMode("data-collection-mode", cl::desc("Run in data collection mode"), cl::init(false), cl::cat(AnalysisCategory));
// static cl::opt<std::string> DatasetFile("dataset-file", cl::desc("Specify the output dataset file"), cl::init("../hls_dataset.jsonl"), cl::cat(AnalysisCategory));


// // --- 第四部分: 主函数 ---
// int main(int argc, const char **argv) {
//     auto ExpectedParser = CommonOptionsParser::create(argc, argv, AnalysisCategory, cl::ZeroOrMore);
//     if (!ExpectedParser) { errs() << ExpectedParser.takeError(); return 1; }
//     CommonOptionsParser& OptionsParser = ExpectedParser.get();
//     if (OptionsParser.getSourcePathList().empty()) { errs() << "Error: No input source file specified.\n"; return 1; }

//     std::string TopFunctionNameValue = TopFunctionName.getValue();
//     std::string InputFilename = OptionsParser.getSourcePathList()[0];
    
//     auto clangPathOrErr = sys::findProgramByName("clang++");
//     if (!clangPathOrErr) {
//         errs() << "Error: clang++ not found in PATH. Cannot proceed.\n";
//         return 1;
//     }
//     std::string clangPath = *clangPathOrErr;
//     std::vector<std::string> command_line;
//     command_line.push_back(clangPath);
//     command_line.push_back("-std=c++17");
//     std::vector<std::string> system_includes = getSystemIncludePaths();
//     for (const auto& path : system_includes) {
//         command_line.push_back("-isystem");
//         command_line.push_back(path);
//     }
//     FixedCompilationDatabase Compilations(".", command_line);
    
//     std::cout << "[INFO] Step 1: Building Design Space from source...\n";
//     DesignSpace designSpace;
//     {
//         ClangTool Tool(Compilations, OptionsParser.getSourcePathList());
//         DesignSpaceActionFactory ActionFactory(designSpace, TopFunctionNameValue);
//         if (Tool.run(&ActionFactory)!= 0) {
//             errs() << " Failed to run DesignSpaceAction. Source code may have errors.\n";
//             return 1;
//         }
//     }
//     std::cout << "[INFO]     => Found " << designSpace.size() << " optimization points.\n";

//     std::cout << "[INFO] Step 2: Generating and analyzing base IR...\n";
//     CryptoHLS::IRGenerator ir_generator(InputFilename, "build", system_includes);
//     if (!ir_generator.generate()) { errs() << " Failed to generate IR.\n"; return 1; }

//     std::string base_ir_file = ir_generator.getIRFilePath();
//     LLVMContext context;
//     SMDiagnostic err;
//     std::unique_ptr<llvm::Module> llvm_module = parseIRFile(base_ir_file, err, context);
//     if (!llvm_module) { errs() << " Failed to parse IR file.\n"; return 1; }

//     std::cout << "\n[INFO] Step 2.5: Running Security Analysis Passes...\n";
//     legacy::PassManager pm;
//     pm.add(new CryptoHLS::CryptoSemanticAnalysisPass());
//     pm.add(new CryptoHLS::SecurityScanPass());
//     pm.run(*llvm_module);

//     if (DataCollectionMode) {
//         std::cout << "\n[INFO] Step 3: Starting DSE in Data Collection Mode...\n";
//         std::mt19937 rng(std::random_device{}());

//         GraphExtractor graph_extractor(llvm_module.get());
//         GraphData code_graph = graph_extractor.extract(TopFunctionNameValue);
        
//         for (int i = 0; i < DSEIterations.getValue(); ++i) {
//             std::cout << "\n--- Iteration " << i + 1 << "/" << DSEIterations.getValue() << " ---\n";
            
//             Solution current_solution = generateRandomSolution(designSpace, rng);
//             if (current_solution.empty()) {
//                 std::cout << "[INFO] Skipping empty solution.\n";
//                 continue;
//             }

//             std::string unique_id = std::to_string(getpid()) + "_" + std::to_string(i);

//             std::string optimized_cpp_path;
//             size_t dot_pos = InputFilename.rfind('.');
//             if (dot_pos != std::string::npos) {
//                 optimized_cpp_path = InputFilename.substr(0, dot_pos) + ".optimized." + unique_id + ".cpp";
//             } else {
//                 optimized_cpp_path = InputFilename + ".optimized." + unique_id + ".cpp";
//             }
            
//             {
//                 ClangTool TempTool(Compilations, OptionsParser.getSourcePathList());
//                 PragmaInserterActionFactory PragmaFactory(current_solution, optimized_cpp_path);
//                 if (TempTool.run(&PragmaFactory) != 0) {
//                     errs() << " Failed to run PragmaInserterAction for data collection.\n";
//                     std::remove(optimized_cpp_path.c_str());
//                     continue;
//                 }
//             }
            
//             std::string temp_graph_prefix = "build/graph_features." + unique_id;
//             graph_extractor.saveToFile(code_graph, temp_graph_prefix);
//             std::string temp_graph_nodes_path = temp_graph_prefix + "_nodes.csv";
//             std::string temp_graph_edges_path = temp_graph_prefix + "_edges.csv";

//             std::stringstream ss;
//             ss << "[";
//             for (auto it = current_solution.begin(); it != current_solution.end(); ++it) {
//                 const auto& cmd = *it;
//                 ss << "{\"line\":" << cmd.line << ",\"type\":\"" << cmd.type << "\",\"name\":\"" << cmd.name << "\",\"function\":\"" << cmd.function << "\",\"params\":{";
//                 for (auto p_it = cmd.params.begin(); p_it != cmd.params.end(); ++p_it) {
//                     ss << "\"" << p_it->first << "\":\"" << p_it->second << "\"";
//                     if (std::next(p_it) != cmd.params.end()) ss << ",";
//                 }
//                 ss << "}}";
//                 if (std::next(it) != current_solution.end()) ss << ",";
//             }
//             ss << "]";
//             std::string solution_json_str = ss.str();

//             std::string project_dir = "hls_project_" + unique_id;

//             std::string command = "python3 ../hls_runner.py ";
//             command += optimized_cpp_path;
//             command += " --top=" + TopFunctionNameValue;
//             command += " --solution='" + solution_json_str + "'";
//             command += " --graph-nodes=" + temp_graph_nodes_path;
//             command += " --graph-edges=" + temp_graph_edges_path;
//             command += " --dataset-file=" + DatasetFile.getValue();
//             command += " --project-dir=" + project_dir;

//             int ret = system(command.c_str());

//             if (ret != 0) {
//                 errs() << " hls_runner.py script failed for the current solution.\n";
//             }

//             // --- [核心修改] ---
//             // 只删除临时的优化后C++文件，保留图特征文件(.csv)
//             std::remove(optimized_cpp_path.c_str());
//             // std::remove(temp_graph_nodes_path.c_str()); // <-- 注释掉这行
//             // std::remove(temp_graph_edges_path.c_str()); // <-- 注释掉这行
//         }
//         std::cout << "\n[INFO] Data collection finished.\n";

//     } else {
//         FunctionCost base_cost = IRAnalyzer::analyzeModule(llvm_module.get(), TopFunctionNameValue);
//         std::cout << "[INFO] Base performance score: " << base_cost.performance_score << ", Base area score: " << base_cost.area_score << "\n";
        
//         CostModel cost_model(base_cost);
//         std::mt19937 rng(std::random_device{}());
//         Solution best_solution;
//         double best_total_score = std::numeric_limits<double>::max();
        
//         std::cout << "\n[INFO] Step 3: Starting in-memory DSE...\n";
//         for (int i = 0; i < DSEIterations.getValue(); ++i) {
//             Solution current_solution = generateRandomSolution(designSpace, rng);
//             FunctionCost predicted_cost = cost_model.predictCost(current_solution);
//             double total_score = PerfWeight.getValue() * predicted_cost.performance_score + (1.0 - PerfWeight.getValue()) * predicted_cost.area_score;
//             if (total_score < best_total_score) {
//                 best_total_score = total_score;
//                 best_solution = current_solution;
//             }
//         }
        
//         std::cout << "\n[INFO] DSE Finished.\n";
        
//         std::cout << "[INFO] Step 5: Generating optimized C++ source file...\n";
//         if (!best_solution.empty()) {
//             std::string final_output_path;
//             size_t dot_pos = InputFilename.rfind('.');
//             if (dot_pos != std::string::npos) {
//                 final_output_path = InputFilename.substr(0, dot_pos) + ".optimized.final.cpp";
//             } else {
//                 final_output_path = InputFilename + ".optimized.final.cpp";
//             }
            
//             ClangTool FinalTool(Compilations, OptionsParser.getSourcePathList());
//             PragmaInserterActionFactory PragmaFactory(best_solution, final_output_path);
//             if (FinalTool.run(&PragmaFactory)!= 0) {
//                 errs() << " Failed to run PragmaInserterAction.\n";
//             } else {
//                 llvm::outs() << "Generated final optimized file: " << final_output_path << "\n";
//             }
//         }
//     }

//     std::remove(base_ir_file.c_str());
//     std::remove(ir_generator.getBitcodeFilePath().c_str());
//     return 0;
// }



#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <fstream>
#include <limits>
#include <random>
#include <cstdio> // 用于 popen
#include <array>
#include <sstream>
#include <unistd.h> // 为了 getpid()

// 核心模块头文件
#include "Core.h"
#include "DesignSpaceVisitor.h"
#include "IRAnalyzer.h"
#include "DSE.h"
#include "PragmaInserter.h"
#include "IRGenerator.h"
#include "AnalysisPasses.h"
#include "GraphExtractor.h"

// Clang/LLVM 核心头文件
#include "clang/AST/ASTConsumer.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "clang/Tooling/CompilationDatabase.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Support/Program.h"
#include "llvm/Support/FileSystem.h"

using namespace clang;
using namespace clang::tooling;
using namespace llvm;

// --- 自动探测系统头文件路径的辅助函数 (保持不变) ---
std::vector<std::string> getSystemIncludePaths() {
    std::vector<std::string> paths;
    std::shared_ptr<FILE> pipe(popen("clang++ -E -x c++ - -v < /dev/null 2>&1", "r"), pclose);
    if (!pipe) return paths;
    std::array<char, 128> buffer;
    std::string result;
    while (fgets(buffer.data(), buffer.size(), pipe.get())!= nullptr) { result += buffer.data(); }
    std::string start_marker = "#include <...> search starts here:";
    std::string end_marker = "End of search list.";
    size_t start_pos = result.find(start_marker);
    if (start_pos == std::string::npos) return paths;
    start_pos += start_marker.length();
    size_t end_pos = result.find(end_marker, start_pos);
    if (end_pos == std::string::npos) return paths;
    std::string paths_str = result.substr(start_pos, end_pos - start_pos);
    std::stringstream ss(paths_str);
    std::string path;
    while (std::getline(ss, path)) {
        size_t first = path.find_first_not_of(" \n\r\t");
        if (std::string::npos == first) continue;
        size_t last = path.find_last_not_of(" \n\r\t");
        std::string clean_path = path.substr(first, (last - first + 1));
        if (sys::fs::is_directory(clean_path)) {
            paths.push_back(clean_path);
        }
    }
    return paths;
}

// --- 第一部分: AST分析相关的类定义 (保持不变) ---
class DesignSpaceConsumer : public ASTConsumer {
public:
    explicit DesignSpaceConsumer(ASTContext *Context, std::string &TopName, DesignSpace &DS)
        : Visitor(Context, TopName), finalDesignSpace(DS) {}
    void HandleTranslationUnit(ASTContext &Context) override {
        Visitor.TraverseDecl(Context.getTranslationUnitDecl());
        finalDesignSpace = Visitor.getDesignSpace();
    }
private:
    DesignSpaceVisitor Visitor;
    DesignSpace &finalDesignSpace;
};
class DesignSpaceAction : public ASTFrontendAction {
public:
    explicit DesignSpaceAction(DesignSpace& DS, std::string& topName)
        : finalDesignSpace(DS), TopFunctionName(topName) {}
protected:
    std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI, StringRef file) override {
        return std::make_unique<DesignSpaceConsumer>(&CI.getASTContext(), TopFunctionName, finalDesignSpace);
    }
private:
    DesignSpace& finalDesignSpace;
    std::string& TopFunctionName;
};
class DesignSpaceActionFactory : public FrontendActionFactory {
public:
    DesignSpaceActionFactory(DesignSpace& DS, std::string& topName)
        : finalDesignSpace(DS), TopFunctionName(topName) {}
    std::unique_ptr<FrontendAction> create() override {
        return std::make_unique<DesignSpaceAction>(finalDesignSpace, TopFunctionName);
    }
private:
    DesignSpace& finalDesignSpace;
    std::string& TopFunctionName;
};


// --- [核心修正] 第二部分: 恢复到稳定、可编译的Factory模式 ---
class PragmaInserterConsumer : public ASTConsumer {
public:
    explicit PragmaInserterConsumer(ASTContext *Context, const Solution &S)
        : Visitor(TheRewriter, S) {
        TheRewriter.setSourceMgr(Context->getSourceManager(), Context->getLangOpts());
    }
    void HandleTranslationUnit(ASTContext &Context) override {
        Visitor.TraverseDecl(Context.getTranslationUnitDecl());
    }
    void writeToFile(const std::string &path) {
        const RewriteBuffer *RewriteBuf = TheRewriter.getRewriteBufferFor(TheRewriter.getSourceMgr().getMainFileID());
        if (RewriteBuf) {
            std::error_code EC;
            llvm::raw_fd_ostream out(path, EC);
            if (EC) {
                llvm::errs() << "Error opening file " << path << ": " << EC.message() << "\n";
                return;
            }
            out << std::string(RewriteBuf->begin(), RewriteBuf->end());
        } else {
            // 如果没有改动，也需要创建一个文件，否则HLS会报错
            std::ifstream src(TheRewriter.getSourceMgr().getBufferOrFake(TheRewriter.getSourceMgr().getMainFileID()).getBufferIdentifier().str(), std::ios::binary);
            std::ofstream dst(path, std::ios::binary);
            dst << src.rdbuf();
        }
    }
private:
    Rewriter TheRewriter;
    PragmaInserterVisitor Visitor;
};

class PragmaInserterAction : public ASTFrontendAction {
public:
    explicit PragmaInserterAction(const Solution &S, const std::string& outPath) 
        : BestSolution(S), outputPath(outPath) {}
    
    void EndSourceFileAction() override {
        if (ConsumerPtr && !outputPath.empty()) {
            ConsumerPtr->writeToFile(outputPath);
        }
    }
protected:
    std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI, StringRef file) override {
        auto Consumer = std::make_unique<PragmaInserterConsumer>(&CI.getASTContext(), BestSolution);
        ConsumerPtr = Consumer.get();
        return Consumer;
    }
private:
    const Solution &BestSolution;
    std::string outputPath;
    PragmaInserterConsumer* ConsumerPtr = nullptr;
};

class PragmaInserterActionFactory : public FrontendActionFactory {
public:
    explicit PragmaInserterActionFactory(const Solution &S, const std::string& outPath) 
        : BestSolution(S), outputPath(outPath) {}
    
    std::unique_ptr<FrontendAction> create() override {
        return std::make_unique<PragmaInserterAction>(BestSolution, outputPath);
    }
private:
    const Solution &BestSolution;
    std::string outputPath;
};

// --- 第三部分: 命令行选项定义 (保持不变) ---
static cl::OptionCategory AnalysisCategory("CryptoHLS Analysis Engine Options");
static cl::opt<std::string> TopFunctionName("top", cl::desc("Specify the top-level function for synthesis"), cl::Required, cl::cat(AnalysisCategory));
static cl::opt<int> DSEIterations("dse-iterations", cl::desc("Number of DSE iterations for optimization"), cl::init(20), cl::cat(AnalysisCategory));
static cl::opt<double> PerfWeight("perf-weight", cl::desc("Weight for performance score in DSE"), cl::init(0.7), cl::cat(AnalysisCategory));
static cl::opt<bool> DataCollectionMode("data-collection-mode", cl::desc("Run in data collection mode"), cl::init(false), cl::cat(AnalysisCategory));
static cl::opt<std::string> DatasetFile("dataset-file", cl::desc("Specify the output dataset file"), cl::init("../hls_dataset.jsonl"), cl::cat(AnalysisCategory));


// --- 第四部分: 主函数 ---
int main(int argc, const char **argv) {
    auto ExpectedParser = CommonOptionsParser::create(argc, argv, AnalysisCategory, cl::ZeroOrMore);
    if (!ExpectedParser) { errs() << ExpectedParser.takeError(); return 1; }
    CommonOptionsParser& OptionsParser = ExpectedParser.get();
    if (OptionsParser.getSourcePathList().empty()) { errs() << "Error: No input source file specified.\n"; return 1; }

    std::string TopFunctionNameValue = TopFunctionName.getValue();
    std::string InputFilename = OptionsParser.getSourcePathList()[0];
    
    // ... (动态构建编译数据库保持不变) ...
    auto clangPathOrErr = sys::findProgramByName("clang++");
    if (!clangPathOrErr) { errs() << "Error: clang++ not found in PATH. Cannot proceed.\n"; return 1; }
    std::string clangPath = *clangPathOrErr;
    std::vector<std::string> command_line;
    command_line.push_back(clangPath);
    command_line.push_back("-std=c++17");
    std::vector<std::string> system_includes = getSystemIncludePaths();
    for (const auto& path : system_includes) {
        command_line.push_back("-isystem");
        command_line.push_back(path);
    }
    FixedCompilationDatabase Compilations(".", command_line);
    
    std::cout << "[INFO] Step 1: Building Design Space from source...\n";
    DesignSpace designSpace;
    {
        ClangTool Tool(Compilations, OptionsParser.getSourcePathList());
        DesignSpaceActionFactory ActionFactory(designSpace, TopFunctionNameValue);
        if (Tool.run(&ActionFactory)!= 0) {
            errs() << " Failed to run DesignSpaceAction. Source code may have errors.\n";
            return 1;
        }
    }
    std::cout << "[INFO]     => Found " << designSpace.size() << " optimization points.\n";

    std::cout << "[INFO] Step 2: Generating and analyzing base IR...\n";
    CryptoHLS::IRGenerator ir_generator(InputFilename, "build", system_includes);
    if (!ir_generator.generate()) { errs() << " Failed to generate IR.\n"; return 1; }

    std::string base_ir_file = ir_generator.getIRFilePath();
    LLVMContext context;
    SMDiagnostic err;
    std::unique_ptr<llvm::Module> llvm_module = parseIRFile(base_ir_file, err, context);
    if (!llvm_module) { errs() << " Failed to parse IR file.\n"; return 1; }

    std::cout << "\n[INFO] Step 2.5: Running Security Analysis Passes...\n";
    legacy::PassManager pm;
    pm.add(new CryptoHLS::CryptoSemanticAnalysisPass());
    pm.add(new CryptoHLS::SecurityScanPass());
    pm.run(*llvm_module);

    if (DataCollectionMode) {
        std::cout << "\n[INFO] Step 3: Starting DSE in Data Collection Mode...\n";
        std::mt19937 rng(std::random_device{}());
        GraphExtractor graph_extractor(llvm_module.get());
        GraphData code_graph = graph_extractor.extract(TopFunctionNameValue);
        
        for (int i = 0; i < DSEIterations.getValue(); ++i) {
            std::cout << "\n--- Iteration " << i + 1 << "/" << DSEIterations.getValue() << " ---\n";
            
            Solution current_solution = generateRandomSolution(designSpace, rng);
            if (current_solution.empty()) {
                std::cout << "[INFO] Skipping empty solution.\n";
                continue;
            }

            std::string unique_id = std::to_string(getpid()) + "_" + std::to_string(i);

            std::string optimized_cpp_path;
            size_t dot_pos = InputFilename.rfind('.');
            if (dot_pos != std::string::npos) {
                optimized_cpp_path = InputFilename.substr(0, dot_pos) + ".optimized." + unique_id + ".cpp";
            } else {
                optimized_cpp_path = InputFilename + ".optimized." + unique_id + ".cpp";
            }
            
            // [核心修正] 恢复使用我们自定义的Factory来运行Clang工具
            {
                ClangTool TempTool(Compilations, OptionsParser.getSourcePathList());
                PragmaInserterActionFactory PragmaFactory(current_solution, optimized_cpp_path);
                if (TempTool.run(&PragmaFactory) != 0) {
                    errs() << " Failed to run PragmaInserterAction for data collection.\n";
                    std::remove(optimized_cpp_path.c_str());
                    continue;
                }
            }
            
            std::string temp_graph_prefix = "build/graph_features." + unique_id;
            graph_extractor.saveToFile(code_graph, temp_graph_prefix);
            std::string temp_graph_nodes_path = temp_graph_prefix + "_nodes.csv";
            std::string temp_graph_edges_path = temp_graph_prefix + "_edges.csv";

            std::stringstream ss;
            ss << "[";
            for (auto it = current_solution.begin(); it != current_solution.end(); ++it) {
                const auto& cmd = *it;
                ss << "{\"line\":" << cmd.line << ",\"type\":\"" << cmd.type << "\",\"name\":\"" << cmd.name << "\",\"function\":\"" << cmd.function << "\",\"params\":{";
                for (auto p_it = cmd.params.begin(); p_it != cmd.params.end(); ++p_it) {
                    ss << "\"" << p_it->first << "\":\"" << p_it->second << "\"";
                    if (std::next(p_it) != cmd.params.end()) ss << ",";
                }
                ss << "}}";
                if (std::next(it) != current_solution.end()) ss << ",";
            }
            ss << "]";
            std::string solution_json_str = ss.str();
            std::string project_dir = "hls_project_" + unique_id;
            std::string command = "python3 ../scripts/hls_runner.py ";
            command += optimized_cpp_path;
            command += " --top=" + TopFunctionNameValue;
            command += " --solution='" + solution_json_str + "'";
            command += " --graph-nodes=" + temp_graph_nodes_path;
            command += " --graph-edges=" + temp_graph_edges_path;
            command += " --dataset-file=" + DatasetFile.getValue();
            command += " --project-dir=" + project_dir;

            int ret = system(command.c_str());
            if (ret != 0) {
                errs() << " hls_runner.py script failed for the current solution.\n";
            }
            std::remove(optimized_cpp_path.c_str());
            // std::remove(temp_graph_nodes_path.c_str());
            // std::remove(temp_graph_edges_path.c_str());
        }
        std::cout << "\n[INFO] Data collection finished.\n";

    } else {
        FunctionCost base_cost = IRAnalyzer::analyzeModule(llvm_module.get(), TopFunctionNameValue);
        std::cout << "[INFO] Base performance score: " << base_cost.performance_score << ", Base area score: " << base_cost.area_score << "\n";
        
        CostModel cost_model(base_cost);
        std::mt19937 rng(std::random_device{}());
        Solution best_solution;
        double best_total_score = std::numeric_limits<double>::max();
        
        std::cout << "\n[INFO] Step 3: Starting in-memory DSE...\n";
        for (int i = 0; i < DSEIterations.getValue(); ++i) {
            Solution current_solution = generateRandomSolution(designSpace, rng);
            FunctionCost predicted_cost = cost_model.predictCost(current_solution);
            double total_score = PerfWeight.getValue() * predicted_cost.performance_score + (1.0 - PerfWeight.getValue()) * predicted_cost.area_score;
            if (total_score < best_total_score) {
                best_total_score = total_score;
                best_solution = current_solution;
            }
        }
        
        std::cout << "\n[INFO] DSE Finished.\n";
        
        std::cout << "[INFO] Step 5: Generating optimized C++ source file...\n";
        if (!best_solution.empty()) {
            std::string final_output_path;
            size_t dot_pos = InputFilename.rfind('.');
            if (dot_pos != std::string::npos) {
                final_output_path = InputFilename.substr(0, dot_pos) + ".optimized.final.cpp";
            } else {
                final_output_path = InputFilename + ".optimized.final.cpp";
            }
            
            ClangTool FinalTool(Compilations, OptionsParser.getSourcePathList());
            PragmaInserterActionFactory PragmaFactory(best_solution, final_output_path);
            if (FinalTool.run(&PragmaFactory)!= 0) {
                errs() << " Failed to run PragmaInserterAction.\n";
            } else {
                llvm::outs() << "Generated final optimized file: " << final_output_path << "\n";
            }
        }
    }

    std::remove(base_ir_file.c_str());
    std::remove(ir_generator.getBitcodeFilePath().c_str());
    return 0;
}
