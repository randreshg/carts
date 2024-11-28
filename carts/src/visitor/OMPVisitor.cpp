#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"

#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"

#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/AST/StmtOpenMP.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/DiagnosticIDs.h"
#include "clang/Basic/DiagnosticOptions.h"
#include "clang/Basic/FileSystemOptions.h"
#include "clang/Frontend/ASTUnit.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/CompilerInvocation.h"
#include "clang/Frontend/TextDiagnosticPrinter.h"
#include "clang/Frontend/Utils.h"
#include "clang/Lex/HeaderSearchOptions.h"

using namespace llvm;
using namespace clang;
#include "llvm/Support/Debug.h"

// Command-line option to specify the input AST file
static cl::opt<std::string>
    InputASTFilename("ast-file", cl::desc("Specify the input AST filename"),
                     cl::value_desc("filename"),
                     cl::init("input.ast")); // Default value if not specified

/// DEBUG
#define DEBUG_TYPE "omp-visitor"
#if !defined(NDEBUG)
static constexpr auto TAG = "[" DEBUG_TYPE "] ";
#endif

/// Visitor class to traverse the AST and find OpenMP directives
class OpenMPVisitor : public RecursiveASTVisitor<OpenMPVisitor> {
public:
  explicit OpenMPVisitor(ASTContext &Context, llvm::Module &M)
      : Context(Context), M(M), SM(Context.getSourceManager()) {}

  bool VisitFunctionDecl(FunctionDecl *FD) {
    if (FD->hasBody())
      TraverseStmt(FD->getBody());
    return true;
  }

  bool VisitStmt(Stmt *S) {
    if (isa<OMPExecutableDirective>(S)) {
      OMPExecutableDirective *Directive = cast<OMPExecutableDirective>(S);

      LLVM_DEBUG(dbgs() << "Found OpenMP directive: "
                        << Directive->getStmtClassName() << " at "
                        << S->getBeginLoc().printToString(SM) << "\n");

      // Check if it's an OpenMP task directive
      if (auto *TaskDirective = dyn_cast<OMPTaskDirective>(Directive)) {
        LLVM_DEBUG(dbgs() << "  This is an OpenMP Task Directive\n");

        // Extract clauses
        for (const OMPClause *Clause : TaskDirective->clauses()) {
          if (!Clause)
            continue;

          LLVM_DEBUG(dbgs()
                     << "    Clause: "
                     << getOpenMPClauseName(Clause->getClauseKind()) << "\n");

          // Handle specific clause types
          switch (Clause->getClauseKind()) {
          case omp::OMPC_depend: {
            const auto *DependClause = cast<OMPDependClause>(Clause);
            LLVM_DEBUG(dbgs() << "      Dependency type: "
                              << OpenMPDependClauseKind(
                                     DependClause->getDependencyKind())
                              << "\n");
            for (const Expr *DepExpr : DependClause->varlists()) {
              LLVM_DEBUG(dbgs() << "      Dependency variable: ");
              PrintingPolicy Policy(Context.getLangOpts());
              DepExpr->printPretty(llvm::outs(), nullptr, Policy);
              LLVM_DEBUG(dbgs() << "\n");
            }
            break;
          }
          case omp::OMPC_if: {
            const auto *IfClause = cast<OMPIfClause>(Clause);
            LLVM_DEBUG(dbgs() << "      Condition: ");
            PrintingPolicy Policy(Context.getLangOpts());
            IfClause->getCondition()->printPretty(llvm::outs(), nullptr,
                                                  Policy);
            LLVM_DEBUG(dbgs() << "\n");
            break;
          }
          default:
            break;
          }
        }
      }
    }
    return true;
  }

private:
  ASTContext &Context;
  llvm::Module &M;
  const SourceManager &SM;
};

/// AST Consumer to handle the traversal
class OpenMPASTConsumer : public ASTConsumer {
public:
  explicit OpenMPASTConsumer(ASTContext &Context) : Visitor(Context) {}

  void HandleTranslationUnit(ASTContext &Context) override {
    Visitor.TraverseDecl(Context.getTranslationUnitDecl());
  }

private:
  OpenMPVisitor Visitor;
};

namespace {
/// LLVM ModulePass that loads an AST file and parses OpenMP functions
class OpenMPVisitorPass : public PassInfoMixin<OpenMPVisitorPass> {
public:
  PreservedAnalyses run(llvm::Module &M, ModuleAnalysisManager &AM) {
    /// AST Traversal with OpenMPVisitor and Clang tooling
    LLVM_DEBUG(
        dbgs() << "\n-------------------------------------------------\n"
               << TAG << "Running OmpVisitorPass on Module: \n"
               << M.getName() << "\n"
               << "\n-------------------------------------------------\n");

    /// Use the command-line option for the AST filename
    std::string Filename = InputASTFilename;

    /// Initialize DiagnosticOptions and DiagnosticIDs
    IntrusiveRefCntPtr<DiagnosticOptions> DiagOpts(new DiagnosticOptions());
    IntrusiveRefCntPtr<DiagnosticIDs> DiagID(new DiagnosticIDs());

    /// Create a TextDiagnosticPrinter as the DiagnosticConsumer
    IntrusiveRefCntPtr<DiagnosticsEngine> Diags(new DiagnosticsEngine(
        DiagID, &*DiagOpts,
        new clang::TextDiagnosticPrinter(llvm::errs(), &*DiagOpts)));
    assert(Diags->getClient() && "DiagnosticsEngine has no DiagnosticClient!");

    /// Set up FileSystemOptions
    clang::FileSystemOptions FileSystemOpts;
    FileSystemOpts.WorkingDir = "."; // Set working directory as needed

    /// Create PCHContainerOperations
    std::shared_ptr<clang::PCHContainerOperations> PCHOps =
        std::make_shared<clang::PCHContainerOperations>();
    const clang::PCHContainerReader &PCHContainerRdr = PCHOps->getRawReader();
    std::shared_ptr<clang::HeaderSearchOptions> HSOpts =
        std::make_shared<clang::HeaderSearchOptions>();

    // Load the AST file with the correct number of arguments
    std::unique_ptr<ASTUnit> AST = ASTUnit::LoadFromASTFile(
        Filename, PCHContainerRdr, ASTUnit::LoadASTOnly, Diags, FileSystemOpts,
        HSOpts);

    if (!AST) {
      LLVM_DEBUG(dbgs() << "Error: Could not load AST file '" << Filename
                        << "'\n");
      return PreservedAnalyses::all();
    }

    // Traverse the AST
    OpenMPASTConsumer Consumer(AST->getASTContext());
    Consumer.HandleTranslationUnit(AST->getASTContext());
    return PreservedAnalyses::all();
  }
};
} // namespace

extern "C" ::llvm::PassPluginLibraryInfo LLVM_ATTRIBUTE_WEAK
llvmGetPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "OpenMPVisitorPass", "v0.1",
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, ModulePassManager &MPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "omp-visitor") {
                    MPM.addPass(OpenMPVisitorPass());
                    return true;
                  }
                  return false;
                });
          }};
}
