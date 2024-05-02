#ifndef LLVM_ARTS_ANALYZER_H
#define LLVM_ARTS_ANALYZER_H

#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Value.h"
#include "llvm/Transforms/IPO/Attributor.h"
#include <cstdint>

using namespace llvm;
namespace arts {
struct ARTSAnalyzer {
  ARTSAnalyzer(AnalysisGetter AG) : AG(AG) {}
  ARTSAnalyzer() {}

  /// Analyzes the Module to identify the OpenMP regions
  uint64_t getNumberofOpenMPRegions(Module &M);

  /// Analyzes the outlined region, replaces the RT call with a call to the
  /// outlined function, which is also modified to remove the arguments that
  /// are not needed.
  bool handleParallelOutlinedRegion(CallBase *RTCall);

  /// Analyzes Task region
  bool handleTaskOutlinedRegion(CallBase *CB);

  /// Analyzes the done region and return next BB to analyze
  BasicBlock *handleDoneRegion(BasicBlock *DoneBB, DominatorTree *DT, 
                               std::string PrefixName, std::string SuffixBB);

  /// This function identifies the EDTs in the function
  bool identifyEDTs(Function &F);

  /// Analyze EDTs
  void analyzeDeps();

  /// Attributes
  AnalysisGetter AG;
};
}

#endif // LLVM_ARTS_ANALYZER_H