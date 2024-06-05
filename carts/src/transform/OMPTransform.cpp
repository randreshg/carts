// Description: Main file for the Compiler for ARTS (OmpTransform) pass.

#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Value.h"

#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"

#include "llvm/Support/raw_ostream.h"

#include "llvm/Support/Debug.h"

#include "carts/transform/OMPTransform.h"
#include "noelle/core/Noelle.hpp"
#include "carts/analysis/ARTS.h"

// #include "ARTSAnalyzer.h"
// #include "ARTSIRBuilder.h"
// #include "EDTGraph.h"

using namespace llvm;
using namespace arts;
using namespace arcana::noelle;

/// DEBUG
#define DEBUG_TYPE "OMPTransform"
#if !defined(NDEBUG)
static constexpr auto TAG = "[" DEBUG_TYPE "] ";
#endif

PreservedAnalyses OMPTransformPass::run(Module &M, ModuleAnalysisManager &AM) {
  LLVM_DEBUG(dbgs() << "\n-------------------------------------------------\n");
  LLVM_DEBUG(dbgs() << TAG << "Running OmpTransform on Module: \n"
                    << M.getName() << "\n");
  LLVM_DEBUG(dbgs() << "\n-------------------------------------------------\n");
  /// Fetch NOELLE Manager
  // auto &NM = AM.getResult<Noelle>(M);
  // EDTCache Cache(M, NM);
  // EDTGraph EDTG(Cache);
  // EDTG.print();

  /// Print module info
  LLVM_DEBUG(dbgs() << "\n-------------------------------------------------\n");
  LLVM_DEBUG(dbgs() << TAG << "Process has finished\n\n" << M << "\n");
  LLVM_DEBUG(dbgs() << "\n-------------------------------------------------\n");
  return PreservedAnalyses::all();
}

// This part is the new way of registering your pass
extern "C" ::llvm::PassPluginLibraryInfo LLVM_ATTRIBUTE_WEAK
llvmGetPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "OMPTransform", "v0.1", [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, ModulePassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == DEBUG_TYPE) {
                    FPM.addPass(OMPTransformPass());
                    return true;
                  }
                  return false;
                });
          }};
}