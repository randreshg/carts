/// AST Visitor to traverse the AST and find OpenMP directives

#include "clang/Basic/OpenMPKinds.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/ErrorHandling.h"
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
#include <sstream>

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

StringRef OMPDirectiveInfo::setType(StringRef Type) {
  return DirectiveType = Type;
}

/// Store transformation data
void OMPDirectiveInfo::setTransformation(
    clang::SourceRange FullRange, clang::CapturedStmt *CapturedStatement,
    SmallVector<OMPDirectiveInfo::CapturedVarInfo, 4> &CapturedVars,
    string &Dependencies) {
  this->FullRange = FullRange;
  this->CapturedStatement = CapturedStatement;
  this->Dependencies = std::move(Dependencies);
  this->CapturedVars = std::move(CapturedVars);
  this->HasTransformation = true;
}

bool OMPDirectiveInfo::hasTransformation() const { return HasTransformation; }

SmallVector<OMPDirectiveInfo::CapturedVarInfo, 4> &
OMPDirectiveInfo::getCapturedVars() {
  return CapturedVars;
}

SourceRange OMPDirectiveInfo::getFullRange() const { return FullRange; }

CapturedStmt *OMPDirectiveInfo::getCapturedStmt() const {
  return CapturedStatement;
}

string OMPDirectiveInfo::getDependencies() const { return Dependencies; }

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
  // switch (DepKind) {
  // case OMPC_DEPEND_in:
  //   Inputs.push_back(DepExpr);
  //   break;
  // case OMPC_DEPEND_out:
  //   Outputs.push_back(DepExpr);
  //   break;
  // case OMPC_DEPEND_inout:
  //   Inputs.push_back(DepExpr);
  //   Outputs.push_back(DepExpr);
  //   break;
  // default:
  //   break;
  // }
}

OMPDirectiveInfo *
OMPTaskInfo::handleDirective(OMPExecutableDirective *Directive) {
  SourceLocation Loc = Directive->getBeginLoc();
  OMPTaskInfo *Info = new OMPTaskInfo(Loc);

  OMPTaskDirective *TaskDirective = dyn_cast<OMPTaskDirective>(Directive);
  if (!TaskDirective)
    return nullptr;
  return Info;
}

/// ------------------------------------------------------------------- ///
///                           OpenMPVisitor                             ///
/// ------------------------------------------------------------------- ///
static unsigned OutlinedFuncCounter = 0;
OpenMPVisitor::OpenMPVisitor(ASTContext &Context, Rewriter &R)
    : Context(Context), SM(Context.getSourceManager()), R(R) {
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

  string AllFuncDecls, AllFuncDefs;
  for (OMPDirectiveInfo *Info : TransformedDirectives) {
    if (!Info->hasTransformation())
      continue;

    string FuncName = "edt_function_" + std::to_string(++OutlinedFuncCounter);
    string Annotation =
        "omp." + Info->getType().str() + Info->getDependencies();
    string FuncAnnotation = "__attribute__((annotate(\"" + Annotation + "\")))";

    /// Build function declaration
    string FuncDecl = "static void __attribute__((annotate(\"" + Annotation +
                      "\"))) " + FuncName + "(";
    bool First = true;
    for (const auto &[Type, Var, Annot] : Info->getCapturedVars()) {
      if (!First)
        FuncDecl += ", ";
      FuncDecl += Type + " " + Var;
      First = false;
    }

    /// Append the function declaration
    FuncDecl += ");\n";
    AllFuncDecls += FuncDecl;

    /// Build function definition
    string FuncDef = "static void __attribute__((annotate(\"" + Annotation +
                     "\"))) " + FuncName + "(";
    First = true;
    for (const auto &[Type, Var, Annot] : Info->getCapturedVars()) {
      if (!First)
        FuncDef += ", ";
      string VarAnnotation = " __attribute__((annotate(\"" + Annot + "\")))";
      FuncDef += "\n  " + Type + " " + Var + VarAnnotation;
      First = false;
    }
    FuncDef += ") {\n";

    /// Extract the function body from the captured range
    string BodyText = R.getRewrittenText(Info->getFullRange());
    string OutlinedBody;
    if (BodyText.size() > 2) {
      /// Remove the braces - Find opening and closing braces
      size_t OpenBrace = BodyText.find('{');
      size_t CloseBrace = BodyText.rfind('}');
      if (OpenBrace != StringRef::npos && CloseBrace != StringRef::npos) {
        OutlinedBody =
            BodyText.substr(OpenBrace + 1, CloseBrace - OpenBrace - 1);
      } else {
        /// No braces found, use the entire body without the pragma
        size_t Start = BodyText.find('\n');
        OutlinedBody = BodyText.substr(Start, BodyText.size() - 1);
      }
    }

    /// Append the body to the function definition and list
    FuncDef += OutlinedBody + "\n}\n\n";
    AllFuncDefs += FuncDef;

    /// Build function call
    string FnCall = FuncName + "(";
    First = true;
    for (const auto &[Type, Var, A] : Info->getCapturedVars()) {
      if (!First)
        FnCall += ", ";
      FnCall += Var;
      First = false;
    }
    FnCall += ");\n";

    /// Replace the directive with the function call
    R.ReplaceText(Info->getFullRange(), FnCall);

    /// Debug
    LLVM_DEBUG(dbgs() << "  Replaced directive at " << Info->getLocation(SM)
                      << " with call: " << FnCall << "\n"
                      << "  Outlined function declaration:\n"
                      << FuncDecl << "  Outlined function definition:\n"
                      << FuncDef << "\n");
  }

  /// Insert function declarations and definitions
  insertFunctions(AllFuncDecls, AllFuncDefs);
}

void OpenMPVisitor::handleDirective(OMPExecutableDirective *Directive) {
  OMPDirectiveInfo *Info = nullptr;
  switch (Directive->getDirectiveKind()) {
  case omp::OMPD_parallel: {
    LLVM_DEBUG(dbgs() << "- - - - - - - - - - - - - - - - - - - \n"
                      << "Parallel Directive found at "
                      << SM.getSpellingLineNumber(Directive->getBeginLoc())
                      << "\n");
    Info = OMPParallelInfo::handleDirective(Directive);
  } break;
  case omp::OMPD_task: {
    LLVM_DEBUG(dbgs() << "- - - - - - - - - - - - - - - - - - - \n"
                      << "Task Directive found at "
                      << SM.getSpellingLineNumber(Directive->getBeginLoc())
                      << "\n");
    Info = OMPTaskInfo::handleDirective(Directive);
  } break;
  case omp::OMPD_single: {
    LLVM_DEBUG(dbgs() << "- - - - - - - - - - - - - - - - - - - \n"
                      << "Single Directive found at "
                      << SM.getSpellingLineNumber(Directive->getBeginLoc())
                      << "\n");
    SourceLocation Loc = Directive->getBeginLoc();
    Info = new OMPDirectiveInfo("single", Loc);
  } break;
  case omp::OMPD_taskloop: {
    LLVM_DEBUG(dbgs() << "- - - - - - - - - - - - - - - - - - - \n"
                      << "Taskloop Directive found at "
                      << SM.getSpellingLineNumber(Directive->getBeginLoc())
                      << "\n");
  } break;
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
  /// Collect dependencies and captured variables
  string Dependencies;
  SmallVector<OMPDirectiveInfo::CapturedVarInfo, 4> CapturedVars;
  collectInfo(Directive, CapturedVars, Dependencies);

  /// Determine full range: from directive begin to captured stmt end
  CapturedStmt *CaptureStatement = Directive->getInnermostCapturedStmt();
  SourceLocation Start = Directive->getBeginLoc();
  SourceLocation End = Lexer::getLocForEndOfToken(
      CaptureStatement->getCapturedStmt()->getEndLoc(), 0, SM, LangOptions());
  SourceRange FullRange(Start, End);

  /// Store transformation data
  Info->setTransformation(FullRange, CaptureStatement, CapturedVars,
                          Dependencies);
  TransformedDirectives.push_back(Info);
}

void OpenMPVisitor::collectInfo(
    OMPExecutableDirective *Directive,
    SmallVector<OMPDirectiveInfo::CapturedVarInfo, 4> &CapturedVars,
    string &Dependencies) {
  map<string, OMPDirectiveInfo::CapturedVarInfo *> CapturedMap;
  SmallVector<OMPDependClause *, 4> DepsVector;

  /// Handle lifetime variables from OpenMP clauses
  for (const OMPClause *Clause : Directive->clauses()) {
    switch (Clause->getClauseKind()) {
    case llvm::omp::OMPC_private:
      processClause<OMPPrivateClause>(cast<OMPPrivateClause>(Clause),
                                      "omp.private", CapturedVars, CapturedMap);
      break;
    case llvm::omp::OMPC_firstprivate:
      processClause<OMPFirstprivateClause>(cast<OMPFirstprivateClause>(Clause),
                                           "omp.firstprivate", CapturedVars,
                                           CapturedMap);
      break;
    case llvm::omp::OMPC_lastprivate:
      processClause<OMPLastprivateClause>(cast<OMPLastprivateClause>(Clause),
                                          "omp.lastprivate", CapturedVars,
                                          CapturedMap);
      break;
    case llvm::omp::OMPC_shared:
      processClause<OMPSharedClause>(cast<OMPSharedClause>(Clause),
                                     "omp.shared", CapturedVars, CapturedMap);
      break;
    case omp::OMPC_depend:
      DepsVector.push_back(
          const_cast<OMPDependClause *>(cast<OMPDependClause>(Clause)));
      break;
    default:
      break;
    }
  }

  /// Handle captured variables from the captured statement
  CapturedStmt *CS = Directive->getInnermostCapturedStmt();
  for (const auto &Cap : CS->captures()) {
    const VarDecl *VD = Cap.getCapturedVar();
    auto Name = VD->getName().str();
    /// Only add the variable if it has not been captured already
    if (CapturedMap.find(Name) == CapturedMap.end()) {
      CapturedVars.emplace_back(VD->getType().getAsString(), Name,
                                "omp.default");
      CapturedMap[Name] = &CapturedVars.back();
    }
  }

  /// Handle dependencies, if any
  if (DepsVector.empty())
    return;

  /// Process dependencies.
  stringstream Input, Output;
  Input << " deps(in: ";
  Output << " deps(out: ";
  bool HasInput = false, HasOutput = false;

  for (OMPDependClause *DependClause : DepsVector) {
    OpenMPDependClauseKind DepKind = DependClause->getDependencyKind();
    for (const Expr *DepExpr : DependClause->varlists()) {
      string VarName;
      llvm::raw_string_ostream OS(VarName);
      DepExpr->printPretty(OS, nullptr, PrintingPolicy(Context.getLangOpts()));
      OS.flush();

      switch (DepKind) {
      case OMPC_DEPEND_in:
        Input << VarName << ", ";
        HasInput = true;
        break;
      case OMPC_DEPEND_out:
        Output << VarName << ", ";
        HasOutput = true;
        break;
      case OMPC_DEPEND_inout:
        Input << VarName << ", ";
        Output << VarName << ", ";
        HasInput = true;
        HasOutput = true;
        break;
      default:
        break;
      }
    }
  }

  if (HasInput)
    Dependencies += Input.str().substr(0, Input.str().size() - 2) + ")";
  if (HasOutput)
    Dependencies += (HasInput ? " " : "") +
                    Output.str().substr(0, Output.str().size() - 2) + ")";
}

void OpenMPVisitor::insertFunctions(string AllFuncDecls, string AllFuncDefs) {
  // Determine insertion points
  FileID MainFileID = SM.getMainFileID();
  // bool IsHeader = isHeaderFile(SM, MainFileID);

  string AllFuncDeclsWithNewlines =
      AllFuncDecls.empty() ? "" : "\n" + AllFuncDecls + "\n";
  string AllFuncDefsWithNewlines =
      AllFuncDefs.empty() ? "" : "\n" + AllFuncDefs;

  /// Insert function declarations after the last #include directive
  /// Retrieve the buffer content as a string
  bool Invalid = false;
  StringRef Buffer = SM.getBufferData(MainFileID, &Invalid);
  if (Invalid) {
    llvm::errs() << "Error: Unable to retrieve buffer data.\n";
    return;
  }

  /// Find the position of the last #include directive
  size_t LastIncludePos = Buffer.rfind("#include");
  SourceLocation InsertDeclLoc;

  if (LastIncludePos != StringRef::npos) {
    /// Find the end of the last #include line
    size_t InsertPos = Buffer.find('\n', LastIncludePos);
    if (InsertPos != StringRef::npos) {
      InsertDeclLoc =
          SM.getLocForStartOfFile(MainFileID).getLocWithOffset(InsertPos + 1);
    } else {
      /// If no newline found after #include, insert at the end
      InsertDeclLoc = SM.getLocForEndOfFile(MainFileID);
    }
  } else {
    /// If no #include directives, insert at the beginning
    InsertDeclLoc = SM.getLocForStartOfFile(MainFileID);
  }

  /// Insert function declarations after the last #include or at the beginning
  R.InsertText(InsertDeclLoc, AllFuncDeclsWithNewlines, true, false);

  /// Insert function definitions at the end of the file
  R.InsertText(SM.getLocForEndOfFile(MainFileID), AllFuncDefsWithNewlines, true,
               false);
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