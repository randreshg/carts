//===- ARTSCache.h - --------------------------------------------*- C++ -*-===//
//===----------------------------------------------------------------------===//
#ifndef LLVM_ARTSCACHE_H
#define LLVM_ARTSCACHE_H

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SetVector.h"

#include "carts/analysis/graph/ARTSGraph.h"
#include "carts/codegen/ARTSCodegen.h"
#include "carts/utils/ARTS.h"
#include "llvm/Transforms/IPO/Attributor.h"

/// ------------------------------------------------------------------- ///
///                              ARTS Cache                             ///
/// ------------------------------------------------------------------- ///
namespace arts {
using namespace llvm;

struct ARTSCache : public InformationCache {
public:
  ARTSCache(Module &M, AnalysisGetter &AG, BumpPtrAllocator &Allocator,
            SetVector<Function *> &Functions);
  ~ARTSCache();

  /// EDTs
  EDT *getEDT(Function *F);
  EDT *getEDT(const CallBase *CB);
  EDT *getOrCreateEDT(Function *F);
  EDT *getOrCreateEDT(const CallBase *CB);

  /// DataBlock
  /// Given a ValueAndContext (EDTCall and EDTValue), create a DataBlock
  DataBlock *createDataBlock(AA::ValueAndContext VAC, DataBlock::Mode M,
                                EDT *ContextEDT);
  DataBlock *getOrCreateDataBlock(AA::ValueAndContext VAC,
                                     int32_t ArgNo = -1);

  /// Helper Functions
  /// Given an EDT, and a value (used in the EDT call), returns the EDT Function
  /// Argument
  Argument *getEDTArg(EDT *E, Value *EDTCallVal);
  /// Given an EDTCall and a EDTVal returns if the value is a dependency
  bool isEDTDep(const AA::ValueAndContext &VAC);
  /// Given a ValueAndContext, find the argument number of the value in the
  /// context of the callbase.
  int32_t getArgNo(AA::ValueAndContext VAC, int32_t ArgNo = -1);
  /// For a given value, returns the pointer
  EDTValue *getPointerVal(Value *Val);
  /// Insert a node in the Graph
  void insertGraphNode(EDT *E);

  /// Getters
  Module &getModule();
  SetVector<Function *> &getFunctions() const;
  EDTSet &getEDTs();
  ARTSGraph &getGraph();
  ARTSCodegen &getCG();

private:
  /// Module
  Module &M;
  /// Collection of IPO Functions
  SetVector<Function *> &Functions;
  /// Collection of known EDTs in the module
  EDTSet EDTs;
  DenseMap<EDTFunction *, EDT *> FunctionEDTMap;
  /// Collection of known DataBlocks in the module
  DataBlockSet DataBlocks;
  DenseMap<AA::ValueAndContext, int32_t> ValueAndCtxToCallArgItr;
  DenseMap<AA::ValueAndContext, DataBlock *> ValueAndCtxToDataBlocks;
  /// Analysis Output
  ARTSGraph *Graph = nullptr;
  ARTSCodegen *CG = nullptr;
};
} // namespace arts

#endif // LLVM_ARTSCACHE_H