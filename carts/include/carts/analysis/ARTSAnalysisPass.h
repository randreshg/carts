#ifndef LLVM_API_CARTS_ARTSANALYSISPASS_H
#define LLVM_API_CARTS_ARTSANALYSISPASS_H

// Description: Header file for the ARTS Analysis pass.
#include "llvm/Pass.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Analysis/MemorySSA.h"

using namespace llvm;
namespace arts {
/// ------------------------------------------------------------------- ///
///                        ARTSAnalysisPass                             ///
/// ------------------------------------------------------------------- ///
class ARTSAnalysisPass : public PassInfoMixin<ARTSAnalysisPass> {
public:
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM);
};

// struct ARTSAnalysisPass : public ModulePass {
//   static char ID;

//   ARTSAnalysisPass();
//   bool doInitialization(Module &M) override;
//   bool runOnModule(Module &M) override;
//   void getAnalysisUsage(AnalysisUsage &AU) const override;

  
//   MemorySSA &getMemorySSA(Function &F);
  
// };

} // namespace
#endif // LLVM_API_CARTS_ARTSANALYSISPASS_H