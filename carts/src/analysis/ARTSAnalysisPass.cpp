// Description: Main file for the ARTS Analysis pass.
#include <cassert>
#include <cstdint>

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/ADT/SmallSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/Value.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"

#include "llvm/Analysis/CallGraph.h"
#include "llvm/Analysis/MemorySSA.h"
#include "llvm/Analysis/ValueTracking.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Transforms/IPO/Attributor.h"
#include "llvm/Transforms/Utils/CallGraphUpdater.h"

#include "carts/analysis/graph/EDTEdge.h"
#include "carts/analysis/graph/EDTGraph.h"
#include "carts/codegen/ARTSCodegen.h"
#include "carts/utils/ARTS.h"

using namespace llvm;
using namespace arts;

/// DEBUG
#define DEBUG_TYPE "arts-analysis"
#if !defined(NDEBUG)
static constexpr auto TAG = "[" DEBUG_TYPE "] ";
#endif

/// ------------------------------------------------------------------- ///
///                             CARTS Cache                             ///
/// ------------------------------------------------------------------- ///
struct CARTSInfoCache : public InformationCache {
  CARTSInfoCache(Module &M, AnalysisGetter &AG, BumpPtrAllocator &Allocator,
                 SetVector<Function *> &Functions)
      : InformationCache(M, AG, Allocator, &Functions), M(M),
        Functions(Functions), CG(M) {}

  /// EDTs
  EDT *getEDT(Function *F) {
    auto Itr = FunctionEDTMap.find(F);
    if (Itr != FunctionEDTMap.end())
      return Itr->second;
    return nullptr;
  }

  EDT *getEDT(const CallBase *CB) { return getEDT(CB->getCalledFunction()); }

  EDT *getOrCreateEDT(Function *F) {
    EDT *E = getEDT(F);
    if (E)
      return E;

    if (EDT *CurrentEDT = EDT::get(F)) {
      EDTs.insert(CurrentEDT);
      FunctionEDTMap[F] = CurrentEDT;
      return CurrentEDT;
    }
    return nullptr;
  }

  EDT *getOrCreateEDT(const CallBase *CB) {
    return getOrCreateEDT(CB->getCalledFunction());
  }

  bool isEDTDep(EDT *E, const AA::ValueAndContext &VAC) {
    /// Get the Argument Number
    int32_t ArgNo = getArgNo(VAC);
    assert(ArgNo != -1 && "Argument number not found");
    return E->isDep(ArgNo);
  }

  /// EDTDataBlock
  EDTDataBlock *createDataBlock(AA::ValueAndContext VAC, EDTDataBlock::Mode M) {
    EDTDataBlock *DB = new EDTDataBlock(VAC.getValue(), M);
    /// Insert the DataBlock into the map with its associated Context
    DataBlocks.insert(DB);
    ValueAndCtxToDataBlocks[VAC] = DB;
    return DB;
  }

  int32_t getArgNo(AA::ValueAndContext VAC, int32_t ArgNo = -1) {
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

  EDTDataBlock *getOrCreateDataBlock(AA::ValueAndContext VAC,
                                     int32_t ArgNo = -1) {
    /// Assert that it is an EDTContext
    const CallBase *ContextCB = cast<CallBase>(VAC.getCtxI());
    EDT *EDTCtxt = getOrCreateEDT(ContextCB);
    assert(EDTCtxt && "EDT Context not found");
    /// Get the Argument Number
    ArgNo = getArgNo(VAC, ArgNo);
    assert(ArgNo != -1 && "Argument number not found");
    /// If the value is not a dependency, the DataBlock Mode is ReadOnly
    /// otherwise it is ReadWrite. NOTE: Later on, check the Attributes of the
    /// value to determine the mode.
    bool ValueIsDep = EDTCtxt->isDep(ArgNo);
    EDTDataBlock::Mode DBMode = ValueIsDep ? EDTDataBlock::Mode::ReadWrite
                                           : EDTDataBlock::Mode::ReadOnly;

    /// If the ValueAndCtx are not in the map, create a new DataBlock
    auto Itr = ValueAndCtxToDataBlocks.find(VAC);
    if (Itr == ValueAndCtxToDataBlocks.end())
      return createDataBlock(VAC, DBMode);

    /// If the value is in the map, make sure the DataBlock is the same
    EDTDataBlock *DB = Itr->second;
    if (DB->getMode() == DBMode)
      return DB;
    else
      llvm_unreachable("DataBlock Mode mismatch");
    return DB;
  }

  /// Helper
  void insertGraphNode(EDT *E) { Graph.insertNode(E); }

  /// Getters
  Module &getModule() { return M; }
  SetVector<Function *> &getFunctions() const { return Functions; }
  EDTSet &getEDTs() { return EDTs; }
  EDTGraph &getGraph() { return Graph; }
  ARTSCodegen &getCG() { return CG; }

  /// Helper
  EDTValue *getPointerVal(Value *Val) {
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

  /// Module
  Module &M;
  /// Collection of IPO Functions
  SetVector<Function *> &Functions;
  /// Collection of known EDTs in the module
  EDTSet EDTs;
  DenseMap<EDTFunction *, EDT *> FunctionEDTMap;
  /// Collection of known DataBlocks in the module
  EDTDataBlockSet DataBlocks;
  DenseMap<AA::ValueAndContext, int32_t> ValueAndCtxToCallArgItr;
  DenseMap<AA::ValueAndContext, EDTDataBlock *> ValueAndCtxToDataBlocks;
  /// Analysis Output
  EDTGraph Graph;
  ARTSCodegen CG;
};

/// ------------------------------------------------------------------- ///
///                         AbstractAttributor                          ///
/// ------------------------------------------------------------------- ///
template <typename Ty> struct BooleanStatePtr : public BooleanState {
  void set(Ty *Val) { Ptr = Val; }
  Ty *get() const { return Ptr; }

  bool operator==(const BooleanStatePtr &RHS) const {
    return BooleanState::operator==(RHS) && Ptr == RHS.Ptr;
  }

  bool operator!=(const BooleanStatePtr &RHS) const { return !(*this == RHS); }
  /// "Clamp" this state with \p RHS.
  BooleanStatePtr &operator^=(const BooleanStatePtr &RHS) {
    BooleanState::operator^=(RHS);
    Ptr = RHS.Ptr;
    return *this;
  }

private:
  Ty *Ptr = nullptr;
};

template <typename Ty, bool InsertInvalidates = true>
struct BooleanStateWithSetVector : public BooleanState {
  bool contains(const Ty &Elem) const { return Set.contains(Elem); }
  bool insert(const Ty &Elem) {
    if (InsertInvalidates)
      BooleanState::indicatePessimisticFixpoint();
    return Set.insert(Elem);
  }
  void insert(const SetVector<Ty> &Elems) {
    if (InsertInvalidates)
      BooleanState::indicatePessimisticFixpoint();
    Set.insert(Elems.begin(), Elems.end());
  }

  const Ty &operator[](int Idx) const { return Set[Idx]; }
  bool operator==(const BooleanStateWithSetVector &RHS) const {
    return BooleanState::operator==(RHS) && Set == RHS.Set;
  }
  bool operator!=(const BooleanStateWithSetVector &RHS) const {
    return !(*this == RHS);
  }

  bool empty() const { return Set.empty(); }
  size_t size() const { return Set.size(); }

  /// "Clamp" this state with \p RHS.
  BooleanStateWithSetVector &operator^=(const BooleanStateWithSetVector &RHS) {
    BooleanState::operator^=(RHS);
    Set.insert(RHS.Set.begin(), RHS.Set.end());
    return *this;
  }

private:
  /// A set to keep track of elements.
  SetVector<Ty> Set;

public:
  typename decltype(Set)::iterator begin() { return Set.begin(); }
  typename decltype(Set)::iterator end() { return Set.end(); }
  typename decltype(Set)::const_iterator begin() const { return Set.begin(); }
  typename decltype(Set)::const_iterator end() const { return Set.end(); }
};

template <typename Ty, bool InsertInvalidates = true>
using BooleanStateWithPtrSetVector =
    BooleanStateWithSetVector<Ty *, InsertInvalidates>;

/// ------------------------------------------------------------------- ///
/// - - - - - - - - - - - - - - EDTInfoState - - - - - - - - - - - - -  ///
/// ------------------------------------------------------------------- ///
/// All the information we have about an EDT
struct EDTInfoState : AbstractState {
  /// Pointer to the CARTSInfoCache
  CARTSInfoCache *CARTSCache = nullptr;

  /// EDT Node
  EDT *EDTInfo = nullptr;
  EDT *ParentEDT = nullptr;

  /// For sync EDTs
  EDT *DoneEDT = nullptr;

  /// Flag to track if we reached a fixpoint.
  bool IsAtFixpoint = false;

  /// ParentSyncEDT
  BooleanStatePtr<EDT> ParentSyncEDT;

  /// The EDTs we know are created by the associated EDT.
  BooleanStateWithPtrSetVector<EDT, /* InsertInvalidates */ false> ChildEDTs;

  /// The EDTs we know are reached from the associated EDT.
  BooleanStateWithPtrSetVector<EDT, /* InsertInvalidates */ false>
      DescendantEDTs;

  /// The EDTs we know are dependent from the associated EDT.
  BooleanStateWithPtrSetVector<EDT, /* InsertInvalidates */ false>
      DependentEDTs;
  DenseMap<EDT *, EDTDataBlockSet> DependentEDTDBs;

  /// Insert
  bool insertChildEDT(EDT *ChildEDT) {
    CARTSCache->getGraph().addCreationEdge(EDTInfo, ChildEDT);
    return ChildEDTs.insert(ChildEDT);
  }

  bool insertDependentEDT(EDT *ToEDT, EDTDataBlock *DB) {
    CARTSCache->getGraph().addDataEdge(EDTInfo, ToEDT, DB);
    DependentEDTDBs[ToEDT].insert(DB);
    return DependentEDTs.insert(ToEDT);
  }

  /// Getters
  EDT *getEDT() const { return EDTInfo; }
  EDT *getDoneEDT() const { return DoneEDT; }
  EDT *getParentEDT() const { return ParentEDT; }
  EDT *getParentSyncEDT() const {
    if (!ParentSyncEDT.isValidState())
      return nullptr;
    return ParentSyncEDT.get();
  }

  /// Reachability
  bool canReach(EDT *E) const {
    return DescendantEDTs.contains(E) || ChildEDTs.contains(E);
  }

  bool isReachabilityInfoAtFixpoint() const {
    return DescendantEDTs.isAtFixpoint() && ChildEDTs.isAtFixpoint();
  }

  /// Abstract State interface
  ///{
  EDTInfoState() {}
  EDTInfoState(bool BestState) {
    if (!BestState)
      indicatePessimisticFixpoint();
  }

  ChangeStatus hasChanged(EDTInfoState &StateBefore) {
    return StateBefore == *this ? ChangeStatus::UNCHANGED
                                : ChangeStatus::CHANGED;
  }

  /// See AbstractState::isValidState(...)
  bool isValidState() const override { return true; }

  /// See AbstractState::isAtFixpoint(...)
  bool isAtFixpoint() const override { return IsAtFixpoint; }

  /// See AbstractState::indicatePessimisticFixpoint(...)
  ChangeStatus indicatePessimisticFixpoint() override {
    IsAtFixpoint = true;
    ChildEDTs.indicatePessimisticFixpoint();
    DescendantEDTs.indicatePessimisticFixpoint();
    /// Sync Info
    ParentSyncEDT.indicatePessimisticFixpoint();
    return ChangeStatus::CHANGED;
  }

  /// See AbstractState::indicateOptimisticFixpoint(...)
  ChangeStatus indicateOptimisticFixpoint() override {
    IsAtFixpoint = true;
    ChildEDTs.indicateOptimisticFixpoint();
    DescendantEDTs.indicateOptimisticFixpoint();
    DependentEDTs.indicateOptimisticFixpoint();
    /// Sync Info
    ParentSyncEDT.indicateOptimisticFixpoint();
    return ChangeStatus::UNCHANGED;
  }

  /// Return the assumed state
  EDTInfoState &getAssumed() { return *this; }
  const EDTInfoState &getAssumed() const { return *this; }

  bool operator==(const EDTInfoState &RHS) const {
    if (ChildEDTs != RHS.ChildEDTs)
      return false;
    if (DescendantEDTs != RHS.DescendantEDTs)
      return false;
    if (DependentEDTs != RHS.DependentEDTs)
      return false;
    if (ParentSyncEDT != RHS.ParentSyncEDT)
      return false;
    return true;
  }

  /// Return empty set as the best state of potential values.
  static EDTInfoState getBestState() { return EDTInfoState(true); }

  static EDTInfoState getBestState(EDTInfoState &KIS) { return getBestState(); }

  /// Return full set as the worst state of potential values.
  static EDTInfoState getWorstState() { return EDTInfoState(false); }

  /// "Clamp" this state with \p KIS.
  EDTInfoState operator^=(const EDTInfoState &KIS) {
    ChildEDTs ^= KIS.ChildEDTs;
    DescendantEDTs ^= KIS.DescendantEDTs;
    DependentEDTs ^= KIS.DependentEDTs;
    return *this;
  }

  /// "Clamp" this state with \p KIS, ignoring the ChildEDTs and
  /// DescendantEDTs.
  EDTInfoState operator*=(const EDTInfoState &KIS) {
    DependentEDTs ^= KIS.DependentEDTs;
    return *this;
  }

  EDTInfoState operator&=(const EDTInfoState &KIS) { return (*this ^= KIS); }

  ///}
};

/// EDTInfo
struct AAEDTInfo : public StateWrapper<EDTInfoState, AbstractAttribute> {
  using Base = StateWrapper<EDTInfoState, AbstractAttribute>;
  AAEDTInfo(const IRPosition &IRP, Attributor &A) : Base(IRP) {}

  /// Statistics are tracked as part of manifest for now.
  void trackStatistics() const override {}

  /// See AbstractAttribute::getAsStr()
  const std::string getAsStr(Attributor *) const override {
    if (!isValidState() || !EDTInfo)
      return "<invalid>";

    auto ParentSyncEDTStr = [&]() {
      if (!ParentSyncEDT.isValidState())
        return string("     -ParentSyncEDT invalid\n");
      if (!ParentSyncEDT.get())
        return string("     -ParentSyncEDT: <null>\n");
      EDT *EDTPtr = ParentSyncEDT.get();
      return (string("     -ParentSyncEDT: EDT #") +
              std::to_string(EDTPtr->getID()) + "\n");
    };

    auto DescendantEDTstr = [&]() {
      if (!DescendantEDTs.isValidState())
        return "     -Reached DescendantEDTs invalid with " +
               std::to_string(DescendantEDTs.size()) + " EDTs\n";
      std::string Str;
      for (EDT *E : DescendantEDTs)
        Str += (Str.empty() ? "" : ", ") + std::to_string(E->getID());
      Str = "     #Reached DescendantEDTs: " +
            std::to_string(DescendantEDTs.size()) + "{" + Str + "}\n";
      return Str;
    };

    auto ChildEDTStr = [&]() {
      if (!ChildEDTs.isValidState())
        return "     -Child EDTs invalid with " +
               std::to_string(ChildEDTs.size()) + " EDTs\n";
      std::string Str;
      for (EDT *E : ChildEDTs)
        Str += (Str.empty() ? "" : ", ") + std::to_string(E->getID());
      Str = "     #Child EDTs: " + std::to_string(ChildEDTs.size()) + "{" +
            Str + "}\n";
      return Str;
    };

    auto DepEDTStr = [&]() {
      if (!DependentEDTs.isValidState())
        return "     -Dependent EDTs invalid with " +
               std::to_string(DependentEDTs.size()) + " EDTs";
      std::string Str;
      for (EDT *E : DependentEDTs)
        Str += (Str.empty() ? "" : ", ") + std::to_string(E->getID());
      Str = "     #Dependent EDTs: " + std::to_string(DependentEDTs.size()) +
            "{" + Str + "}";
      return Str;
    };

    std::string EDTStr =
        "\nState for EDT #" + std::to_string(EDTInfo->getID()) + " -> \n";
    if (ParentEDT)
      EDTStr +=
          "     -ParentEDT: EDT #" + std::to_string(ParentEDT->getID()) + "\n";
    return (EDTStr + ParentSyncEDTStr() + ChildEDTStr() + DescendantEDTstr() +
            DepEDTStr());
  }
  /// Create an abstract attribute biew for the position \p IRP.
  static AAEDTInfo &createForPosition(const IRPosition &IRP, Attributor &A);

  /// See AbstractAttribute::getName()
  const std::string getName() const override { return "AAEDTInfo"; }

  /// See AbstractAttribute::getIdAddr()
  const char *getIdAddr() const override { return &ID; }

  /// This function should return true if the type of the \p AA is AAEDTInfo
  static bool classof(const AbstractAttribute *AA) {
    return (AA->getIdAddr() == &ID);
  }

  static const char ID;
};

/// ------------------------------------------------------------------- ///
/// - - - - - - - - - - -  EDTDataBlockInfoState  - - - - - - - - - - - ///
/// ------------------------------------------------------------------- ///
/// All the information we have about an EDT data block
struct AAEDTDataBlockInfo;

struct EDTDataBlockInfoState : AbstractState {
  /// Pointer to the CARTSInfoCache
  CARTSInfoCache *CARTSCache = nullptr;
  /// Flag to track if we reached a fixpoint.
  bool IsAtFixpoint = false;
  /// DataBlock
  EDTDataBlock *DB = nullptr;
  /// The EDT that creates the data block
  EDT *ParentEDT = nullptr;
  /// The EDT that is associated with the context of the data block
  EDT *CtxEDT = nullptr;

  /// MayDependEDTs
  // BooleanStateWithPtrSetVector<EDT, false> MayDependEDTs;
  /// SignalEDTs
  BooleanStatePtr<EDT> SignalEDT;
  BooleanStatePtr<EDT> DependentDoneEDT;

  /// Set of ChildrenEDTs that signal to their associated EDTDone
  BooleanStateWithPtrSetVector<EDT, false> SignaledChildEDTs;

  /// What we know about the DependentSiblingEDTs
  BooleanStateWithPtrSetVector<EDT, false> DependentSiblingEDTs;
  SetVector<AA::ValueAndContext> DependentSiblingValAndCtx;

  /// What we know about the DependentChildEDTs
  BooleanStateWithPtrSetVector<EDT, false> DependentChildEDTs;
  SetVector<AA::ValueAndContext> DependentChildValAndCtx;

  /// Set of EDTs MayBeMoved to
  // BooleanStateWithPtrSetVector<EDT, false> MayBeMovedToEDTs;
  // DenseMap<EDT *, EDTSet> MayBeMovedToEDTsMap;
  /// Map of DataMovements
  DenseMap<EDT *, EDTSet> DataMovements;

  /// Size

  /// Usability

  /// Location

  /// Getters
  EDTDataBlock *getDataBlock() const { return DB; }
  Value *getValue() const { return DB->getValue(); }
  EDT *getParentEDT() const { return ParentEDT; }
  EDT *getCtxEDT() const { return CtxEDT; }

  /// Data Movements
  void insertDataMovement(EDT *From, EDT *To) {
    DataMovements[From].insert(To);
  }

  EDTSet getDataMovements(EDT *From) const {
    auto Itr = DataMovements.find(From);
    if (Itr == DataMovements.end())
      return EDTSet();
    return Itr->second;
  }

  bool isMoved(EDT *From, EDT *To) const {
    auto Itr = DataMovements.find(From);
    if (Itr == DataMovements.end())
      return false;
    return Itr->second.count(To);
  }

  /// Abstract State interface
  ///{
  EDTDataBlockInfoState() {}
  EDTDataBlockInfoState(bool BestState) {
    if (!BestState)
      indicatePessimisticFixpoint();
  }

  ChangeStatus hasChanged(EDTDataBlockInfoState &StateBefore) {
    return StateBefore == *this ? ChangeStatus::UNCHANGED
                                : ChangeStatus::CHANGED;
  }

  /// See AbstractState::isValidState(...)
  bool isValidState() const override { return true; }

  /// See AbstractState::isAtFixpoint(...)
  bool isAtFixpoint() const override { return IsAtFixpoint; }

  /// See AbstractState::indicatePessimisticFixpoint(...)
  ChangeStatus indicatePessimisticFixpoint() override {
    IsAtFixpoint = true;
    SignalEDT.indicatePessimisticFixpoint();
    DependentDoneEDT.indicatePessimisticFixpoint();
    SignaledChildEDTs.indicatePessimisticFixpoint();
    DependentSiblingEDTs.indicatePessimisticFixpoint();
    DependentChildEDTs.indicatePessimisticFixpoint();
    return ChangeStatus::CHANGED;
  }

  /// See AbstractState::indicateOptimisticFixpoint(...)
  ChangeStatus indicateOptimisticFixpoint() override {
    IsAtFixpoint = true;
    SignalEDT.indicateOptimisticFixpoint();
    DependentDoneEDT.indicateOptimisticFixpoint();
    SignaledChildEDTs.indicateOptimisticFixpoint();
    DependentSiblingEDTs.indicateOptimisticFixpoint();
    DependentChildEDTs.indicateOptimisticFixpoint();
    return ChangeStatus::UNCHANGED;
  }

  /// Return the assumed state
  EDTDataBlockInfoState &getAssumed() { return *this; }
  const EDTDataBlockInfoState &getAssumed() const { return *this; }

  bool operator==(const EDTDataBlockInfoState &RHS) const {
    if (DependentChildEDTs != RHS.DependentChildEDTs)
      return false;
    if (DependentSiblingEDTs != RHS.DependentSiblingEDTs)
      return false;
    return true;
  }

  /// Return empty set as the best state of potential values.
  static EDTDataBlockInfoState getBestState() {
    return EDTDataBlockInfoState(true);
  }

  static EDTDataBlockInfoState getBestState(EDTDataBlockInfoState &KIS) {
    return getBestState();
  }

  /// Return full set as the worst state of potential values.
  static EDTDataBlockInfoState getWorstState() {
    return EDTDataBlockInfoState(false);
  }

  /// "Clamp" this state with \p KIS.
  EDTDataBlockInfoState operator^=(const EDTDataBlockInfoState &KIS) {
    DependentChildEDTs ^= KIS.DependentChildEDTs;
    DependentSiblingEDTs ^= KIS.DependentSiblingEDTs;
    return *this;
  }

  EDTDataBlockInfoState operator&=(const EDTDataBlockInfoState &KIS) {
    return (*this ^= KIS);
  }

  ///}
};

/// EDTDataBlockInfo
struct AAEDTDataBlockInfo
    : public StateWrapper<EDTDataBlockInfoState, AbstractAttribute> {
  using Base = StateWrapper<EDTDataBlockInfoState, AbstractAttribute>;
  AAEDTDataBlockInfo(const IRPosition &IRP, Attributor &A) : Base(IRP) {}

  /// Statistics are tracked as part of manifest for now.
  void trackStatistics() const override {}

  /// See AbstractAttribute::getAsStr()
  const std::string getAsStr(Attributor *) const override {
    if (!isValidState())
      return "<invalid>\n";

    auto DependentChildEDTsStr = [&]() {
      if (!DependentChildEDTs.isValidState())
        return "     - DependentChildEDTs invalid with " +
               std::to_string(DependentChildEDTs.size()) + " EDTs\n";
      std::string Str;
      for (EDT *E : DependentChildEDTs)
        Str += (Str.empty() ? "" : ", ") + std::to_string(E->getID());
      Str = "     - #DependentChildEDTs: " +
            std::to_string(DependentChildEDTs.size()) + "{" + Str + "}\n";
      return Str;
    };

    auto DependentSiblingEDTsStr = [&]() {
      if (!DependentSiblingEDTs.isValidState())
        return "     - DependentSiblingEDTs invalid with " +
               std::to_string(DependentSiblingEDTs.size()) + " EDTs\n";
      std::string Str;
      for (EDT *E : DependentSiblingEDTs)
        Str += (Str.empty() ? "" : ", ") + std::to_string(E->getID());
      Str = "     - #DependentSiblingEDTs: " +
            std::to_string(DependentSiblingEDTs.size()) + "{" + Str + "}\n";
      return Str;
    };

    std::string DBStr = "EDTDataBlock ->\n";
    if (ParentEDT)
      DBStr +=
          "     - ParentEDT: EDT #" + std::to_string(ParentEDT->getID()) + "\n";
    return (DBStr + DependentChildEDTsStr() + DependentSiblingEDTsStr());
  }

  /// Create an abstract attribute biew for the position \p IRP.
  static AAEDTDataBlockInfo &createForPosition(const IRPosition &IRP,
                                               Attributor &A);

  /// See AbstractAttribute::getName()
  const std::string getName() const override { return "AAEDTDataBlockInfo"; }

  /// See AbstractAttribute::getIdAddr()
  const char *getIdAddr() const override { return &ID; }

  /// This function should return true if the type of the \p AA is
  /// AAEDTDataBlockInfo
  static bool classof(const AbstractAttribute *AA) {
    return (AA->getIdAddr() == &ID);
  }

  static const char ID;
};

/// ------------------------------------------------------------------- ///
/// - - - - - - - - - - - - - - - AAEDTInfo - - - - - - - - - - - - - - ///
/// ------------------------------------------------------------------- ///
/// The EDTFunction info abstract attribute, basically, what can we say
/// about a function with regards to the EDTInfoState.
struct AAEDTInfoFunction : AAEDTInfo {
  AAEDTInfoFunction(const IRPosition &IRP, Attributor &A) : AAEDTInfo(IRP, A) {}

  /// See AbstractAttribute::initialize(...).
  void initialize(Attributor &A) override {
    CARTSCache = &static_cast<CARTSInfoCache &>(A.getInfoCache());
    EDTFunction *Fn = getAnchorScope();
    EDTInfo = CARTSCache->getOrCreateEDT(Fn);
    if (!EDTInfo) {
      indicatePessimisticFixpoint();
      return;
    }
    LLVM_DEBUG(dbgs() << "\n[AAEDTInfoFunction::initialize] EDT #"
                      << EDTInfo->getID() << " for function \"" << Fn->getName()
                      << "\"\n");
    CARTSCache->insertGraphNode(EDTInfo);

    /// Callback to check all the calls to the EDT Function.
    uint32_t EDTCallCounter = 0;
    auto CheckCallSite = [&](AbstractCallSite ACS) {
      EDTCallCounter++;
      /// Verify it is a call to the EDT Function
      EDTCall = cast<CallBase>(ACS.getInstruction());
      auto *CalledEDT = CARTSCache->getOrCreateEDT(EDTCall);
      assert(CalledEDT == EDTInfo &&
             "EDTCall doesn't correspond to the EDTInfo of the function!");
      assert(EDTCallCounter == 1 &&
             "Multiple calls to EDTFunction not supported yet.");

      /// Set EDTCall
      EDTInfo->setCall(EDTCall);

      /// Set ParentEDT - Is the EDT called from another EDT?
      ParentEDT = CARTSCache->getOrCreateEDT(EDTCall->getCaller());
      assert(ParentEDT && "EDT is not called from another EDT");
      LLVM_DEBUG(dbgs() << "   - ParentEDT: EDT #" << ParentEDT->getID()
                        << "\n");
      EDTInfo->setParent(ParentEDT);

      /// Check if it is a sync EDT
      if (EDTInfo->isAsync())
        return true;

      /// SyncEDTs must have a DoneEDT.
      /// It is usually in the first instruction of the next BB.
      /// Start by looking at the next instruction after the EDTCall
      auto *NextBB = EDTCall->getParent()->getNextNode();
      auto *CB = dyn_cast<CallBase>(&NextBB->front());
      assert(CB && "Next instruction is not a CallBase!");
      DoneEDT = CARTSCache->getOrCreateEDT(CB);
      assert(
          DoneEDT &&
          "DoneEDT is null! It should have been found in the next BasicBlock!");

      LLVM_DEBUG(dbgs() << "   - DoneEDT: EDT #" << DoneEDT->getID() << "\n");

      /// Also, Sync EDTs do not have a ParentSyncEDT
      ParentSyncEDT.indicateOptimisticFixpoint();
      return true;
    };

    bool AllCallSitesKnown = true;
    if (!A.checkForAllCallSites(CheckCallSite, *this,
                                true /* RequireAllCallSites */,
                                AllCallSitesKnown)) {
      LLVM_DEBUG(dbgs() << "   - Failed to visit all Callsites!\n");
      if (EDTInfo->getTy() != EDTType::Main) {
        LLVM_DEBUG(dbgs() << "   - EDT #" << EDTInfo->getID()
                          << " is dead. It can be safely removed\n");
        indicatePessimisticFixpoint();
        return;
      }
    }
  }

  /// Update the ChildEDTs and DescendantEDTs of the EDT. It returns true
  /// if the ChildEDTs and DescendantEDTs is at a fixpoint
  bool updateChildEDTs(Attributor &A) {
    /// No need to update if we are already at a fixpoint
    if (isReachabilityInfoAtFixpoint())
      return true;

    /// Flags to determine fixpoint
    bool AllReachedEDTsWereFixed = true;
    /// Check calls to EDTChilds
    auto CheckCallInst = [&](Instruction &I) {
      CallBase &EDTCall = cast<CallBase>(I);
      EDT *CalledEDT = CARTSCache->getEDT(&EDTCall);
      if (!CalledEDT)
        return true;

      LLVM_DEBUG(dbgs() << "   - EDT #" << CalledEDT->getID()
                        << " is a child of EDT #" << EDTInfo->getID() << "\n");
      insertChildEDT(CalledEDT);

      /// Run AAEDTInfo on the ChildEDT
      auto *ChildEDTAA = A.getAAFor<AAEDTInfo>(
          *this, IRPosition::function(*CalledEDT->getFn()),
          DepClassTy::OPTIONAL);
      assert(ChildEDTAA && "ChildEDTAA is null!");
      /// Clamp set of DescendantEDTs of the ChildEDT
      DescendantEDTs ^= ChildEDTAA->DescendantEDTs;
      DescendantEDTs ^= ChildEDTAA->ChildEDTs;
      /// Fixpoint check
      AllReachedEDTsWereFixed &= ChildEDTAA->isReachabilityInfoAtFixpoint();
      return true;
    };

    bool UsedAssumedInformationInCheckCallInst = false;
    if (!A.checkForAllCallLikeInstructions(
            CheckCallInst, *this, UsedAssumedInformationInCheckCallInst)) {
      LLVM_DEBUG(dbgs() << TAG
                        << "Failed to visit all call-like instructions!\n";);
      indicatePessimisticFixpoint();
      return false;
    }

    /// If we got to this point, we know that all children were created
    ChildEDTs.indicateOptimisticFixpoint();
    /// Indicate Optimistic Fixpoint if all the DescendantEDTs are fixed
    if (!AllReachedEDTsWereFixed)
      return false;
    LLVM_DEBUG(dbgs() << "   - All ReachedEDTs are fixed for EDT #"
                      << EDTInfo->getID() << "\n");
    DescendantEDTs.indicateOptimisticFixpoint();
    return true;
  }

  /// Recursively check the ancestors of the ParentSyncEDTInfo to find the
  /// ParentSyncEDT
  EDT *getParentSyncEDTInfo(EDT *ParentSyncEDTInfo, uint32_t Depth = 0) {
    /// The ParentSyncEDT had to eventually created me. Analyze the
    /// ParentSyncEDTInfo and its ancestors to find the ParentSyncEDT

    /// Check if the ParentSyncEDTInfo is a SyncEDT
    SyncEDT *SyncEDTInfo = dyn_cast<SyncEDT>(ParentSyncEDTInfo);

    /// If it is an async EDT, recursively check the ancestors
    if (!SyncEDTInfo)
      return getParentSyncEDTInfo(ParentSyncEDTInfo->getParent(), Depth + 1);

    /// If it is, check if it syncs me
    if (SyncEDTInfo->mustSyncChilds() && Depth == 0)
      return ParentSyncEDTInfo;

    /// If it does not, and depth is greater than 0, check the ancestors
    if (SyncEDTInfo->mustSyncDescendants() && Depth > 0)
      return ParentSyncEDTInfo;

    /// If it does not, and depth is 0, it means that the ParentSyncEDT is not
    return nullptr;
  }

  /// Fixpoint iteration update function. Will be called every time a
  /// dependence changed its state (and in the beginning).
  ChangeStatus updateImpl(Attributor &A) override {
    LLVM_DEBUG(dbgs() << "\n[AAEDTInfoFunction::updateImpl] EDT #"
                      << EDTInfo->getID() << "\n";);
    EDTInfoState StateBefore = getState();

    /// Wait until the reachability info is at a fixpoint
    if (!updateChildEDTs(A))
      return hasChanged(StateBefore);

    /// Set the ParentSyncEDT if it is not set
    if (!ParentSyncEDT.isAtFixpoint()) {
      if (EDT *ParentSyncEDTInfo = getParentSyncEDTInfo(ParentEDT)) {
        LLVM_DEBUG(dbgs() << "   - ParentSyncEDT: EDT #"
                          << ParentSyncEDTInfo->getID() << "\n");
        ParentSyncEDT.set(ParentSyncEDTInfo);
        ParentSyncEDT.indicateOptimisticFixpoint();
        EDTInfo->setParentSync(ParentSyncEDT.get());
      } else {
        LLVM_DEBUG(dbgs() << "   - ParentSyncEDT could not be found\n");
      }
    }

    /// If not EDTCall, return optimistic fixpoint, for now.
    if (!EDTCall)
      return indicateOptimisticFixpoint();

    /// Run AAEDTInfo on the EDTCall
    auto *EDTCallAA = A.getOrCreateAAFor<AAEDTInfo>(
        IRPosition::callsite_function(*EDTCall), this, DepClassTy::OPTIONAL);
    getState() *= EDTCallAA->getState();
    /// Continue if MaySignalEDTs is at a fixpoint

    /// For now, Indicate optimistic fixpoint
    indicateOptimisticFixpoint();

    /// Return the change status
    return hasChanged(StateBefore);
  }

  /// Modify the IR based on the EDTInfoState as the fixpoint iteration is
  /// finished now.
  ChangeStatus manifest(Attributor &A) override {
    /// Debug output info
    LLVM_DEBUG(dbgs() << "-------------------------------\n"
                      << "[AAEDTInfoFunction::manifest] " << getAsStr(&A)
                      << "\n\n");
    if (!EDTInfo)
      return ChangeStatus::UNCHANGED;
    LLVM_DEBUG(dbgs() << *EDTInfo);

    /// Reserve EDTGuid
    // ARTSCodegen &CG = CARTSCache->getCG();
    // CG.getOrCreateEDTFunction(*EDTInfo);
    // CG.getOrCreateEDTGuid(*EDTInfo);
    return ChangeStatus::UNCHANGED;
  }

private:
  EDTCallBase *EDTCall = nullptr;
};

/// The EDTCallsite info abstract attribute, basically, what can we now about an
/// EDT from its corresponding callsite.
struct AAEDTInfoCallsite : AAEDTInfo {
  AAEDTInfoCallsite(const IRPosition &IRP, Attributor &A) : AAEDTInfo(IRP, A) {}

  /// See AbstractAttribute::initialize(...).
  void initialize(Attributor &A) override {
    CARTSCache = &static_cast<CARTSInfoCache &>(A.getInfoCache());
    /// Get the Called EDT
    CallBase &EDTCall = cast<CallBase>(getAnchorValue());
    EDTInfo = CARTSCache->getEDT(EDTCall.getCalledFunction());
    assert(EDTInfo && "EDTInfo is null!");
    LLVM_DEBUG(dbgs() << "\n[AAEDTInfoCallsite::initialize] EDT #"
                      << EDTInfo->getID() << "\n");
  }

  /// See AbstractAttribute::updateImpl(Attributor &A).
  ChangeStatus updateImpl(Attributor &A) override {
    LLVM_DEBUG(dbgs() << "\n[AAEDTInfoCallsite::updateImpl] EDT #"
                      << EDTInfo->getID() << "\n");

    CallBase &EDTCall = cast<CallBase>(getAnchorValue());
    /// Run AAEDTDataBlockInfo on each argument of the call instruction
    bool AllDBsWereFixed = true;
    for (uint32_t CallArgItr = 0; CallArgItr < EDTCall.data_operands_size();
         ++CallArgItr) {
      auto *ArgEDTDataBlockAA = A.getAAFor<AAEDTInfo>(
          *this, IRPosition::callsite_argument(EDTCall, CallArgItr),
          DepClassTy::OPTIONAL);
      if (!ArgEDTDataBlockAA->isAtFixpoint()) {
        AllDBsWereFixed = false;
        continue;
      }
      /// Here we clamp the set of and DependentEDTs
      DependentEDTs ^= ArgEDTDataBlockAA->DependentEDTs;
    }

    if (!AllDBsWereFixed)
      return ChangeStatus::UNCHANGED;

    LLVM_DEBUG(dbgs() << "   - All DataBlocks were fixed for EDT #"
                      << EDTInfo->getID() << "\n");
    indicateOptimisticFixpoint();
    return ChangeStatus::CHANGED;
  }

  /// See AbstractAttribute::manifest(Attributor &A).
  ChangeStatus manifest(Attributor &A) override {
    return ChangeStatus::UNCHANGED;
  }
};

struct AAEDTInfoCallsiteArg : AAEDTInfo {
  AAEDTInfoCallsiteArg(const IRPosition &IRP, Attributor &A)
      : AAEDTInfo(IRP, A) {}

  /// See AbstractAttribute::initialize(...).
  void initialize(Attributor &A) override {
    CARTSCache = &static_cast<CARTSInfoCache &>(A.getInfoCache());

    /// Get the called EDT
    CallBase &EDTCall = cast<CallBase>(getAnchorValue());
    EDTInfo = CARTSCache->getEDT(EDTCall.getCalledFunction());
    assert(EDTInfo && "CalledEDT is null!");

    auto CallArgItr = getCallSiteArgNo();
    LLVM_DEBUG(dbgs() << "\n[AAEDTInfoCallsiteArg::initialize] CallArg #"
                      << CallArgItr << " from EDT #" << EDTInfo->getID()
                      << "\n");

    /// Get the value associated with the argument. In case of load/store, get
    /// the pointer operand.
    ArgVal = &getAssociatedValue();
    assert(ArgVal && "ArgVal is null!");

    /// Create DataBlockInfo for the argument
    AA::ValueAndContext VAC(*ArgVal, EDTCall);
    CARTSCache->getOrCreateDataBlock(VAC, CallArgItr);

    /// If the value is a not a dependency of the EDT, indicate optimistic
    /// fixpoint
    if (!EDTInfo->isDep(CallArgItr))
      indicateOptimisticFixpoint();
  }

  /// See AbstractAttribute::updateImpl(Attributor &A).
  ChangeStatus updateImpl(Attributor &A) override {
    EDT *CalledEDT = getEDT();
    LLVM_DEBUG(dbgs() << "\n[AAEDTInfoCallsiteArg::updateImpl] "
                      << getAssociatedValue() << " from EDT #"
                      << CalledEDT->getID() << "\n");
    /// Run AAEDTDataBlockInfo on the associated value
    CallBase &EDTCall = cast<CallBase>(getAnchorValue());
    auto *ArgValDBInfoAA = A.getAAFor<AAEDTDataBlockInfo>(
        *this, IRPosition::callsite_argument(EDTCall, getCallSiteArgNo()),
        DepClassTy::OPTIONAL);
    if (!ArgValDBInfoAA || !ArgValDBInfoAA->getState().isValidState()) {
      LLVM_DEBUG(dbgs() << "     - Failed to get AAEDTDataBlockInfo for value: "
                        << *ArgVal << "\n");
      return indicatePessimisticFixpoint();
    }

    /// Wait until fixpoint is reached
    if (!ArgValDBInfoAA->isAtFixpoint())
      return ChangeStatus::UNCHANGED;

    /// Add Dependency if any
    EDT *SignalEDT = ArgValDBInfoAA->SignalEDT.get();
    if (SignalEDT) {
      LLVM_DEBUG(dbgs() << "     - EDT #" << CalledEDT->getID()
                        << " signals to EDT #" << SignalEDT->getID() << "\n");
      insertDataEdge(CalledEDT, SignalEDT, ArgValDBInfoAA->getDataBlock());
      DependentEDTs.insert(SignalEDT);
    }

    indicateOptimisticFixpoint();
    return ChangeStatus::CHANGED;
  }

  void invalidatePreviouslyInsertedEdges(EDTDataBlock *DB) {
    for (EDTGraphEdge *Edge : InsertedEdges) {
      EDTGraphDataEdge *DataEdge = cast<EDTGraphDataEdge>(Edge);
      DataEdge->removeDataBlock(DB);
    }
    InsertedEdges.clear();
  }

  void insertDataEdge(EDT *From, EDT *To, EDTDataBlock *DB) {
    auto *Edge = CARTSCache->getGraph().addDataEdge(From, To, DB);
    InsertedEdges.insert(Edge);
  }

  /// See AbstractAttribute::manifest(Attributor &A).
  ChangeStatus manifest(Attributor &A) override {
    return ChangeStatus::UNCHANGED;
  }

private:
  /// Value to analyze
  Value *ArgVal = nullptr;
  SmallSet<EDTGraphEdge *, 4> InsertedEdges;
};

/// ------------------------------------------------------------------- ///
/// - - - - - - - - - - - - -  AAEDTDataBlockInfo - - - - - - - - - - - ///
/// ------------------------------------------------------------------- ///
/// The EDTDataBlock info abstract attribute, basically, what can we say
/// about a DataBlock with regards to the EDTDataBlockInfoState.
struct AAEDTDataBlockInfoCtxAndVal : AAEDTDataBlockInfo {
  AAEDTDataBlockInfoCtxAndVal(const IRPosition &IRP, Attributor &A)
      : AAEDTDataBlockInfo(IRP, A) {}

  /// See AbstractAttribute::initialize(...).
  void initialize(Attributor &A) override {
    /// Set EDT DataBlock info
    CARTSCache = &static_cast<CARTSInfoCache &>(A.getInfoCache());

    /// Create DataBlockInfo for the argument
    CallBase &EDTCall = cast<CallBase>(getAnchorValue());
    Value *ArgVal = &getAssociatedValue();
    AA::ValueAndContext VAC(*ArgVal, EDTCall);
    DB = CARTSCache->getOrCreateDataBlock(VAC, getCallSiteArgNo());

    /// Set ParentEDT and CtxEDT
    ParentEDT = CARTSCache->getEDT(EDTCall.getCaller());
    CtxEDT = CARTSCache->getEDT(&EDTCall);

    LLVM_DEBUG(
        dbgs() << "\n[AAEDTDataBlockInfoCtxAndVal::initialize] "
               << *DB->getValue() << " from EDT #"
               << CARTSCache->getEDT(EDTCall.getCalledFunction())->getID()
               << "\n");
  }

  /// For Sync EDTs, analyze the dependencies with the siblings
  ChangeStatus handleSyncEDTs(Attributor &A) {
    /// If we are at a fixpoint, we are done here
    if (DependentDoneEDT.isAtFixpoint())
      return ChangeStatus::UNCHANGED;

    /// Otherwise, we have to check if any of the DependentSiblingEDTs
    if (!setDependentSiblingEDTs(A))
      return ChangeStatus::UNCHANGED;

    /// There must be only one sibling EDT
    assert(DependentSiblingEDTs.size() == 1 &&
           "Sync EDTs must have only one DoneEDT!");

    /// Verify that the DoneEDT is the same as the sibling EDT
    assert((CtxEDT->getDoneSync() == DependentSiblingEDTs[0]) &&
           "DoneEDT is not the same as the sibling EDT!");

    /// Set the dependency with the DoneEDT
    DependentDoneEDT.set(DependentSiblingEDTs[0]);
    DependentDoneEDT.indicateOptimisticFixpoint();
    return ChangeStatus::CHANGED;
  }

  /// For Async EDTs, analyze the dependencies with respect to the
  /// sync parent context
  ChangeStatus handleAsyncEDTs(Attributor &A) {
    /// Compute only if the DependentDoneEDTs is not at a fixpoint
    if (DependentDoneEDT.isAtFixpoint())
      return ChangeStatus::UNCHANGED;

    /// If CtxEDT has no children, it must signal the value to the DoneEDT
    auto *CtxEDTAA = A.lookupAAFor<AAEDTInfo>(
        IRPosition::function(*CtxEDT->getFn()), this, DepClassTy::NONE);
    if (CtxEDTAA->ChildEDTs.empty()) {
      EDT *EDTDoneSync = CtxEDT->getDoneSync();
      SignalEDT.set(EDTDoneSync);
      // LLVM_DEBUG(dbgs() << "     - EDT #" << CtxEDT->getID()
      //                   << " must signal the value to EDT #"
      //                   << CtxEDT->getDoneSync()->getID() << "\n");
      indicateOptimisticFixpoint();
      return ChangeStatus::CHANGED;
    }

    /// If we reach this point, we have to check if any of the ChildEDTs
    /// signals the DB. We do this later
    return ChangeStatus::UNCHANGED;
  }

  /// Check if the value is signaled by a ChildEDT
  ChangeStatus handleChildEDTs(Attributor &A) {
    /// Check if we haven't computed the information yet
    if (DependentChildEDTs.isAtFixpoint())
      return ChangeStatus::UNCHANGED;

    if (!setDependentChildEDTs(A))
      return ChangeStatus::UNCHANGED;

    /// If no child EDTs depend on the value, we are done here
    EDT *EDTDoneSync = CtxEDT->getDoneSync();
    if (DependentChildEDTs.size() == 0) {
      SignalEDT.set(EDTDoneSync);
      // LLVM_DEBUG(dbgs() << "     - EDT #" << CtxEDT->getID()
      //                   << " must signal the value to EDT #"
      //                   << EDTDoneSync->getID() << "\n");
      return indicateOptimisticFixpoint();
    }

    /// Otherwise, we have to check if any of the DependentChildEDTs
    /// signals the DB
    bool AllChildEDTsWereFixed = true;
    bool MustSignal = true;
    for (AA::ValueAndContext ChildVAC : DependentChildValAndCtx) {
      const CallBase &ChildCall = *cast<CallBase>(ChildVAC.getCtxI());
      auto ChildCallArg = CARTSCache->getArgNo(ChildVAC);
      auto *ChildEDTAA = A.getAAFor<AAEDTDataBlockInfo>(
          *this, IRPosition::callsite_argument(ChildCall, ChildCallArg),
          DepClassTy::OPTIONAL);
      assert(ChildEDTAA && "ChildEDTAA is null!");

      if (!ChildEDTAA->isAtFixpoint()) {
        AllChildEDTsWereFixed = false;
        continue;
      }

      /// If the value is signaled by any ChildEDT, or any of its descendants,
      /// we do not have to signal it. Clamp the set of SignaledChildEDTs
      if (ChildEDTAA->SignalEDT.get() == EDTDoneSync) {
        SignaledChildEDTs.insert(ChildEDTAA->getCtxEDT());
        MustSignal = false;
      } else if (ChildEDTAA->SignaledChildEDTs.size() > 0) {
        SignaledChildEDTs ^= ChildEDTAA->SignaledChildEDTs;
        /// Check if any of the SignaledChildEDTs signals the value to my
        /// EDTDoneSync
        for (EDT *SignaledChildEDT : ChildEDTAA->SignaledChildEDTs) {
          if (SignaledChildEDT != EDTDoneSync)
            continue;
          MustSignal = false;
        }
      }
    }

    if (!AllChildEDTsWereFixed)
      return ChangeStatus::UNCHANGED;

    if (MustSignal)
      SignalEDT.set(CtxEDT->getDoneSync());

    indicateOptimisticFixpoint();
    return ChangeStatus::CHANGED;
  }

  /// Util functions
  bool runAAPointerInfoOnValue(
      Attributor &A, Value &Val,
      BooleanStateWithPtrSetVector<EDT, false> &DependentEDTs,
      SetVector<AA::ValueAndContext> &DependentValAndCtx,
      bool CheckReachability = false) {
    /// Run AAPointerInfo on the ArgCall
    const AAPointerInfo *PI = A.getAAFor<AAPointerInfo>(
        *this, IRPosition::value(Val), DepClassTy::OPTIONAL);
    if (!PI || !PI->getState().isValidState()) {
      LLVM_DEBUG(dbgs() << "   - AAPointerInfo Failed or is invalid!\n");
      llvm_unreachable("AAPointerInfo is not valid!");
      indicatePessimisticFixpoint();
      return false;
    }
    if (!PI->getState().isAtFixpoint()) {
      LLVM_DEBUG(dbgs() << "   - AAPointerInfo is not at fixpoint!\n");
      return false;
    }

    /// Analyze offset bin and collect all calls that use the value
    SetVector<AA::ValueAndContext> PointerInfoValAndCtx;
    for (auto PIItr = PI->begin(); PIItr != PI->end(); ++PIItr) {
      const auto &It = PI->begin();
      const SmallSet<unsigned, 4> &AccessIndex = It->second;
      /// Analyze all accesses
      for (auto AI : AccessIndex) {
        const AAPointerInfo::Access &Acc = PI->getAccess(AI);
        Instruction *LocalInst = Acc.getLocalInst();
        /// If it is a load instruction, check call instruction uses
        if (LoadInst *LI = dyn_cast<LoadInst>(LocalInst)) {
          for (auto *U : LI->users()) {
            Instruction *UI = cast<Instruction>(U);
            if (CallBase *CBTo = dyn_cast<CallBase>(UI)) {
              PointerInfoValAndCtx.insert(AA::ValueAndContext(*LI, CBTo));
            }
          }
        }

        /// If call instruction add the ValueAndContext to the set
        if (CallBase *CBTo = dyn_cast<CallBase>(LocalInst))
          PointerInfoValAndCtx.insert(AA::ValueAndContext(Val, CBTo));
      }
    }

    /// Analyze all ValueAndContexts
    CallBase &EDTCall = cast<CallBase>(getAnchorValue());
    for (const AA::ValueAndContext &VAC : PointerInfoValAndCtx) {
      auto *CBTo = cast<CallBase>(VAC.getCtxI());
      EDT *ToEDT = CARTSCache->getEDT(CBTo->getCalledFunction());
      if (!ToEDT || (CBTo == &EDTCall))
        continue;

      /// If the CBTo is not reachable from the EDTCall, ignore it.
      /// e.g. EDTCall is after CBTo
      if (CheckReachability &&
          !AA::isPotentiallyReachable(A, EDTCall, *CBTo, *this))
        continue;

      /// If the value is not a dependency, continue
      /// An error will be thrown if the value is not used in CBTo
      if (!CARTSCache->isEDTDep(ToEDT, VAC))
        continue;

      /// Add dependency to set of DependentSiblingEDTs
      DependentEDTs.insert(ToEDT);
      DependentValAndCtx.insert(VAC);
    }
    return true;
  }

  bool setDependentSiblingEDTs(Attributor &A) {
    /// If the DependentSiblingEDTs is at a fixpoint, we are done here
    if (DependentSiblingEDTs.isAtFixpoint())
      return true;

    /// Run AAPointerInfo on the ArgCall
    Value &EDTVal = getAssociatedValue();
    LLVM_DEBUG(dbgs() << "   - Analyzing DependentSiblingEDTs on " << EDTVal
                      << "\n");
    EDTValue &PointerValue = *CARTSCache->getPointerVal(&EDTVal);
    if (!runAAPointerInfoOnValue(A, PointerValue, DependentSiblingEDTs,
                                 DependentSiblingValAndCtx, true))
      return false;

    /// If we got to this point, we know all the DependentSiblingEDTs
    DependentSiblingEDTs.indicateOptimisticFixpoint();
    LLVM_DEBUG(dbgs() << "        - Number of DependentSiblingEDTs: "
                      << DependentSiblingEDTs.size() << "\n");
    return true;
  }

  bool setDependentChildEDTs(Attributor &A) {
    /// If the DependentChildEDTs is at a fixpoint, we are done here
    if (DependentChildEDTs.isAtFixpoint())
      return true;

    /// Run AAPointerInfo on the EDTFn Argument
    Value &EDTVal = getAssociatedValue();
    CallBase &EDTCall = cast<CallBase>(getAnchorValue());
    AA::ValueAndContext VAC = AA::ValueAndContext(EDTVal, EDTCall);
    Value &ArgVal =
        *EDTCall.getCalledFunction()->getArg(CARTSCache->getArgNo(VAC));

    LLVM_DEBUG(dbgs() << "   - Analyzing DependentChildEDTs on " << EDTVal
                      << " (" << ArgVal << ")\n");
    // / Run
    if (!runAAPointerInfoOnValue(A, ArgVal, DependentChildEDTs,
                                 DependentChildValAndCtx, false))
      return false;

    /// If we got to this point, we know all the DependentChildEDTs
    DependentChildEDTs.indicateOptimisticFixpoint();
    LLVM_DEBUG(dbgs() << "        - Number of DependentChildEDTs: "
                      << DependentChildEDTs.size() << "\n");
    return true;
  }

  /// See AbstractAttribute::updateImpl(Attributor &A).
  ChangeStatus updateImpl(Attributor &A) override {
    LLVM_DEBUG(
        CallBase &EDTCall = cast<CallBase>(getAnchorValue());
        dbgs() << "\n[AAEDTDataBlockInfoCtxAndVal::updateImpl] "
               << getAssociatedValue() << " from EDT #"
               << CARTSCache->getEDT(EDTCall.getCalledFunction())->getID()
               << "\n");

    ChangeStatus Changed = ChangeStatus::UNCHANGED;
    if (CtxEDT->isAsync())
      Changed |= handleAsyncEDTs(A);
    else
      Changed |= handleSyncEDTs(A);
    Changed |= handleChildEDTs(A);
    return Changed;
  }

  /// See AbstractAttribute::manifest(Attributor &A).
  ChangeStatus manifest(Attributor &A) override {
    LLVM_DEBUG(dbgs() << "-------------------------------\n");
    LLVM_DEBUG(
        CallBase &EDTCall = cast<CallBase>(getAnchorValue());
        dbgs() << "[AAEDTDataBlockInfoCtxAndVal::manifest] " << *DB->getValue()
               << " from EDT #"
               << CARTSCache->getEDT(EDTCall.getCalledFunction())->getID()
               << "\n"
               << getAsStr(&A) << "\n");
    return ChangeStatus::UNCHANGED;
  }
};

/// ------------------------------------------------------------------- ///
/// - - - - - - - - - - - -  AACreateForPosition  - - - - - - - - - - - ///
/// ------------------------------------------------------------------- ///
/// Create for position
const char AAEDTInfo::ID = 0;
const char AAEDTDataBlockInfo::ID = 0;

AAEDTInfo &AAEDTInfo::createForPosition(const IRPosition &IRP, Attributor &A) {
  AAEDTInfo *AA = nullptr;
  switch (IRP.getPositionKind()) {
  case IRPosition::IRP_INVALID:
  case IRPosition::IRP_ARGUMENT:
  case IRPosition::IRP_RETURNED:
  case IRPosition::IRP_CALL_SITE_RETURNED:
  case IRPosition::IRP_FLOAT:
    llvm_unreachable("EDTInfo can only be created for this IR position!");
  case IRPosition::IRP_CALL_SITE_ARGUMENT:
    AA = new (A.Allocator) AAEDTInfoCallsiteArg(IRP, A);
    break;
  case IRPosition::IRP_CALL_SITE:
    AA = new (A.Allocator) AAEDTInfoCallsite(IRP, A);
    break;
  case IRPosition::IRP_FUNCTION:
    AA = new (A.Allocator) AAEDTInfoFunction(IRP, A);
    break;
  }

  return *AA;
}

AAEDTDataBlockInfo &AAEDTDataBlockInfo::createForPosition(const IRPosition &IRP,
                                                          Attributor &A) {
  AAEDTDataBlockInfo *AA = nullptr;
  switch (IRP.getPositionKind()) {
  case IRPosition::IRP_INVALID:
  case IRPosition::IRP_RETURNED:
  case IRPosition::IRP_CALL_SITE_RETURNED:
  case IRPosition::IRP_CALL_SITE:
  case IRPosition::IRP_FUNCTION:
  case IRPosition::IRP_ARGUMENT:
  case IRPosition::IRP_FLOAT:
    llvm_unreachable(
        "EDTDataBlockInfo can only be created for floating position!");
  case IRPosition::IRP_CALL_SITE_ARGUMENT:
    AA = new (A.Allocator) AAEDTDataBlockInfoCtxAndVal(IRP, A);
    break;
  }

  return *AA;
}

/// ------------------------------------------------------------------- ///
///                        ARTSAnalysisPass                             ///
/// ------------------------------------------------------------------- ///
namespace {
class ARTSAnalysisPass : public PassInfoMixin<ARTSAnalysisPass> {
public:
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM) {
    LLVM_DEBUG(dbgs() << "\n"
                      << "-------------------------------------------------\n");
    LLVM_DEBUG(dbgs() << TAG << "Running ARTS Analysis pass on Module: \n"
                      << M.getName() << "\n");
    LLVM_DEBUG(dbgs() << "-------------------------------------------------\n");
    /// EDT Cache
    LLVM_DEBUG(dbgs() << TAG << "Creating and Initializing EDTs: \n");
    SetVector<Function *> Functions;
    for (Function &Fn : M) {
      if (Fn.isDeclaration() && !Fn.hasLocalLinkage())
        continue;
      Functions.insert(&Fn);
    }

    FunctionAnalysisManager &FAM =
        AM.getResult<FunctionAnalysisManagerModuleProxy>(M).getManager();
    AnalysisGetter AG(FAM);
    BumpPtrAllocator Allocator;
    CARTSInfoCache InfoCache(M, AG, Allocator, Functions);

    computeGraph(M, InfoCache);
    generateCode(M, InfoCache);
    LLVM_DEBUG(dbgs() << "\n"
                      << "-------------------------------------------------\n");
    LLVM_DEBUG(dbgs() << TAG << "Process has finished\n");
    LLVM_DEBUG(dbgs() << "\n" << M << "\n");
    LLVM_DEBUG(dbgs() << "\n"
                      << "-------------------------------------------------\n");
    return PreservedAnalyses::all();
  }

  void computeGraph(Module &M, CARTSInfoCache &InfoCache) {
    LLVM_DEBUG(dbgs() << "\n\n[Attributor] Initializing AAEDTInfo: \n");
    /// Create attributor
    auto &Functions = InfoCache.getFunctions();
    CallGraphUpdater CGUpdater;
    AttributorConfig AC(CGUpdater);
    AC.DefaultInitializeLiveInternals = false;
    AC.IsModulePass = true;
    AC.RewriteSignatures = false;
    AC.MaxFixpointIterations = 32;
    AC.DeleteFns = false;
    AC.PassName = DEBUG_TYPE;
    Attributor A(Functions, InfoCache, AC);

    /// Register AAs
    for (Function *Fn : Functions) {
      A.getOrCreateAAFor<AAEDTInfo>(IRPosition::function(*Fn),
                                    /* QueryingAA */ nullptr, DepClassTy::NONE,
                                    /* ForceUpdate */ false,
                                    /* UpdateAfterInit */ false);
    }
    ChangeStatus Changed = A.run();
    LLVM_DEBUG(dbgs() << "[Attributor] Done with " << Functions.size()
                      << " functions, result: " << Changed << ".\n");
    InfoCache.getGraph().print();
  }

  void generateCode(Module &M, CARTSInfoCache &InfoCache) {
    ARTSCodegen &CG = InfoCache.getCG();
    EDTGraph &Graph = InfoCache.getGraph();
    auto EDTNodes = Graph.getNodes();
    /// Reserve GUIDs for all EDTs
    // LLVM_DEBUG(dbgs() << "\nAll EDT Guids have been reserved\n");
    // for (EDTGraphNode *EDTNode : EDTNodes) {
    //   EDT &CurrentEDT = *EDTNode->getEDT();
    //   /// For now ignore sync EDTs
    //   if (!CurrentEDT.isAsync())
    //     continue;
    //   /// Modify data environments based on dependencies.
    //   /// EDTs can only signal to DoneEDTs. Check if any output data edges
    //   auto OutEdges = Graph.getOutgoingEdges(EDTNode);
    //   if (OutEdges.size() == 0) {
    //     continue;
    //   } else {
    //     /// Print the outgoing edges.
    //     EDT *ParentSyncEDT = CurrentEDT.getParentSync();
    //     for (EDTGraphEdge *DepEdge : OutEdges) {
    //       EDTGraphNode *ToNode = DepEdge->getTo();
    //       EDT *ToEDT = ToNode->getEDT();
    //       /// For now ignore not Data edges
    //       if (!DepEdge->isDataEdge())
    //         continue;

    //       /// Make sure the EDT knows where to signal the value.
    //       LLVM_DEBUG(dbgs() << "    - EDT #" << ParentSyncEDT->getID()
    //                         << " must signal the DoneSyncEDTGuid to EDT #"
    //                         << CurrentEDT.getID() << "\n");
    //       Value *DoneSyncGuid = CurrentEDT.getDoneSync()->getGuidAddress();
    //       assert(DoneSyncGuid && "DoneSyncGuid is null!");
    //       EDTDataBlock *DoneSyncGuidDB = InfoCache.getOrCreateDataBlock(
    //           DoneSyncGuid, EDTDataBlock::Mode::ReadOnly, ParentSyncEDT);
    //       CurrentEDT.insertValueToEnv(DoneSyncGuid, true);
    //       Graph.addDataEdge(ParentSyncEDT, &CurrentEDT, DoneSyncGuidDB);
    //       /// Insert signal to the EDT in the exit block
    //     }
    //   }
    //   // LLVM_DEBUG(dbgs() << "\n");
    // }

    // LLVM_DEBUG(dbgs() << "    - [");
    // if (DepEdge->isDataEdge()) {
    //   LLVM_DEBUG(dbgs() << "data");
    // } else if (DepEdge->isControlEdge()) {
    //   LLVM_DEBUG(dbgs() << "control");
    // }
    // if (DepEdge->hasCreationDep()) {
    //   LLVM_DEBUG(dbgs() << "/ creation");
    // }
    // LLVM_DEBUG(dbgs() << "] \"EDT #" << ToE->getID() << "\"\n");
    // if (DepEdge->isDataEdge()) {
    //   auto *DataEdge = cast<EDTGraphDataEdge>(DepEdge);
    //   auto Values = DataEdge->getValues();
    //   for (auto *V : Values) {
    //     LLVM_DEBUG(dbgs() << "        - " << *V << "\n");
    //   }
    // }
    //   /// Analyze output edges. If any data dependency, check if I need to
    //   /// signal a GUID address or not.
    //   //
    //   // CG.insertEDTEntry(CurrentEDT);

    //   // return;
    //   // switch (CurrentEDT.getTy()) {
    //   // case EDTType::Task:
    //   //   CG.generateTaskEDT(CurrentEDT);
    //   //   break;
    //   // case EDTType::Sync:
    //   //   CG.generateSyncEDT(CurrentEDT);
    //   //   break;
    //   // case EDTType::Parallel:
    //   //   CG.generateParallelEDT(CurrentEDT);
    //   //   break;
    //   // default:
    //   //   llvm_unreachable("EDT Type not supported!");
    //   // }

    //   // LLVM_DEBUG(dbgs() << "- EDT #" << E->getID() << " - \"" <<
    //   E->getName()
    //   //                   << "\"\n");
    //   // LLVM_DEBUG(dbgs() << "  - Type: " << toString(E->getTy()) << "\n");
    //   // /// Data environment
    //   // LLVM_DEBUG(dbgs() << "  - Data Environment:\n");
    //   // auto &DE = E->getDataEnv();
    //   // LLVM_DEBUG(dbgs() << "    - " << "Number of ParamV = " <<
    //   // DE.getParamC()
    //   //                   << "\n");
    //   // for (auto &P : DE.ParamV) {
    //   //   LLVM_DEBUG(dbgs() << "      - " << *P << "\n");
    //   // }
    //   // LLVM_DEBUG(dbgs() << "    - " << "Number of DepV = " << DE.getDepC()
    //   //                   << "\n");
    //   // for (auto &D : DE.DepV) {
    //   //   LLVM_DEBUG(dbgs() << "      - " << *D << "\n");
    //   // }
    //   /// Dependencies
    //   // LLVM_DEBUG(dbgs() << "  - Incoming Edges:\n");
    //   // auto InEdges = CARTSGraph.getIncomingEdges(EDTNode);
    //   // if (InEdges.size() == 0) {
    //   //   LLVM_DEBUG(dbgs() << "    - The EDT has no incoming edges\n");
    //   // } else {
    //   //   /// Print the incoming edges.
    //   //   for (auto *DepEdge : InEdges) {
    //   //     auto *From = DepEdge->getFrom();
    //   //     auto *FromE = From->getEDT();
    //   //     // LLVM_DEBUG(dbgs() << "    - [");
    //   //     // if (DepEdge->isDataEdge()) {
    //   //     //   LLVM_DEBUG(dbgs() << "data");
    //   //     // } else if (DepEdge->isControlEdge()) {
    //   //     //   LLVM_DEBUG(dbgs() << "control");
    //   //     // }
    //   //     // if (DepEdge->hasCreationDep()) {
    //   //     //   LLVM_DEBUG(dbgs() << "/ creation");
    //   //     // }
    //   //     // LLVM_DEBUG(dbgs() << "] \"EDT #" << FromE->getID() <<
    //   "\"\n");
    //   //   }
    //   // }
    // }
  }
};

} // namespace

// This part is the new way of registering your pass
extern "C" ::llvm::PassPluginLibraryInfo LLVM_ATTRIBUTE_WEAK
llvmGetPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "ARTSAnalysisPass", "v0.1",
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, ModulePassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "arts-analysis") {
                    FPM.addPass(ARTSAnalysisPass());
                    return true;
                  }
                  return false;
                });
          }};
}