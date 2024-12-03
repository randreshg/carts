/// Description: AST Visitor to traverse the AST and find OpenMP directives

#ifndef LLVM_API_CARTS_OMPVISITOR_H
#define LLVM_API_CARTS_OMPVISITOR_H
#include "llvm/IR/Instruction.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Support/raw_ostream.h"

#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/DiagnosticIDs.h"
#include "clang/Frontend/CompilerInstance.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"

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
    if(IRInstruction)
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
  static OMPDirectiveInfo *handleDirective(clang::OMPExecutableDirective *Directive);

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
  static OMPDirectiveInfo *handleDirective(clang::OMPExecutableDirective *Directive);

  bool hasDependencies() const;

  void printInfo(raw_ostream &OS, const clang::SourceManager &SM,
                 unsigned Depth = 0) const override {
    OMPDirectiveInfo::printInfo(OS, SM, Depth);
    OS.indent(Depth * 2 + 2)
        << "Dependencies: " << (Dependencies ? "Yes" : "No") << "\n";
  }

private:
  bool Dependencies = false;
  SetVector<Instruction *> Inputs;
  SetVector<Instruction *> Outputs;
};

class OpenMPASTConsumer;
/// OMPVisitor class to traverse the AST and find OpenMP directives
class OMPVisitor {
public:
  OMPVisitor(llvm::Module &M, SetVector<Function *> &Functions);
  ~OMPVisitor();

  void run(string InputASTFilename);

  const OMPDirectiveInfo *getDirectiveForInstruction(Instruction *I) const;

private:
  llvm::Module &M;
  SetVector<Function *> &Functions;
  OpenMPASTConsumer *Consumer;
};

} // namespace arts
#endif // LLVM_API_CARTS_OMPVISITOR_H