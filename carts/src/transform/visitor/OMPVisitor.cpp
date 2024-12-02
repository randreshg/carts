/// AST Visitor to traverse the AST and find OpenMP directives

#include "llvm/IR/Instruction.h"
#include "llvm/Passes/PassBuilder.h"
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
#include "llvm/ADT/SetVector.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"

#include "llvm/Support/Debug.h"

#include <string>
using namespace llvm;
using namespace clang;
using namespace std;

/// DEBUG
#define DEBUG_TYPE "omp-visitor"
#if !defined(NDEBUG)
static constexpr auto TAG = "[" DEBUG_TYPE "] ";
#endif

// Base class for all OpenMP directives
class OMPDirectiveInfo {
public:
  OMPDirectiveInfo(StringRef Type, SourceLocation Loc)
      : DirectiveType(Type), Location(Loc) {}

  virtual ~OMPDirectiveInfo() = default;

  StringRef getType() const { return DirectiveType; }

  string getLocation(const SourceManager &SM) const {
    return Location.printToString(SM);
  }

  virtual void printInfo(raw_ostream &OS) const {
    OS << "Directive: " << DirectiveType << "\n";
  }

protected:
  StringRef DirectiveType;
  SourceLocation Location;
};

// Parallel directive info
class OMPParallelInfo : public OMPDirectiveInfo {
public:
  OMPParallelInfo(SourceLocation Loc) : OMPDirectiveInfo("parallel", Loc) {}

  unsigned getNumThreads() const { return NumThreads; }

  void printInfo(raw_ostream &OS) const override {
    OMPDirectiveInfo::printInfo(OS);
    OS << "  Number of Threads: " << NumThreads << "\n";
  }

private:
  /// Number of threads for the parallel region
  unsigned NumThreads;
};

// Task directive info
class OMPTaskInfo : public OMPDirectiveInfo {
public:
  OMPTaskInfo(SourceLocation Loc) : OMPDirectiveInfo("task", Loc) {}

  bool hasDependencies() const { return !Inputs.empty() || !Outputs.empty(); }

  void printInfo(raw_ostream &OS) const override {
    OMPDirectiveInfo::printInfo(OS);
    OS << "  Dependencies: " << (Dependencies ? "Yes" : "No") << "\n";
  }

private:
  bool Dependencies;
  SetVector<Instruction *> Inputs;
  SetVector<Instruction *> Outputs;
};

/// Visitor class to traverse the AST and find OpenMP directives
class OpenMPVisitor : public RecursiveASTVisitor<OpenMPVisitor> {
public:
  explicit OpenMPVisitor(ASTContext &Context, SetVector<Function *> &Functions)
      : Context(Context), Functions(Functions), SM(Context.getSourceManager()) {
  }

  bool VisitFunctionDecl(FunctionDecl *FD) {
    if (FD->hasBody()) {
      /// Traverse the function body given a function context
      CurrentFn = FD;
      TraverseStmt(FD->getBody());
      CurrentFn = nullptr;
    }
    return true;
  }

  bool VisitStmt(Stmt *S) {
    OMPExecutableDirective *Directive = dyn_cast<OMPExecutableDirective>(S);
    /// We are only interested in OpenMP directives
    if (!Directive)
      return true;

    LLVM_DEBUG(dbgs() << "Found OpenMP Directive ("
                      << Directive->getStmtClassName() << ") at "
                      << Directive->getBeginLoc().printToString(SM) << "\n");

    if (!CurrentFn) {
      LLVM_DEBUG(dbgs() << "  Error: No enclosing function context found.\n");
      return true;
    }

    LLVM_DEBUG(dbgs() << "  Enclosing function: " << CurrentFn->getName()
                      << "\n");
    handleDirective(Directive);
    return true;
  }

  /// Maps LLVM IR instructions to frontend directives based on debug metadata
  /// (DILocation).
  void preprocessLLVMIR() {
    for (Function *F : Functions) {
      for (Instruction &I : instructions(F)) {
        if (DILocation *Loc = I.getDebugLoc()) {
          unsigned Line = Loc->getLine();
          StringRef FunctionName = F->getName();
          auto FuncIt = LineToDirective.find(FunctionName.str());
          if (FuncIt != LineToDirective.end()) {
            auto LineIt = FuncIt->second.find(Line);
            if (LineIt != FuncIt->second.end()) {
              InstructionToDirective[&I] = LineIt->second;
            }
          }
        }
      }
    }
  }

  const OMPDirectiveInfo *getDirectiveForInstruction(Instruction *I) const {
    auto It = InstructionToDirective.find(I);
    if (It != InstructionToDirective.end())
      return It->second;
    return nullptr;
  }

private:
  ASTContext &Context;
  SetVector<Function *> &Functions;
  const SourceManager &SM;
  const FunctionDecl *CurrentFn;

  DenseMap<string, DenseMap<unsigned, OMPDirectiveInfo *>> LineToDirective;
  DenseMap<Instruction *, OMPDirectiveInfo *> InstructionToDirective;

  void handleDirective(OMPExecutableDirective *Directive) {
    // StringRef DirectiveName = Directive->getStmtClassName();
    SourceLocation Loc = Directive->getBeginLoc();
    switch (Directive->getDirectiveKind()) {
    case omp::OMPD_parallel: {
      auto *ParallelDirective = cast<OMPParallelDirective>(Directive);
      // unsigned NumThreads = 0;
      // for (const OMPClause *Clause : ParallelDirective->clauses()) {
      //   if (const auto *NumThreadsClause =
      //   dyn_cast<OMPNumThreadsClause>(Clause)) {
      //     if (const Expr *NumThreadsExpr = NumThreadsClause->getNumThreads())
      //     {
      //       llvm::APSInt Result;
      //       if (NumThreadsExpr->EvaluateAsInt(Result, Context)) {
      //         NumThreads = Result.getLimitedValue();
      //       }
      //     }
      //   }
      // }
      OMPParallelInfo *Info = new OMPParallelInfo(Loc);
      storeDirective(Info, Directive);
      // handleClauses(Directive);
    } break;
    case omp::OMPD_task: {
      OMPTaskInfo *Info = new OMPTaskInfo(Loc);
      storeDirective(Info, Directive);
      // handleClauses(Directive);
    } break;
    default:
      LLVM_DEBUG(dbgs() << "  (Unhandled directive type)\n");
      break;
    }
  }

  void handleClauses(const OMPExecutableDirective *Directive) {
    //   LLVM_DEBUG(dbgs() << "  Processing clauses for directive...\n");
    //   for (const OMPClause *Clause : Directive->clauses()) {
    //     if (!Clause)
    //       continue;

    //     LLVM_DEBUG(dbgs() << "    Clause: "
    //                       << getOpenMPClauseName(Clause->getClauseKind())
    //                       << "\n");

    //     switch (Clause->getClauseKind()) {
    //     case omp::OMPC_depend: {
    //       auto *DependClause = cast<OMPDependClause>(Clause);
    //       LLVM_DEBUG(
    //           dbgs() << "      Dependency type: "
    //                  <<
    //                  OpenMPDependClauseKind(DependClause->getDependencyKind())
    //                  << "\n");
    //       for (const Expr *DepExpr : DependClause->varlists()) {
    //         LLVM_DEBUG(dbgs() << "      Dependency variable: ");
    //         PrintingPolicy Policy(Context.getLangOpts());
    //         DepExpr->printPretty(llvm::outs(), nullptr, Policy);
    //         LLVM_DEBUG(dbgs() << "\n");
    //       }
    //     } break;
    //     case omp::OMPC_if: {
    //       auto *IfClause = cast<OMPIfClause>(Clause);
    //       LLVM_DEBUG(dbgs() << "      Condition: ");
    //       PrintingPolicy Policy(Context.getLangOpts());
    //       IfClause->getCondition()->printPretty(llvm::outs(), nullptr,
    //       Policy); LLVM_DEBUG(dbgs() << "\n");
    //     } break;
    //     default:
    //       LLVM_DEBUG(dbgs() << "      (Unhandled clause type)\n");
    //       break;
    //     }
    //   }
  }

  void storeDirective(OMPDirectiveInfo *Info,
                      const OMPExecutableDirective *Directive) {
    StringRef FunctionName = CurrentFn->getName();
    unsigned Line = SM.getSpellingLineNumber(Directive->getBeginLoc());
    LineToDirective[FunctionName.str()][Line] = Info;
  }
};

/// AST Consumer to handle the traversal
class OpenMPASTConsumer : public ASTConsumer {
public:
  explicit OpenMPASTConsumer(ASTContext &Context,
                             SetVector<Function *> &Functions)
      : Visitor(Context, Functions) {}

  void HandleTranslationUnit(ASTContext &Context) override {
    Visitor.TraverseDecl(Context.getTranslationUnitDecl());
    Visitor.preprocessLLVMIR();
  }

  const OMPDirectiveInfo *getDirectiveForInstruction(Instruction *I) const {
    return Visitor.getDirectiveForInstruction(I);
  }

private:
  OpenMPVisitor Visitor;
};

class OMPVisitor {
public:
  OMPVisitor() = default;
  ~OMPVisitor() {
    if (Consumer)
      delete Consumer;
  }

  void run(llvm::Module &M, SetVector<Function *> &Functions,
           string InputASTFilename) {
    /// AST Traversal with OpenMPVisitor and Clang tooling
    LLVM_DEBUG(
        dbgs() << "\n-------------------------------------------------\n"
               << TAG << "Running OMPVisitor on Module: \n"
               << M.getName() << "\n"
               << "\n-------------------------------------------------\n");

    /// Use the command-line option for the AST filename
    string Filename = InputASTFilename;

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
    shared_ptr<clang::PCHContainerOperations> PCHOps =
        make_shared<clang::PCHContainerOperations>();
    const clang::PCHContainerReader &PCHContainerRdr = PCHOps->getRawReader();
    shared_ptr<clang::HeaderSearchOptions> HSOpts =
        make_shared<clang::HeaderSearchOptions>();

    // Load the AST file with the correct number of arguments
    unique_ptr<ASTUnit> AST = ASTUnit::LoadFromASTFile(
        Filename, PCHContainerRdr, ASTUnit::LoadASTOnly, Diags, FileSystemOpts,
        HSOpts);

    if (!AST) {
      LLVM_DEBUG(dbgs() << "Error: Could not load AST file '" << Filename
                        << "'\n");
      assert(false && "Error: Could not load AST file");
    }

    /// Traverse the AST
    ASTContext &Context = AST->getASTContext();
    Consumer = new OpenMPASTConsumer(Context, Functions);
    Consumer->HandleTranslationUnit(Context);
  }

  const OMPDirectiveInfo *getDirectiveForInstruction(Instruction *I) const {
    return Consumer->getDirectiveForInstruction(I);
  }

private:
  OpenMPASTConsumer *Consumer;
};
