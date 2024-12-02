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
using namespace clang;
using namespace std;



/// Base class for all OpenMP directives
class OMPDirectiveInfo {
public:
  OMPDirectiveInfo(StringRef Type, SourceLocation Loc);
  virtual ~OMPDirectiveInfo() = default;

  StringRef getType() const;
  string getLocation(const SourceManager &SM) const;

  virtual void printInfo(raw_ostream &OS) const {
    OS << "Directive: " << DirectiveType << "\n";
  };

protected:
  StringRef DirectiveType;
  SourceLocation Location;
};

/// Parallel directive info
class OMPParallelInfo : public OMPDirectiveInfo {
public:
  OMPParallelInfo(SourceLocation Loc);
  unsigned getNumThreads() const;

  void printInfo(raw_ostream &OS) const override {
    OMPDirectiveInfo::printInfo(OS);
    OS << "  Number of Threads: " << NumThreads << "\n";
  }

private:
  /// Number of threads for the parallel region
  unsigned NumThreads;
};

/// Task directive info
class OMPTaskInfo : public OMPDirectiveInfo {
public:
  OMPTaskInfo(SourceLocation Loc);

  bool hasDependencies() const;

  void printInfo(raw_ostream &OS) const override {
    OMPDirectiveInfo::printInfo(OS);
    OS << "  Dependencies: " << (Dependencies ? "Yes" : "No") << "\n";
  }

private:
  bool Dependencies;
  SetVector<Instruction *> Inputs;
  SetVector<Instruction *> Outputs;
};

class OpenMPASTConsumer;
/// OMPVisitor class to traverse the AST and find OpenMP directives
class OMPVisitor {
public:
  OMPVisitor() = default;
  ~OMPVisitor();

  void run(llvm::Module &M, SetVector<Function *> &Functions,
           string InputASTFilename) ;

  const OMPDirectiveInfo *getDirectiveForInstruction(Instruction *I) const;

private:
  OpenMPASTConsumer *Consumer;
};

} // namespace arts
#endif // LLVM_API_CARTS_OMPVISITOR_H