// #include "clang/Frontend/FrontendActions.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"

#include "clang/Frontend/FrontendPluginRegistry.h"
#include "carts/frontend/openmp/OpenMPConsumer.h"

using namespace clang;
using namespace clang::tooling;
using namespace llvm;

// Define a category for command-line options
static cl::OptionCategory OpenMPToolCategory("openmp-tool options");

class OpenMPPluginAction : public PluginASTAction {
protected:

    // This is called before processing the source file begins.
    bool BeginSourceFileAction(CompilerInstance &CI) override {
    llvm::outs() << "Starting OpenMPPluginAction...\n";

    // Set the program action to emit LLVM IR
    CI.getFrontendOpts().ProgramAction = frontend::EmitLLVM;
    
    return true; // Continue processing
}

  std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI,
                                                 StringRef) override {
    R.setSourceMgr(CI.getSourceManager(), CI.getLangOpts());
    return std::make_unique<arts::OpenMPConsumer>(CI.getASTContext(), R);
  }

  bool ParseArgs(const CompilerInstance &,
                 const std::vector<std::string> &) override {
    return true;
  }

  ActionType getActionType() override { return AddAfterMainAction; }

private:
  clang::Rewriter R;
};

static FrontendPluginRegistry::Add<OpenMPPluginAction>
X("omp-plugin", "Outline OMP parallel regions with captured vars and mark as parallel");

int main(int argc, const char **argv) {
    /// Print initial info
    outs() << "CARTS - OpenMP Frontend\n";
    /// Print input file
    outs() << " - Input file: " << argv[argc - 1] << "\n";
    // Parse command-line arguments
    auto ExpectedParser = CommonOptionsParser::create(argc, argv, OpenMPToolCategory);
    if (!ExpectedParser) {
        errs() << "Error creating CommonOptionsParser: " << toString(ExpectedParser.takeError()) << "\n";
        return 1;
    }
    CommonOptionsParser &OptionsParser = *ExpectedParser;

    // Set up the ClangTool with parsed options and input files
    ClangTool Tool(OptionsParser.getCompilations(), OptionsParser.getSourcePathList());

    // Create a factory for the OpenMPPluginAction
    // Normally, you'd load this plugin with `-Xclang -load` and `-Xclang -plugin`,
    // but here we directly instantiate it for demonstration purposes.
    std::unique_ptr<FrontendActionFactory> Factory = newFrontendActionFactory<OpenMPPluginAction>();

    // Run the tool with the given action.
    // The plugin action will modify OpenMP constructs and rely on the default code generation for the rest.
    return Tool.run(Factory.get());
}