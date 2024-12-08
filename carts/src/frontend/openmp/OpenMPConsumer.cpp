/// AST Visitor to traverse the AST and find OpenMP directives

#include "clang/Rewrite/Core/Rewriter.h"
#include "llvm/IR/Instruction.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Support/raw_ostream.h"

#include "carts/frontend/openmp/OpenMPConsumer.h"

#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/AST/StmtOpenMP.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/DiagnosticIDs.h"
#include "clang/Frontend/ASTUnit.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/TextDiagnosticPrinter.h"
#include "llvm/IR/Instructions.h"

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
///                           OpenMPVisitor                             ///
/// ------------------------------------------------------------------- ///
static unsigned OutlinedFuncCounter = 0;
OpenMPVisitor::OpenMPVisitor(ASTContext &Context, Rewriter &R)
    : SM(Context.getSourceManager()), R(R) {
  MainFileID = R.getSourceMgr().getMainFileID();
}

OpenMPVisitor::~OpenMPVisitor() {
  /// Free all directive info objects
  for (auto &Line : LineToDirective)
    delete Line.second;
}

bool OpenMPVisitor::TraverseStmt(Stmt *S) {
  RecursiveASTVisitor::TraverseStmt(S);
  if (!S)
    return true;
  /// Pop the directive stack if we are at the end of a directive
  if (OMPExecutableDirective *Directive = dyn_cast<OMPExecutableDirective>(S)) {
    if (!DirectiveStack.empty())
      DirectiveStack.pop_back();
  }
  return true;
}

bool OpenMPVisitor::VisitStmt(Stmt *S) {
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

void OpenMPVisitor::printHierarchy() const {
  LLVM_DEBUG(dbgs() << "- - - - - - - - - - - - - - - - - - - \n"
                    << "Printing OpenMP Directive Hierarchy...\n");
  for (const auto &Root : RootDirectives)
    Root->printInfo(llvm::outs(), SM);
}

void OpenMPVisitor::handleDirective(OMPExecutableDirective *Directive) {
  OMPDirectiveInfo *Info = nullptr;
  switch (Directive->getDirectiveKind()) {
  case omp::OMPD_parallel:
    LLVM_DEBUG(dbgs() << "Parallel Directive found at "
                      << SM.getSpellingLineNumber(Directive->getBeginLoc())
                      << "\n");
    Info = OMPParallelInfo::handleDirective(Directive);
    break;
  case omp::OMPD_task:
    LLVM_DEBUG(dbgs() << "Task Directive found at "
                      << SM.getSpellingLineNumber(Directive->getBeginLoc())
                      << "\n");
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

void OpenMPVisitor::storeDirective(OMPDirectiveInfo *Info,
                                   const OMPExecutableDirective *Directive) {
  unsigned Line = SM.getSpellingLineNumber(Directive->getBeginLoc());
  LineToDirective[Line] = Info;
}

void OpenMPVisitor::handleClauses(const OMPExecutableDirective *Directive) {
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

void OpenMPVisitor::handleCaptureStmt(OMPParallelDirective *Node,
                                      CapturedStmt *CS) {
  // CapturedDecl *CD = CS->getCapturedDecl();
  string FuncName =
      "outlined_parallel_region_" + to_string(++OutlinedFuncCounter);

  // Extract body
  SourceRange BodyRange = CS->getCapturedStmt()->getSourceRange();
  StringRef BodyText = getSourceText(BodyRange);
  string OutlinedBody;
  if (isa<CompoundStmt>(CS->getCapturedStmt()) && BodyText.size() > 2) {
    OutlinedBody = BodyText.substr(1, BodyText.size() - 2).str();
  } else {
    OutlinedBody = BodyText.str();
  }

  // Build function signature with captured variables
  string FuncSig =
      "static void __attribute__((annotate(\"parallel\"))) " + FuncName + "(";

  bool first = true;
  for (const auto &Cap : CS->captures()) {
    const VarDecl *VD = Cap.getCapturedVar();
    QualType QT = VD->getType();
    if (!first)
      FuncSig += ", ";
    FuncSig += QT.getAsString() + " " + VD->getName().str();
    first = false;
  }
  FuncSig += ") {\n" + OutlinedBody + "\n}\n";

  // Insert function at end of file
  insertAtFileEnd(FuncSig);

  // Build call expression
  string CallExpr = FuncName + "(";
  first = true;
  for (const auto &Cap : CS->captures()) {
    const VarDecl *VD = Cap.getCapturedVar();
    if (!first)
      CallExpr += ", ";
    CallExpr += VD->getName().str();
    first = false;
  }
  CallExpr += ");";

  // Replace directive
  R.ReplaceText(Node->getSourceRange(), CallExpr);
}

StringRef OpenMPVisitor::getSourceText(SourceRange SR) const {
  const SourceManager &SM = R.getSourceMgr();
  LangOptions LangOpts;
  return Lexer::getSourceText(CharSourceRange::getCharRange(SR), SM, LangOpts);
}

void OpenMPVisitor::insertAtFileEnd(const string &Text) {
  SourceLocation EndLoc = R.getSourceMgr().getLocForEndOfFile(MainFileID);
  R.InsertText(EndLoc, "\n" + Text, true, false);
}

/// ------------------------------------------------------------------- ///
///                           OpenMPASTConsumer                         ///
/// ------------------------------------------------------------------- ///
OpenMPConsumer::OpenMPConsumer(ASTContext &Context, Rewriter &R)
    : Visitor(Context, R) {}

void OpenMPConsumer::HandleTranslationUnit(ASTContext &Context) {
  Visitor.TraverseDecl(Context.getTranslationUnitDecl());
  Visitor.printHierarchy();
}

} // namespace arts