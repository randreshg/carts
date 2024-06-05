// Description: Main file for the Compiler for ARTS (OmpTransform) pass.

#ifndef LLVM_API_CARTS_OMPTRANSFORM_H
#define LLVM_API_CARTS_OMPTRANSFORM_H

#include "llvm/IR/PassManager.h"
#include "llvm/Pass.h"

namespace arts {
using namespace llvm;
/// OpenMP optimizations pass.
class OMPTransformPass : public PassInfoMixin<OMPTransformPass> {
public:
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM);
};

} // namespace arts

#endif // LLVM_API_CARTS_OMPTRANSFORM_H