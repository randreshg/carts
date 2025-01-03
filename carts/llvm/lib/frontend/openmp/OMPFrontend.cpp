// #include "clang/Frontend/FrontendActions.h"
// #include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Tooling/Tooling.h"

#include "arts/frontend/openmp/OMPConsumer.h"
#include "clang/Frontend/FrontendPluginRegistry.h"

#include "llvm/Support/Debug.h"

#include <cassert>
#include <string>

/// DEBUG
#define DEBUG_TYPE "omp-plugin"
#if !defined(NDEBUG)
static constexpr auto TAG = "[" DEBUG_TYPE "] ";
#endif

using namespace clang;
using namespace clang::tooling;
using namespace llvm;

// Define a category for command-line options
static cl::OptionCategory OpenMPToolCategory("openmp-plugin options");

class OpenMPPluginAction : public PluginASTAction {
protected:
  // This is called before processing the source file begins.
  bool BeginSourceFileAction(CompilerInstance &CI) override {
    LLVM_DEBUG(dbgs() << "- - - - - - - - - - - - - - - - - - - \n"
                      << "Starting CARTS - OpenMPPluginAction...\n"
                      << "- - - - - - - - - - - - - - - - - - - \n");
    return true;
  }

  std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI,
                                                 StringRef InFile) override {
    InputFileName = InFile.str();
    R.setSourceMgr(CI.getSourceManager(), CI.getLangOpts());
    LLVM_DEBUG(dbgs() << "CreateASTConsumer with file: " << InputFileName
                      << "\n");
    return std::make_unique<arts::OMPConsumer>(CI.getASTContext(), R);
  }

  bool ParseArgs(const CompilerInstance &,
                 const std::vector<std::string> &) override {
    return true;
  }

  void EndSourceFileAction() override {
    LLVM_DEBUG(dbgs() << "\n"
                      << "- - - - - - - - - - - - - - - - - -\n"
                      << "Finished CARTS - OpenMPPluginAction\n"
                      << "- - - - - - - - - - - - - - - - - -\n");
    FileID FID = R.getSourceMgr().getMainFileID();
    const RewriteBuffer *Buffer = R.getRewriteBufferFor(FID);
    if (!Buffer) {
      LLVM_DEBUG(dbgs() << "No rewrite buffer found! No changes?\n");
      return;
    }

    /// Extract name without extension
    size_t LastIndex = InputFileName.find_last_of(".");
    assert(LastIndex != std::string::npos && "Invalid input file name");
    std::string OutName = InputFileName.substr(0, LastIndex) + "_annotaded" +
                          InputFileName.substr(LastIndex);
    std::error_code EC;
    llvm::raw_fd_ostream OutFile(OutName, EC, llvm::sys::fs::OF_None);
    if (EC) {
      LLVM_DEBUG(dbgs() << "Error opening " << OutName << ": " << EC.message()
                        << "\n");
      return;
    }

    OutFile << std::string(Buffer->begin(), Buffer->end());
    OutFile.close();
    LLVM_DEBUG(dbgs() << "Rewritten code saved to " << OutName << "\n");
  }

private:
  clang::Rewriter R;
  std::string InputFileName;
};

static FrontendPluginRegistry::Add<OpenMPPluginAction>
    X("omp-plugin",
      "Outline OMP parallel regions with captured vars and mark as parallel");
