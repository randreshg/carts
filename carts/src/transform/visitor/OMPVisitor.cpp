/// AST Visitor to traverse the AST and find OpenMP directives

#include "llvm/IR/Instruction.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Support/raw_ostream.h"

#include "carts/transform/visitor/OMPVisitor.h"

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

/// DEBUG
#define DEBUG_TYPE "omp-visitor"
#if !defined(NDEBUG)
static constexpr auto TAG = "[" DEBUG_TYPE "] ";
#endif

namespace arts {
using namespace llvm;
using namespace std;
using namespace clang;

/// ------------------------------------------------------------------- ///
///                          OMPDirectiveInfo                           ///
/// ------------------------------------------------------------------- ///
OMPDirectiveInfo::OMPDirectiveInfo(StringRef Type, SourceLocation Loc)
    : DirectiveType(Type), Location(Loc) {}

void OMPDirectiveInfo::addChild(OMPDirectiveInfo *Child) {
  Child->Parent = this;
  Children.push_back(Child);
}

StringRef OMPDirectiveInfo::getType() const { return DirectiveType; }

string OMPDirectiveInfo::getLocation(const SourceManager &SM) const {
  return Location.printToString(SM);
}

const SmallVector<OMPDirectiveInfo *, 4> &
OMPDirectiveInfo::getChildren() const {
  return Children;
}

const OMPDirectiveInfo *OMPDirectiveInfo::getParent() const { return Parent; }

Instruction *OMPDirectiveInfo::getIRInstruction() const {
  return IRInstruction;
}

void OMPDirectiveInfo::setIRInstruction(Instruction *IRInstruction) {
  this->IRInstruction = IRInstruction;
}

/// ------------------------------------------------------------------- ///
///                        ParallelDirectiveInfo                        ///
/// ------------------------------------------------------------------- ///
OMPParallelInfo::OMPParallelInfo(SourceLocation Loc)
    : OMPDirectiveInfo("parallel", Loc) {}

unsigned OMPParallelInfo::getNumThreads() const { return NumThreads; }

OMPDirectiveInfo *
OMPParallelInfo::handleDirective(OMPExecutableDirective *Directive) {
  SourceLocation Loc = Directive->getBeginLoc();
  OMPParallelInfo *Info = new OMPParallelInfo(Loc);
  return Info;
}
/// ------------------------------------------------------------------- ///
///                          TaskDirectiveInfo                          ///
/// ------------------------------------------------------------------- ///
OMPTaskInfo::OMPTaskInfo(SourceLocation Loc) : OMPDirectiveInfo("task", Loc) {}

bool OMPTaskInfo::hasDependencies() const {
  return !Inputs.empty() || !Outputs.empty();
}

void OMPTaskInfo::addDependency(OpenMPDependClauseKind DepKind, Expr *DepExpr) {
  switch (DepKind) {
  case OMPC_DEPEND_in:
    Inputs.push_back(DepExpr);
    break;
  case OMPC_DEPEND_out:
    Outputs.push_back(DepExpr);
    break;
  case OMPC_DEPEND_inout:
    Inputs.push_back(DepExpr);
    Outputs.push_back(DepExpr);
    break;
  default:
    break;
  }
}

OMPDirectiveInfo *
OMPTaskInfo::handleDirective(OMPExecutableDirective *Directive) {
  SourceLocation Loc = Directive->getBeginLoc();
  OMPTaskInfo *Info = new OMPTaskInfo(Loc);

  OMPTaskDirective *TaskDirective = dyn_cast<OMPTaskDirective>(Directive);
    if (!TaskDirective)
      return nullptr;

    LLVM_DEBUG(dbgs() << "- - - - - - - - - - - - - - - \n"
                      << "  Processing task directive...\n");
    /// Process clauses
    for (const OMPClause *Clause : TaskDirective->clauses()) {
      switch (Clause->getClauseKind()) {
      case omp::OMPC_depend: {
        auto *DependClause = cast<OMPDependClause>(Clause);
        LLVM_DEBUG(
            dbgs() << "    Dependency type: "
                   << OpenMPDependClauseKind(DependClause->getDependencyKind())
                   << "\n");
        for (const Expr *DepExpr : DependClause->varlists()) {
          LLVM_DEBUG(dbgs() << "    Dependency variable: " << DepExpr << "\n");
        }
      } break;

      case omp::OMPC_if: {
        auto *IfClause = cast<OMPIfClause>(Clause);
        LLVM_DEBUG(dbgs() << "    Condition: " << IfClause->getCondition()
                          << "\n");
      } break;

      default:
        LLVM_DEBUG(dbgs() << "    (Unhandled clause type)\n");
        break;
      }

    }
    return Info;
  /// Handle task dependencies
  return Info;
}

/// ------------------------------------------------------------------- ///
///                            OpenMPVisitor                            ///
/// ------------------------------------------------------------------- ///
class OpenMPVisitor : public RecursiveASTVisitor<OpenMPVisitor> {
public:
  explicit OpenMPVisitor(ASTContext &Context, SetVector<Function *> &Functions)
      : Functions(Functions), SM(Context.getSourceManager()) {}

  ~OpenMPVisitor() {
    /// Free all directive info objects
    for (auto &Line : LineToDirective)
      delete Line.second;
  }

  /// Traverse the AST and find OpenMP directives
  bool TraverseStmt(Stmt *S) {
    RecursiveASTVisitor::TraverseStmt(S);
    if (!S)
      return true;
    /// Pop the directive stack if we are at the end of a directive
    if (OMPExecutableDirective *Directive =
            dyn_cast<OMPExecutableDirective>(S)) {
      if (!DirectiveStack.empty())
        DirectiveStack.pop_back();
    }
    return true;
  }

  bool VisitStmt(Stmt *S) {
    /// We are only interested in OpenMP directives
    OMPExecutableDirective *Directive = dyn_cast<OMPExecutableDirective>(S);
    if (!Directive)
      return true;

    LLVM_DEBUG(dbgs() << "Found OpenMP Directive ("
                      << Directive->getStmtClassName() << ") at "
                      << Directive->getBeginLoc().printToString(SM) << "\n");
    handleDirective(Directive);
    return true;
  }

  /// Maps LLVM IR instructions to frontend directives based on debug metadata
  /// (DILocation).
  void preprocessLLVMIR() {
    for (Function *F : Functions) {
      for (Instruction &I : instructions(F)) {
        if (DILocation *Loc = I.getDebugLoc()) {
          auto LineIt = LineToDirective.find(Loc->getLine());
          if (LineIt != LineToDirective.end()) {
            OMPDirectiveInfo *Directive = LineIt->second;
            Directive->setIRInstruction(&I);
            InstructionToDirective[&I] = Directive;
          }
        }
      }
    }
  }

  /// Get the directive associated with an LLVM IR instruction
  const OMPDirectiveInfo *getDirectiveForInstruction(Instruction *I) const {
    auto It = InstructionToDirective.find(I);
    if (It != InstructionToDirective.end())
      return It->second;
    return nullptr;
  }

  /// Print the directive hierarchy
  void printHierarchy() const {
    LLVM_DEBUG(dbgs() << "- - - - - - - - - - - - - - - - - - - \n"
                      << "Printing OpenMP Directive Hierarchy...\n");
    for (const auto &Root : RootDirectives)
      Root->printInfo(llvm::outs(), SM);
  }

private:
  // ASTContext &Context;
  SetVector<Function *> &Functions;
  const SourceManager &SM;

  SmallVector<OMPDirectiveInfo *, 4> DirectiveStack;
  SmallVector<OMPDirectiveInfo *, 4> RootDirectives;

  DenseMap<unsigned, OMPDirectiveInfo *> LineToDirective;
  DenseMap<Instruction *, OMPDirectiveInfo *> InstructionToDirective;

  void handleDirective(OMPExecutableDirective *Directive) {
    OMPDirectiveInfo *Info = nullptr;
    switch (Directive->getDirectiveKind()) {
    case omp::OMPD_parallel:
      Info = OMPParallelInfo::handleDirective(Directive);
      break;
    case omp::OMPD_task:
      Info = OMPTaskInfo::handleDirective(Directive);
      break;
    default:
      LLVM_DEBUG(dbgs() << "  (Unhandled directive type)\n");
      break;
    }

    if (!Info)
      return;

    /// If the directive stack is not empty, add the directive as a child of the
    /// top directive in the stack
    if (!DirectiveStack.empty())
      DirectiveStack.back()->addChild(Info);
    /// Otherwise, add the directive to the root directives
    else
      RootDirectives.push_back(Info);

    /// Push the new directive onto the stack
    DirectiveStack.push_back(RootDirectives.back());

    /// Store the directive in the map
    storeDirective(Info, Directive);
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
    unsigned Line = SM.getSpellingLineNumber(Directive->getBeginLoc());
    LineToDirective[Line] = Info;
  }
};

/// ------------------------------------------------------------------- ///
///                           OpenMPASTConsumer                         ///
/// ------------------------------------------------------------------- ///
class OpenMPASTConsumer : public ASTConsumer {
public:
  explicit OpenMPASTConsumer(ASTContext &Context,
                             SetVector<Function *> &Functions)
      : Visitor(Context, Functions) {}

  void HandleTranslationUnit(ASTContext &Context) override {
    Visitor.TraverseDecl(Context.getTranslationUnitDecl());
    Visitor.preprocessLLVMIR();
    Visitor.printHierarchy();
  }

  const OMPDirectiveInfo *getDirectiveForInstruction(Instruction *I) const {
    return Visitor.getDirectiveForInstruction(I);
  }

private:
  OpenMPVisitor Visitor;
};

/// ------------------------------------------------------------------- ///
///                              OMPVisitor                             ///
/// ------------------------------------------------------------------- ///
OMPVisitor::OMPVisitor(llvm::Module &M, SetVector<Function *> &Functions)
    : M(M), Functions(Functions), Consumer(nullptr) {}

OMPVisitor::~OMPVisitor() {
  if (Consumer)
    delete Consumer;
}

void OMPVisitor::run(string InputASTFilename) {
  /// AST Traversal with OpenMPVisitor and Clang tooling
  LLVM_DEBUG(dbgs() << TAG << "Running OMPVisitor\n");

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
  unique_ptr<ASTUnit> AST =
      ASTUnit::LoadFromASTFile(Filename, PCHContainerRdr, ASTUnit::LoadASTOnly,
                               Diags, FileSystemOpts, HSOpts);

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

const OMPDirectiveInfo *
OMPVisitor::getDirectiveForInstruction(Instruction *I) const {
  return Consumer->getDirectiveForInstruction(I);
}

} // namespace arts
