/// Description: AST Visitor to traverse the AST and find OpenMP directives

#ifndef LLVM_API_CARTS_OMPVISITOR_H
#define LLVM_API_CARTS_OMPVISITOR_H

#include "clang/AST/Decl.h"
#include "clang/AST/StmtOpenMP.h"
#include "clang/Basic/SourceLocation.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/IR/Instruction.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Support/raw_ostream.h"

#include "clang/AST/Expr.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/DiagnosticIDs.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include <string>

namespace arts {
using namespace llvm;
using namespace std;

/// Base class for all OpenMP directives
class OMPDirectiveInfo {
public:
  OMPDirectiveInfo(StringRef Type, clang::SourceLocation Loc);
  virtual ~OMPDirectiveInfo() = default;

  void addChild(OMPDirectiveInfo *Child);

  StringRef getType() const;
  clang::OMPExecutableDirective *getDirective() const;
  string getLocation(const clang::SourceManager &SM) const;
  const SmallVector<OMPDirectiveInfo *, 4> &getChildren() const;
  const OMPDirectiveInfo *getParent() const;

  /// Transformation data
  struct CapturedVarInfo {
    std::string Type;
    std::string Name;
    std::string Annotation;

    CapturedVarInfo(std::string Type, std::string Name, std::string Annotation)
        : Type(std::move(Type)), Name(std::move(Name)),
          Annotation(std::move(Annotation)) {}
  };

  void setTransformation(clang::SourceRange FullRange,
                         clang::CapturedStmt *CapturedStatement,
                         SmallVector<CapturedVarInfo, 4> &CapturedVars);
  bool hasTransformation() const;

  SmallVector<CapturedVarInfo, 4> &getCapturedVars();
  clang::SourceRange getFullRange() const;
  clang::CapturedStmt *getCapturedStmt() const;

  virtual void printInfo(raw_ostream &OS, const clang::SourceManager &SM,
                         unsigned Depth = 0) const {
    OS.indent(Depth * 2) << "Directive: " << DirectiveType << "\n";
    OS.indent(Depth * 2 + 2) << "Location: " << getLocation(SM) << "\n";
    if (IRInstruction)
      OS.indent(Depth * 2 + 2) << "Instruction: " << *IRInstruction << "\n";
    for (const auto &Child : Children)
      Child->printInfo(OS, SM, Depth + 1);
  }

protected:
  StringRef DirectiveType = "omp";
  clang::SourceLocation Location;
  OMPDirectiveInfo *Parent;
  Instruction *IRInstruction = nullptr;
  SmallVector<OMPDirectiveInfo *, 4> Children;

  /// Transformation details
  clang::SourceRange FullRange;
  clang::CapturedStmt *CapturedStatement;
  SmallVector<CapturedVarInfo, 4> CapturedVars;
  bool HasTransformation = false;
};

/// Parallel directive info
class OMPParallelInfo : public OMPDirectiveInfo {
public:
  OMPParallelInfo(clang::SourceLocation Loc);
  static OMPDirectiveInfo *
  handleDirective(clang::OMPExecutableDirective *Directive);

  unsigned getNumThreads() const;

  void printInfo(raw_ostream &OS, const clang::SourceManager &SM,
                 unsigned Depth = 0) const override {
    OMPDirectiveInfo::printInfo(OS, SM, Depth);
    OS.indent(Depth * 2 + 2) << "Number of Threads: " << NumThreads << "\n";
    if (ForConstruct) {
      OS.indent(Depth * 2 + 2) << "Parallel For: Yes\n";
    }
  }

private:
  unsigned NumThreads = 1;
  bool ForConstruct = false;
};

/// Task directive info
class OMPTaskInfo : public OMPDirectiveInfo {
public:
  OMPTaskInfo(clang::SourceLocation Loc);
  static OMPDirectiveInfo *
  handleDirective(clang::OMPExecutableDirective *Directive);

  bool hasDependencies() const;

  void addDependency(clang::OpenMPDependClauseKind DepKind,
                     clang::Expr *DepExpr);

  void printInfo(raw_ostream &OS, const clang::SourceManager &SM,
                 unsigned Depth = 0) const override {
    OMPDirectiveInfo::printInfo(OS, SM, Depth);
    OS.indent(Depth * 2 + 2)
        << "Dependencies: " << (Dependencies ? "Yes" : "No") << "\n";
  }

private:
  bool Dependencies = false;
  SmallVector<clang::Expr *, 4> Inputs;
  SmallVector<clang::Expr *, 4> Outputs;
};

/// OpenMP AST Visitor
class OpenMPVisitor : public clang::RecursiveASTVisitor<OpenMPVisitor> {
public:
  explicit OpenMPVisitor(clang::ASTContext &Context, clang::Rewriter &R);
  ~OpenMPVisitor();

  bool TraverseStmt(clang::Stmt *S);
  bool VisitStmt(clang::Stmt *S);
  void printHierarchy() const;
  void applyTransformations();

private:
  void handleDirective(clang::OMPExecutableDirective *Directive);
  void handleClauses(const clang::OMPExecutableDirective *Directive);
  void handleCaptureStmt(clang::OMPExecutableDirective *Directive,
                         OMPDirectiveInfo *Info);
  template <typename ClauseType>
  void
  processClause(const ClauseType *Clause, const std::string &Annotation,
                SmallVector<OMPDirectiveInfo::CapturedVarInfo, 4> &CapturedVars,
                std::set<std::string> &CapturedSet) {
    /// Iterate over each DeclRefExpr in the clause's variable list
    // Iterate over each Expr* in the clause's variable list
    for (const clang::Expr *E : Clause->varlists()) {
      if (const clang::DeclRefExpr *DRE =
              clang::dyn_cast<clang::DeclRefExpr>(E)) {
        if (const clang::VarDecl *VD =
                clang::dyn_cast<clang::VarDecl>(DRE->getDecl())) {
          /// Store the variable's type and name
          std::string Name = VD->getName().str();
          if (CapturedSet.insert(Name).second)
            CapturedVars.emplace_back(VD->getType().getAsString(), Name,
                                      Annotation);
        }
      }
    }
  }
  void collectCapturedVars(
      clang::OMPExecutableDirective *Directive,
      SmallVector<OMPDirectiveInfo::CapturedVarInfo, 4> &CapturedVars);
  void insertFunctions(std::string AllFuncDecls, std::string AllFuncDefs);

  /// Attributes
  const clang::SourceManager &SM;
  clang::Rewriter &R;
  clang::FileID MainFileID;

  SmallVector<OMPDirectiveInfo *, 4> DirectiveStack;
  SmallVector<OMPDirectiveInfo *, 4> RootDirectives;
  SmallVector<OMPDirectiveInfo *, 4> Directives;
  SmallVector<OMPDirectiveInfo *, 4> TransformedDirectives;
};

/// Visitor to traverse the AST and find OpenMP directives
class OpenMPConsumer : public clang::ASTConsumer {
public:
  explicit OpenMPConsumer(clang::ASTContext &Context, clang::Rewriter &R);

  void HandleTranslationUnit(clang::ASTContext &Context) override;

private:
  OpenMPVisitor Visitor;
};

} // namespace arts
#endif // LLVM_API_CARTS_OMPVISITOR_H