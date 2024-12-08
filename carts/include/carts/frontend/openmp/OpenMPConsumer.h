/// Description: AST Visitor to traverse the AST and find OpenMP directives

#ifndef LLVM_API_CARTS_OMPVISITOR_H
#define LLVM_API_CARTS_OMPVISITOR_H

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
  string getLocation(const clang::SourceManager &SM) const;
  const SmallVector<OMPDirectiveInfo *, 4> &getChildren() const;
  const OMPDirectiveInfo *getParent() const;
  Instruction *getIRInstruction() const;

  void setIRInstruction(Instruction *IRInstruction);

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
  StringRef DirectiveType;
  clang::SourceLocation Location;
  OMPDirectiveInfo *Parent;
  Instruction *IRInstruction = nullptr;
  SmallVector<OMPDirectiveInfo *, 4> Children;
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

private:
  void handleDirective(clang::OMPExecutableDirective *Directive);
  void storeDirective(OMPDirectiveInfo *Info,
                      const clang::OMPExecutableDirective *Directive);
  void handleClauses(const clang::OMPExecutableDirective *Directive);
  void handleCaptureStmt(clang::OMPParallelDirective *Node,
                         clang::CapturedStmt *CS);
  StringRef getSourceText(clang::SourceRange SR) const;
  void insertAtFileEnd(const std::string &Text);

  /// Attributes
  const clang::SourceManager &SM;
  clang::Rewriter &R;
  clang::FileID MainFileID;

  SmallVector<OMPDirectiveInfo *, 4> DirectiveStack;
  SmallVector<OMPDirectiveInfo *, 4> RootDirectives;
  DenseMap<unsigned, OMPDirectiveInfo *> LineToDirective;
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