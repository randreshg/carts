#include "llvm/IR/Function.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

#include "noelle/core/Noelle.hpp"

using namespace arcana::noelle;

namespace {

struct CARTS : public ModulePass {
  static char ID;

  CARTS() : ModulePass(ID) {}

  bool doInitialization(Module &M) override { return false; }

  bool runOnModule(Module &M) override {

    /// Fetch NOELLE
    auto &Noelle = getAnalysis<Noelle>();
    /// Use NOELLE
    auto Insts = Noelle.numberOfProgramInstructions();
    errs() << "The program has " << Insts << " instructions\n";

    return false;
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<Noelle>();
  }
};

} // namespace

// Next there is code to register your pass to "opt"
char CARTS::ID = 0;
static RegisterPass<CARTS> X("CARTS", "Compiler for ARTS");

// Next there is code to register your pass to "clang"
static CARTS *_PassMaker = NULL;
static RegisterStandardPasses _RegPass1(PassManagerBuilder::EP_OptimizerLast,
                                        [](const PassManagerBuilder &,
                                           legacy::PassManagerBase &PM) {
                                          if (!_PassMaker) {
                                            PM.add(_PassMaker = new CARTS());
                                          }
                                        }); // ** for -Ox
static RegisterStandardPasses
    _RegPass2(PassManagerBuilder::EP_EnabledOnOptLevel0,
              [](const PassManagerBuilder &, legacy::PassManagerBase &PM) {
                if (!_PassMaker) {
                  PM.add(_PassMaker = new CARTS());
                }
              }); // ** for -O0
