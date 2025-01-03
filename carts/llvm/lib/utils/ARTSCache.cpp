#include "arts/utils/ARTSCache.h"
#include "llvm/Support/Debug.h"

/// ------------------------------------------------------------------- ///
///                              ARTS Cache                             ///
/// ------------------------------------------------------------------- ///
namespace arts {

/// DEBUG
#define DEBUG_TYPE "arts-analysis"
#if !defined(NDEBUG)
static constexpr auto TAG = "[" DEBUG_TYPE "] ";
#endif

ARTSCache::ARTSCache(Module &M, AnalysisGetter &AG, BumpPtrAllocator &Allocator,
                     SetVector<Function *> &Functions)
    : InformationCache(M, AG, Allocator, &Functions), M(M),
      Functions(Functions) {
  LLVM_DEBUG(dbgs() << "- - - - - - - - - - - - - - - - - - - - - - - - \n");
  LLVM_DEBUG(dbgs() << TAG << "Initializing ARTS Cache\n");
  Graph = new ARTSGraph();
  CG = new ARTSCodegen(this);
  handleGlobalAnnotations();

  /// Analyzing the functions
  LLVM_DEBUG(dbgs() << " - Analyzing Functions\n");
  for (Function *Fn : Functions) {
    LLVM_DEBUG(dbgs() << "-- Function: " << Fn->getName() << "\n");
    getOrCreateEDT(Fn);
    LLVM_DEBUG(dbgs() << "\n");
  }
}

ARTSCache::~ARTSCache() {
  delete Graph;
  delete CG;
}

/// EDTs
EDT *ARTSCache::getEDT(Function *F) {
  auto Itr = FnEDTMap.find(F);
  if (Itr != FnEDTMap.end())
    return Itr->second;
  return nullptr;
}

EDT *ARTSCache::getEDT(const CallBase *CB) {
  return getEDT(CB->getCalledFunction());
}

EDT *ARTSCache::getOrCreateEDT(Function *F) {
  EDT *E = getEDT(F);
  if (E)
    return E;

  /// If the EDT does not exist, check if it has
  /// an EDT Annotation
  if (FnAnnotationMap.find(F) == FnAnnotationMap.end())
    return nullptr;

  /// If the EDT does not exist, create a new one
  string Annotation = FnAnnotationMap[F];
  if (EDT *CurrentEDT = EDT::get(F, Annotation)) {
    EDTs.insert(CurrentEDT);
    FnEDTMap[F] = CurrentEDT;
    return CurrentEDT;
  }
  return nullptr;
}

EDT *ARTSCache::getOrCreateEDT(const CallBase *CB) {
  return getOrCreateEDT(CB->getCalledFunction());
}

/// DataBlock
DataBlock *ARTSCache::createDataBlock(AA::ValueAndContext VAC,
                                      DataBlock::Mode M, EDT *ContextEDT) {
  DataBlock *DB = new DataBlock(VAC.getValue(), M, ContextEDT);
  /// Insert the DataBlock into the map with its associated Context
  DataBlocks.insert(DB);
  ValueAndCtxToDataBlocks[VAC] = DB;
  return DB;
}

DataBlock *ARTSCache::getOrCreateDataBlock(AA::ValueAndContext VAC,
                                           int32_t ArgNo) {
  /// Assert that it is an EDTContext
  const CallBase *ContextEDTCB = cast<CallBase>(VAC.getCtxI());
  EDT *ContextEDT = getOrCreateEDT(ContextEDTCB);
  assert(ContextEDT && "EDT Context not found");
  /// Get the Argument Number
  ArgNo = getArgNo(VAC, ArgNo);
  assert(ArgNo != -1 && "Argument number not found");
  /// If the value is not a dependency, the DataBlock Mode is ReadOnly
  /// otherwise it is ReadWrite. NOTE: Later on, check the Attributes of the
  /// value to determine the mode.
  bool ValueIsDep = ContextEDT->isDep(ArgNo);
  DataBlock::Mode DBMode =
      ValueIsDep ? DataBlock::Mode::ReadWrite : DataBlock::Mode::ReadOnly;

  /// If the ValueAndCtx are not in the map, create a new DataBlock
  auto Itr = ValueAndCtxToDataBlocks.find(VAC);
  if (Itr == ValueAndCtxToDataBlocks.end())
    return createDataBlock(VAC, DBMode, ContextEDT);

  /// If the value is in the map, make sure the DataBlock is the same
  DataBlock *DB = Itr->second;
  if (DB->getMode() == DBMode)
    return DB;
  else
    llvm_unreachable("DataBlock Mode mismatch");
  return DB;
}

/// Helper Functions
Argument *ARTSCache::getEDTArg(EDT *E, Value *EDTCallVal) {
  AA::ValueAndContext VAC(*EDTCallVal, E->getCall());
  int32_t ArgNo = getArgNo(VAC);
  assert(ArgNo != -1 && "Argument number not found");
  return E->getFn()->getArg(ArgNo);
}

bool ARTSCache::isEDTDep(const AA::ValueAndContext &VAC) {
  int32_t ArgNo = getArgNo(VAC);
  assert(ArgNo != -1 && "Argument number not found");
  EDT *E = getEDT(cast<CallBase>(VAC.getCtxI()));
  assert(E && "EDT not found");
  return E->isDep(ArgNo);
}

int32_t ARTSCache::getArgNo(AA::ValueAndContext VAC, int32_t ArgNo) {
  /// If value is different from -1, learn it and return it
  if (ArgNo != -1) {
    ValueAndCtxToCallArgItr[VAC] = ArgNo;
    return ArgNo;
  }
  /// Try to find it in the map first
  auto Itr = ValueAndCtxToCallArgItr.find(VAC);
  if (Itr != ValueAndCtxToCallArgItr.end())
    return Itr->second;
  /// Find the argument number
  Value *V = VAC.getValue();
  const CallBase *ContextCB = cast<CallBase>(VAC.getCtxI());
  for (uint32_t i = 0; i < ContextCB->data_operands_size(); i++) {
    if (ContextCB->getArgOperand(i) == V) {
      ValueAndCtxToCallArgItr[VAC] = i;
      return i;
    }
  }
  return -1;
}

EDTValue *ARTSCache::getPointerVal(Value *Val) {
  Value *PointerVal = nullptr;
  if (auto *I = dyn_cast<Instruction>(Val)) {
    if (auto *LI = dyn_cast<LoadInst>(I))
      PointerVal = LI->getPointerOperand();
    else if (auto *SI = dyn_cast<StoreInst>(I))
      PointerVal = SI->getPointerOperand();
  }
  /// If the value is not an load/store instruction, use the value itself
  if (!PointerVal)
    PointerVal = Val;
  return PointerVal;
}

void ARTSCache::insertGraphNode(EDT *E) { Graph->getOrCreateNode(E); }

/// Getters
Module &ARTSCache::getModule() { return M; }
SetVector<Function *> &ARTSCache::getFunctions() const { return Functions; }
EDTSet &ARTSCache::getEDTs() { return EDTs; }
ARTSGraph &ARTSCache::getGraph() { return *Graph; }
ARTSCodegen &ARTSCache::getCG() { return *CG; }

/// Global Annotations
void ARTSCache::handleGlobalAnnotations() {
  GlobalVariable *GA = M.getGlobalVariable("llvm.global.annotations");
  if (!GA)
    return;

  ConstantArray *CA = dyn_cast<ConstantArray>(GA->getOperand(0));
  if (!CA)
    return;

  for (auto &Op : CA->operands()) {
    ConstantStruct *CS = dyn_cast<ConstantStruct>(Op);
    if (!CS)
      continue;

    /// Argument 0 is the function
    Function *Fn = dyn_cast<Function>(CS->getOperand(0));
    if (!Fn)
      continue;
    LLVM_DEBUG(dbgs() << TAG << "Function: " << Fn->getName() << "\n");

    /// Argument 1 is the annotation of type private unnamed_addr constant
    GlobalVariable *GV = dyn_cast<GlobalVariable>(CS->getOperand(1));
    if (!GV || !GV->hasInitializer())
      continue;

    ConstantDataArray *CDA = dyn_cast<ConstantDataArray>(GV->getInitializer());
    if (!CDA)
      continue;

    StringRef Annotation = CDA->getAsCString();
    LLVM_DEBUG(dbgs() << TAG << "Annotation: " << Annotation << "\n");

    /// Store in map
    FnAnnotationMap[Fn] = Annotation;

    
  }
}

} // namespace arts
