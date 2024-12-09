/// AST Visitor to traverse the AST and find OpenMP directives

#include "clang/Rewrite/Core/Rewriter.h"
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
#define DEBUG_TYPE "omp-plugin"
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

/// Store transformation data
void OMPDirectiveInfo::setTransformation(
    clang::SourceRange FullRange,
    SmallVector<std::pair<std::string, std::string>, 4> CapturedVars,
    bool IsCompoundStmt) {
  this->FullRange = FullRange;
  this->CapturedVars = std::move(CapturedVars);
  this->HasTransformation = true;
  this->IsCompoundStmt = IsCompoundStmt;
}

bool OMPDirectiveInfo::hasTransformation() const { return HasTransformation; }

bool OMPDirectiveInfo::isCompoundStmt() const { return IsCompoundStmt; }

// const std::string &OMPDirectiveInfo::getFnSignature() const {
//   return FnSignature;
// }

// const std::string &OMPDirectiveInfo::getFnCall() const { return FnCall; }

SmallVector<std::pair<std::string, std::string>, 4> &
OMPDirectiveInfo::getCapturedVars() {
  return CapturedVars;
}

SourceRange OMPDirectiveInfo::getFullRange() const { return FullRange; }

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
      const OMPDependClause *DependClause = cast<OMPDependClause>(Clause);
      LLVM_DEBUG(
          dbgs() << "    Dependency type: "
                 << OpenMPDependClauseKind(DependClause->getDependencyKind())
                 << "\n");
      for (const Expr *DepExpr : DependClause->varlists()) {
        LLVM_DEBUG(dbgs() << "    Dependency variable: " << DepExpr << "\n");
      }
    } break;

    case omp::OMPC_if: {
      const OMPIfClause *IfClause = cast<OMPIfClause>(Clause);
      LLVM_DEBUG(dbgs() << "    Condition: " << IfClause->getCondition()
                        << "\n");
    } break;

    default:
      // LLVM_DEBUG(dbgs() << "    (Unhandled clause type)\n");
      break;
    }
  }
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
  for (OMPDirectiveInfo *Directive : Directives)
    delete Directive;
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

void OpenMPVisitor::applyTransformations() {
  /// Sort by starting location descending so that inner directives are replaced
  /// first
  const SourceManager &SM = R.getSourceMgr();
  std::sort(TransformedDirectives.begin(), TransformedDirectives.end(),
            [&SM](OMPDirectiveInfo *A, OMPDirectiveInfo *B) {
              return SM.getFileOffset(A->getFullRange().getBegin()) >
                     SM.getFileOffset(B->getFullRange().getBegin());
            });

  /// Collect all function signatures to insert at end
  std::string AllFuncSigs;

  for (OMPDirectiveInfo *Info : TransformedDirectives) {
    if (!Info->hasTransformation())
      continue;

    /// Build function signature
    std::string FuncName =
        "outlined_region_" + std::to_string(++OutlinedFuncCounter);
    std::string Annotation = "edt." + Info->getType().str();
    std::string FnSignature = "static void __attribute__((annotate(\"" +
                              Annotation + "\"))) " + FuncName + "(";

    bool First = true;
    for (const auto &[Type, Var] : Info->getCapturedVars()) {
      if (!First)
        FnSignature += ", ";
      FnSignature += Type + " " + Var;
      First = false;
    }
    FnSignature += ") {\n";

    /// Extract the function body from the captured range
    std::string BodyText = R.getRewrittenText(Info->getFullRange());
    /// Strip the braces if it's a compound statement
    /// Assuming the body is a compound statement { ... }
    std::string OutlinedBody;
    if (Info->isCompoundStmt()) {
      if (BodyText.size() > 2)
        OutlinedBody = BodyText.substr(1, BodyText.size() - 2);
      else
        OutlinedBody = BodyText;
    } else
      OutlinedBody = BodyText;
    FnSignature += OutlinedBody + "\n}\n";

    /// Build function call
    std::string FnCall = FuncName + "(";
    First = true;
    for (const auto &[Type, Var] : Info->getCapturedVars()) {
      if (!First)
        FnCall += ", ";
      FnCall += Var;
      First = false;
    }
    FnCall += ");\n";

    /// Replace the directive with the function call
    R.ReplaceText(Info->getFullRange(), FnCall);

    /// Collect all function signatures
    AllFuncSigs += "\n" + FnSignature;

    LLVM_DEBUG(dbgs() << "  Replaced directive at " << Info->getLocation(SM)
                      << " with call: " << FnCall << "\n");
    LLVM_DEBUG(dbgs() << "  Outlined function:\n" << FnSignature << "\n");
  }
  // Insert all function signatures at end
  if (!AllFuncSigs.empty()) {
    SourceLocation EndLoc = R.getSourceMgr().getLocForEndOfFile(MainFileID);
    R.InsertText(EndLoc, AllFuncSigs, true, false);
  }
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
  Directives.push_back(Info);

  /// Handle capture statements
  if (Directive->hasAssociatedStmt())
    handleCaptureStmt(Directive, Info);
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

void OpenMPVisitor::handleCaptureStmt(OMPExecutableDirective *Directive,
                                      OMPDirectiveInfo *Info) {
  /// Collect captured variables with their types
  SmallVector<std::pair<std::string, std::string>, 4> CapturedVars;
  CapturedStmt *CS = Directive->getInnermostCapturedStmt();
  for (const auto &Cap : CS->captures()) {
    const VarDecl *VD = Cap.getCapturedVar();
    CapturedVars.emplace_back(VD->getType().getAsString(), VD->getName().str());
  }

  /// Determine full range: from directive begin to captured stmt end
  SourceLocation Start = Directive->getBeginLoc();
  SourceLocation End = Lexer::getLocForEndOfToken(
      CS->getCapturedStmt()->getEndLoc(), 0, SM, LangOptions());
  SourceRange FullRange(Start, End);

  /// Store transformation data
  Info->setTransformation(FullRange, CapturedVars, isa<CompoundStmt>(CS));
  TransformedDirectives.push_back(Info);
}

/// ------------------------------------------------------------------- ///
///                           OpenMPASTConsumer                         ///
/// ------------------------------------------------------------------- ///
OpenMPConsumer::OpenMPConsumer(ASTContext &Context, Rewriter &R)
    : Visitor(Context, R) {}

void OpenMPConsumer::HandleTranslationUnit(ASTContext &Context) {
  Visitor.TraverseDecl(Context.getTranslationUnitDecl());
  Visitor.printHierarchy();
  Visitor.applyTransformations();
}

} // namespace arts