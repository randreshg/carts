#ifndef LLVM_API_CARTS_ARTSANALYSISPASS_H
#define LLVM_API_CARTS_ARTSANALYSISPASS_H

// Description: Header file for the ARTS Analysis pass.
#include "llvm/Pass.h"
#include "llvm/IR/LegacyPassManager.h"
#include "noelle/core/Noelle.hpp"
#include "llvm/Analysis/MemorySSA.h"

using namespace llvm;
namespace arts {
using namespace arcana::noelle;
/// ------------------------------------------------------------------- ///
///                        ARTSAnalysisPass                             ///
/// ------------------------------------------------------------------- ///
struct ARTSAnalysisPass : public ModulePass {
  static char ID;

  ARTSAnalysisPass();
  bool doInitialization(Module &M) override;
  bool runOnModule(Module &M) override;
  void getAnalysisUsage(AnalysisUsage &AU) const override;

  Noelle &getNoelle();
  MemorySSA &getMemorySSA(Function &F);
  
};

} // namespace
#endif // LLVM_API_CARTS_ARTSANALYSISPASS_H