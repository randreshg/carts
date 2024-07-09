// Description: Main file for the ARTS Analysis pass.
#include <cassert>
#include <cstdint>

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/Value.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"

#include "llvm/Analysis/CallGraph.h"
#include "llvm/Analysis/MemorySSA.h"
#include "llvm/Analysis/ValueTracking.h"
#include "llvm/Support/Debug.h"
#include "llvm/Transforms/IPO/Attributor.h"
#include "llvm/Transforms/Utils/CallGraphUpdater.h"

// #include "carts/analysis/ARTSAnalysisPass.h"
#include "carts/analysis/graph/EDTGraph.h"
#include "carts/utils/ARTS.h"

using namespace llvm;
using namespace arts;

/// DEBUG
#define DEBUG_TYPE "arts-analysis"
#if !defined(NDEBUG)
static constexpr auto TAG = "[" DEBUG_TYPE "] ";
#endif

/// ------------------------------------------------------------------- ///
///                           EDTAnalyzer                               ///
/// ------------------------------------------------------------------- ///
/// Attribute state
template <typename Ty, bool InsertInvalidates = true>
struct BooleanStateWithSetVector : public BooleanState {
  bool contains(const Ty &Elem) const { return Set.contains(Elem); }
  bool insert(const Ty &Elem) {
    if (InsertInvalidates)
      BooleanState::indicatePessimisticFixpoint();
    return Set.insert(Elem);
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

/// All the information we have about an EDT
struct EDTInfoState : AbstractState {
  /// EDT Node
  EDT *EDTInfo = nullptr;
  /// Flag to track if we reached a fixpoint.
  bool IsAtFixpoint = false;

  /// The EDTs we know are created by the associated EDT.
  BooleanStateWithPtrSetVector<EDT, /* InsertInvalidates */ false> ChildEDTs;

  /// The EDTS we know are reached from the associated EDT.
  BooleanStateWithPtrSetVector<EDT, /* InsertInvalidates */ false>
      ReachedChildEDTs;

  /// The set of EDTs we know we may signal from the associated EDT.
  BooleanStateWithPtrSetVector<EDT, /* InsertInvalidates */ false>
      MaySignalChildEDTs;
  DenseMap<EDT *, EDTValueSet> MaySignalEDTValues;

  /// The EDTS we know are dependent from the associated EDT.
  BooleanStateWithPtrSetVector<EDT, /* InsertInvalidates */ false>
      DependentEDTs;

  /// The set of EDTDataBlocks
  // BooleanStateWithPtrSetVector<EDTDataBlock, /* InsertInvalidates */ false>
  //     DataBlocks;

  EDT *getEDT() const { return EDTInfo; }

  /// Reachability
  bool canReachChild(EDT *E) const { return ReachedChildEDTs.contains(E); }

  /// Abstract State interface
  ///{
  EDTInfoState() {}
  EDTInfoState(bool BestState) {
    if (!BestState)
      indicatePessimisticFixpoint();
  }

  /// See AbstractState::isValidState(...)
  bool isValidState() const override { return true; }

  /// See AbstractState::isAtFixpoint(...)
  bool isAtFixpoint() const override { return IsAtFixpoint; }

  /// See AbstractState::indicatePessimisticFixpoint(...)
  ChangeStatus indicatePessimisticFixpoint() override {
    IsAtFixpoint = true;
    ChildEDTs.indicatePessimisticFixpoint();
    ReachedChildEDTs.indicatePessimisticFixpoint();
    MaySignalChildEDTs.indicatePessimisticFixpoint();
    DependentEDTs.indicatePessimisticFixpoint();
    return ChangeStatus::CHANGED;
  }

  /// See AbstractState::indicateOptimisticFixpoint(...)
  ChangeStatus indicateOptimisticFixpoint() override {
    IsAtFixpoint = true;
    ChildEDTs.indicateOptimisticFixpoint();
    ReachedChildEDTs.indicateOptimisticFixpoint();
    MaySignalChildEDTs.indicateOptimisticFixpoint();
    DependentEDTs.indicateOptimisticFixpoint();
    return ChangeStatus::UNCHANGED;
  }

  /// Return the assumed state
  EDTInfoState &getAssumed() { return *this; }
  const EDTInfoState &getAssumed() const { return *this; }

  bool operator==(const EDTInfoState &RHS) const {
    if (ChildEDTs != RHS.ChildEDTs)
      return false;
    if (ReachedChildEDTs != RHS.ReachedChildEDTs)
      return false;
    if (MaySignalChildEDTs != RHS.MaySignalChildEDTs)
      return false;
    if (DependentEDTs != RHS.DependentEDTs)
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
    ReachedChildEDTs ^= KIS.ReachedChildEDTs;
    MaySignalChildEDTs ^= KIS.MaySignalChildEDTs;
    DependentEDTs ^= KIS.DependentEDTs;
    return *this;
  }

  /// "Clamp" this state with \p KIS, ignoring the ChildEDTs and
  /// ReachedChildEDTs.
  EDTInfoState operator*=(const EDTInfoState &KIS) {
    MaySignalChildEDTs ^= KIS.MaySignalChildEDTs;
    DependentEDTs ^= KIS.DependentEDTs;
    return *this;
  }

  EDTInfoState operator&=(const EDTInfoState &KIS) { return (*this ^= KIS); }

  ///}
};

/// All the information we have about an EDT data block
struct EDTDataBlockInfoState : AbstractState {
  EDT *ParentEDT = nullptr;
  Value *DBVal = nullptr;
  /// The data block
  EDTDataBlock *DB = nullptr;
  /// Flag to track if we reached a fixpoint.
  bool IsAtFixpoint = false;
  /// Set of child EDTs (seen from the perspective of the ParentEDT) that
  /// are reached from the associated DataBlock.
  BooleanStateWithPtrSetVector<EDT, /* InsertInvalidates */ false>
      ReachedChildEDTs;
  /// Map of DataMovements
  DenseMap<EDT *, EDTSet> DataMovements;

  /// Size

  /// Usability

  /// Location

  /// INTERFACE
  EDT *getParentEDT() const { return ParentEDT; }
  Value *getDataBlockValue() const { return DBVal; }
  EDTDataBlock *getDataBlock() const { return DB; }

  /// Reachability
  bool canReach(EDT *E) const { return ReachedChildEDTs.contains(E); }

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

  /// See AbstractState::isValidState(...)
  bool isValidState() const override { return true; }

  /// See AbstractState::isAtFixpoint(...)
  bool isAtFixpoint() const override { return IsAtFixpoint; }

  /// See AbstractState::indicatePessimisticFixpoint(...)
  ChangeStatus indicatePessimisticFixpoint() override {
    IsAtFixpoint = true;
    ReachedChildEDTs.indicatePessimisticFixpoint();
    return ChangeStatus::CHANGED;
  }

  /// See AbstractState::indicateOptimisticFixpoint(...)
  ChangeStatus indicateOptimisticFixpoint() override {
    IsAtFixpoint = true;
    ReachedChildEDTs.indicateOptimisticFixpoint();
    return ChangeStatus::UNCHANGED;
  }

  /// Return the assumed state
  EDTDataBlockInfoState &getAssumed() { return *this; }
  const EDTDataBlockInfoState &getAssumed() const { return *this; }

  bool operator==(const EDTDataBlockInfoState &RHS) const {
    if (ReachedChildEDTs != RHS.ReachedChildEDTs)
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
    ReachedChildEDTs ^= KIS.ReachedChildEDTs;
    return *this;
  }

  EDTDataBlockInfoState operator&=(const EDTDataBlockInfoState &KIS) {
    return (*this ^= KIS);
  }

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

    auto ReachedChildEDTstr = [&]() {
      if (!ReachedChildEDTs.isValidState())
        return "<invalid> with " + std::to_string(ReachedChildEDTs.size()) +
               " EDTs\n";
      std::string Str;
      for (EDT *E : ReachedChildEDTs)
        Str += (Str.empty() ? "" : ", ") + std::to_string(E->getID());
      Str = "     #Reached ChildEDTs: " +
            std::to_string(ReachedChildEDTs.size()) + "{" + Str + "}\n";
      return Str;
    };

    auto ChildEDTStr = [&]() {
      if (!ChildEDTs.isValidState())
        return "<invalid> with " + std::to_string(ChildEDTs.size()) + " EDTs\n";
      std::string Str;
      for (EDT *E : ChildEDTs)
        Str += (Str.empty() ? "" : ", ") + std::to_string(E->getID());
      Str = "     #Child EDTs: " + std::to_string(ChildEDTs.size()) + "{" +
            Str + "}\n";
      return Str;
    };

    auto MaySignalEDTStr = [&]() {
      if (!MaySignalChildEDTs.isValidState())
        return "<invalid> with " + std::to_string(MaySignalChildEDTs.size()) +
               " EDTs\n";
      std::string Str;
      for (EDT *E : MaySignalChildEDTs)
        Str += (Str.empty() ? "" : ", ") + std::to_string(E->getID());
      Str =
          "     #MaySignal EDTs: " + std::to_string(MaySignalChildEDTs.size()) +
          "{" + Str + "}\n";
      return Str;
    };

    auto DepEDTStr = [&]() {
      if (!DependentEDTs.isValidState())
        return "<invalid> with " + std::to_string(DependentEDTs.size()) +
               " EDTs";
      std::string Str;
      for (EDT *E : DependentEDTs)
        Str += (Str.empty() ? "" : ", ") + std::to_string(E->getID());
      Str = "     #Dependent EDTs: " + std::to_string(DependentEDTs.size()) +
            "{" + Str + "}";
      return Str;
    };

    std::string EDTStr = "EDT #" + std::to_string(EDTInfo->getID()) + " -> \n";
    return (EDTStr + ChildEDTStr() + ReachedChildEDTstr() + MaySignalEDTStr() +
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
      return "<invalid>";

    auto ReachedChildEDTstr = [&]() {
      if (!ReachedChildEDTs.isValidState())
        return "<invalid> with " + std::to_string(ReachedChildEDTs.size()) +
               " EDTs";
      std::string Str;
      for (EDT *E : ReachedChildEDTs)
        Str += (Str.empty() ? "" : ", ") + std::to_string(E->getID());
      Str = "#Reached EDTs: " + std::to_string(ReachedChildEDTs.size()) + "{" +
            Str + "}";
      return Str;
    };

    std::string EDTStr = "EDTDataBlock -> ";
    return (EDTStr + ReachedChildEDTstr());
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

/// EDTInfoCache
struct EDTInfoCache : public InformationCache {
  EDTInfoCache(Module &M, AnalysisGetter &AG, BumpPtrAllocator &Allocator,
               SetVector<Function *> &Functions, EDTSet &EDTs,
               DenseMap<EDTFunction *, EDT *> &FunctionEDTMap)
      : InformationCache(M, AG, Allocator, &Functions), EDTs(EDTs),
        FunctionEDTMap(FunctionEDTMap) {}

  /// EDTGraph
  EDTGraph &getGraph() { return Graph; }

  /// EDTs
  EDT *getEDT(Function *F) const {
    auto Itr = FunctionEDTMap.find(F);
    if (Itr == FunctionEDTMap.end())
      return nullptr;
    return Itr->second;
  }

  EDT *getEDT(CallBase *CB) const { return getEDT(CB->getCalledFunction()); }

  /// EDTDataBlock
  EDTDataBlockSet getEDTDataBlocks(Value *V) const {
    auto Itr = ValueToDataBlocks.find(V);
    if (Itr == ValueToDataBlocks.end())
      return EDTDataBlockSet();
    return Itr->second;
  }

  EDTDataBlock *getOrCreateEDTDataBlock(Value *V, EDTDataBlock::Mode M) {
    auto Itr = ValueToDataBlocks.find(V);
    /// If the value is not in the map, create a new DataBlock
    if (Itr == ValueToDataBlocks.end()) {
      auto *DB = new EDTDataBlock(V, M);
      DataBlocks.insert(DB);
      ValueToDataBlocks[V].insert(DB);
      return DB;
    }
    /// If the value is in the map, check if the DataBlock is already there
    for (auto *DB : Itr->second) {
      if (DB->getMode() == M)
        return DB;
    }
    /// If the DataBlock is not there, create a new one
    auto *DB = new EDTDataBlock(V, M);
    DataBlocks.insert(DB);
    Itr->second.insert(DB);
    return DB;
  }

  /// Collection of known EDTs in the module
  EDTSet &EDTs;
  DenseMap<EDTFunction *, EDT *> &FunctionEDTMap;
  /// Collection of known DataBlocks in the module
  EDTDataBlockSet DataBlocks;
  DenseMap<EDTValue *, EDTDataBlockSet> ValueToDataBlocks;

  /// EDTGraph
  EDTGraph Graph;
};

/// The EDTFunction info abstract attribute, basically, what can we say
/// about a function with regards to the EDTInfoState.
struct AAEDTInfoFunction : AAEDTInfo {
  AAEDTInfoFunction(const IRPosition &IRP, Attributor &A) : AAEDTInfo(IRP, A) {}

  /// See AbstractAttribute::initialize(...).
  void initialize(Attributor &A) override {
    auto &EDTCache = static_cast<EDTInfoCache &>(A.getInfoCache());
    EDTFunction *Fn = getAnchorScope();
    EDTInfo = EDTCache.getEDT(Fn);
    if (!EDTInfo) {
      indicatePessimisticFixpoint();
      return;
    }
    LLVM_DEBUG(dbgs() << "[AAEDTInfoFunction::initialize] EDT #"
                      << EDTInfo->getID() << " for function \"" << Fn->getName()
                      << "\"\n");

    /// Create EDT Node in the graph
    EDTCache.getGraph().insertNode(EDTInfo);

    /// Callback to check all the calls to the EDT Function.
    auto CheckCallSite = [&](AbstractCallSite ACS) {
      auto *EDTCall = cast<CallBase>(ACS.getInstruction());
      auto *CalledEDT = EDTCache.getEDT(EDTCall);
      assert(CalledEDT == EDTInfo &&
             "EDTCall doesn't correspond to the EDTInfo of the function!");
      assert(!CalledEDT->getCall() && "EDT can only be called once!");
      CalledEDT->setCall(EDTCall);
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
      ///
    }
  }

  void updateChildEDTs(Attributor &A) {
    /// No need to update if we are already at a fixpoint
    if (ChildEDTs.isAtFixpoint() && ReachedChildEDTs.isAtFixpoint())
      return;

    auto &EDTCache = static_cast<EDTInfoCache &>(A.getInfoCache());
    /// Flags to determine fixpoint
    bool AllReachedChildEDTsWereFixed = true;
    /// Check calls to EDTChilds
    auto CheckCallInst = [&](Instruction &I) {
      auto &EDTCall = cast<CallBase>(I);
      auto *CalledEDT = EDTCache.getEDT(&EDTCall);
      if (!CalledEDT)
        return true;

      LLVM_DEBUG(dbgs() << "   - EDT #" << CalledEDT->getID()
                        << " is a child of EDT #" << EDTInfo->getID() << "\n");

      /// Add the EDT to the set of ChildEDTs and ReachedChildEDTs.
      ChildEDTs.insert(CalledEDT);
      ReachedChildEDTs.insert(CalledEDT);
      /// Add the creation edge to the graph
      EDTCache.getGraph().addCreationEdge(EDTInfo, CalledEDT);

      /// Run AAEDTInfo on the ChildEDT
      auto *ChildEDTAA = A.getAAFor<AAEDTInfo>(
          *this, IRPosition::function(*CalledEDT->getFn()),
          DepClassTy::OPTIONAL);
      if (!ChildEDTAA)
        return false;
      /// Clamp set of ReachedChildEDTs of the ChildEDT
      ReachedChildEDTs ^= ChildEDTAA->ReachedChildEDTs;
      /// Fixpoint check
      AllReachedChildEDTsWereFixed &=
          ChildEDTAA->ReachedChildEDTs.isAtFixpoint();
      return true;
    };

    bool UsedAssumedInformationInCheckCallInst = false;
    if (!A.checkForAllCallLikeInstructions(
            CheckCallInst, *this, UsedAssumedInformationInCheckCallInst)) {
      LLVM_DEBUG(dbgs() << TAG
                        << "Failed to visit all call-like instructions!\n";);
      indicatePessimisticFixpoint();
    }

    /// If we got to this point, we know that all children were created
    ChildEDTs.indicateOptimisticFixpoint();
    /// Indicate Optimistic Fixpoint if all the ReachedChildEDTs are fixed
    if (AllReachedChildEDTsWereFixed)
      ReachedChildEDTs.indicateOptimisticFixpoint();
  }

  void updateDependencies(Attributor &A) {
    /// Analyze all the maysignal EDTs and check if it is a dependency
    /// or not. If it is a dependency, add it to the DependentEDTs set.
    /// Basically it is a dependency it the value is a not a dependency
    /// of any of my childEDT.
    /// Check DataMovements map to see if the EDT is a dependency
  }

  /// Fixpoint iteration update function. Will be called every time a
  /// dependence changed its state (and in the beginning).
  ChangeStatus updateImpl(Attributor &A) override {
    auto &EDTCache = static_cast<EDTInfoCache &>(A.getInfoCache());
    LLVM_DEBUG(dbgs() << "[AAEDTInfoFunction::updateImpl] EDT #"
                      << EDTInfo->getID() << "\n";);
    EDTInfoState StateBefore = getState();

    /// Update the ChildEDTs and ReachedChildEDTs of the EDT
    updateChildEDTs(A);

    /// Run AAEDTInfo on the called to EDT
    if (auto *EDTCall = EDTInfo->getCall()) {
      auto *EDTCallAA = A.getOrCreateAAFor<AAEDTInfo>(
          IRPosition::callsite_function(*EDTCall), this, DepClassTy::OPTIONAL);
      getState() *= EDTCallAA->getState();

      if (EDTCallAA->getState().isAtFixpoint())
        indicateOptimisticFixpoint();
    }

    return StateBefore == getState() ? ChangeStatus::UNCHANGED
                                     : ChangeStatus::CHANGED;
  }

  /// Modify the IR based on the EDTInfoState as the fixpoint iteration is
  /// finished now.
  ChangeStatus manifest(Attributor &A) override {
    /// Debug output info
    LLVM_DEBUG(dbgs() << "AAEDTInfoFunction::manifest: " << getAsStr(&A)
                      << "\n");
    if (EDTInfo)
      LLVM_DEBUG(dbgs() << *EDTInfo << "\n");
    ChangeStatus Changed = ChangeStatus::UNCHANGED;
    return Changed;
  }
};

struct AAEDTInfoCallsite : AAEDTInfo {
  AAEDTInfoCallsite(const IRPosition &IRP, Attributor &A) : AAEDTInfo(IRP, A) {}

  /// See AbstractAttribute::initialize(...).
  void initialize(Attributor &A) override {
    auto &EDTCache = static_cast<EDTInfoCache &>(A.getInfoCache());
    /// Get the Called EDT
    auto &EDTCall = cast<CallBase>(getAnchorValue());
    EDTInfo = EDTCache.getEDT(EDTCall.getCalledFunction());
    assert(EDTInfo && "EDTInfo is null!");
    LLVM_DEBUG(dbgs() << "[AAEDTInfoCallsite::initialize] EDT #"
                      << EDTInfo->getID() << "\n");
  }

  /// See AbstractAttribute::updateImpl(Attributor &A).
  ChangeStatus updateImpl(Attributor &A) override {
    EDTInfoState StateBefore = getState();
    LLVM_DEBUG(dbgs() << "[AAEDTInfoCallsite::updateImpl] EDT #"
                      << EDTInfo->getID() << "\n");

    auto *EDTCall = EDTInfo->getCall();
    /// Run AAEDTDataBlockInfo on each argument of the call instruction
    bool AllDBsWereFixed = true;
    for (uint32_t CallArgItr = 0; CallArgItr < EDTCall->data_operands_size();
         ++CallArgItr) {
      auto *ArgEDTDataBlockAA = A.getOrCreateAAFor<AAEDTInfo>(
          IRPosition::callsite_argument(*EDTCall, CallArgItr), this,
          DepClassTy::OPTIONAL, false, true);
      AllDBsWereFixed &= ArgEDTDataBlockAA->isAtFixpoint();
      /// Here we clamp the set of MaySignalChildEDTs and DependentEDTs
      getState() *= ArgEDTDataBlockAA->getState();
    }

    if (AllDBsWereFixed) {
      LLVM_DEBUG(dbgs() << "   - All DataBlocks were fixed for EDT #"
                        << EDTInfo->getID() << "\n");
      indicateOptimisticFixpoint();
    }
    return StateBefore == getState() ? ChangeStatus::UNCHANGED
                                     : ChangeStatus::CHANGED;
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
    auto &EDTCache = static_cast<EDTInfoCache &>(A.getInfoCache());
    /// Get the called EDT
    CallBase &EDTCall = cast<CallBase>(getAnchorValue());
    EDTInfo = EDTCache.getEDT(EDTCall.getCalledFunction());
    assert(EDTInfo && "CalledEDT is null!");

    LLVM_DEBUG(dbgs() << "[AAEDTInfoCallsiteArg::initialize] CallArg #"
                      << getCallSiteArgNo() << " from EDT #" << EDTInfo->getID()
                      << "\n");
  }

  /// See AbstractAttribute::updateImpl(Attributor &A).
  ChangeStatus updateImpl(Attributor &A) override {
    auto &EDTCache = static_cast<EDTInfoCache &>(A.getInfoCache());
    auto *ParentEDT = EDTCache.getEDT(getAnchorScope());
    EDT *CalledEDT = getEDT();
    LLVM_DEBUG(dbgs() << "[AAEDTInfoCallsiteArg::updateImpl] "
                      << getAssociatedValue() << " from EDT #"
                      << CalledEDT->getID() << "\n");
    ChangeStatus Changed = ChangeStatus::UNCHANGED;
    EDTInfoState StateBefore = getState();

    /// Get the value associated with the argument. In case of load/store, get
    /// the pointer operand.
    Value &ArgVal = getAssociatedValue();
    Value *V = nullptr;
    if (auto *I = dyn_cast<Instruction>(&ArgVal)) {
      if (auto *LI = dyn_cast<LoadInst>(I))
        V = LI->getPointerOperand();
      else if (auto *SI = dyn_cast<StoreInst>(I))
        V = SI->getPointerOperand();
    }
    /// If the value is not an load/store instruction, use the value itself
    if (!V)
      V = &ArgVal;

    /// Run AAUnderlyingObjects (inter/intra procedural analysis) on the value
    const auto *UnderlyingObjsAA = A.getAAFor<AAUnderlyingObjects>(
        *this, IRPosition::value(*V), DepClassTy::OPTIONAL);
    if (!UnderlyingObjsAA) {
      LLVM_DEBUG(dbgs() << "  - Failed to get AAUnderlyingObjects!\n");
      return indicatePessimisticFixpoint();
    }
    if (!UnderlyingObjsAA->getState().isAtFixpoint()) {
      LLVM_DEBUG(dbgs() << "   - Underlying objects are not at fixpoint!\n");
      return Changed;
    }

    /// Lambda to check the underlying object
    Value *UnderlyingObjVal = nullptr;
    uint64_t UnderlyingObjsCount = 0;
    auto CheckUnderlyingObj = [&](Value &Obj,
                                  AA::ValueScope Scope = AA::AnyScope) {
      LLVM_DEBUG(dbgs() << "   - Underlying object: " << Obj << "\n");
      UnderlyingObjsCount++;
      UnderlyingObjVal = &Obj;
      return true;
    };

    if (!UnderlyingObjsAA->forallUnderlyingObjects(CheckUnderlyingObj)) {
      LLVM_DEBUG(dbgs() << "   - Failed to visit all underlying objects!\n");
      return indicatePessimisticFixpoint();
    }

    if (UnderlyingObjsCount > 1) {
      LLVM_DEBUG(dbgs() << "Multiple underlying objects not supported yet.\n");
      return indicatePessimisticFixpoint();
    }

    /// Assert expected instruction
    Instruction *UnderlyingObjInst = dyn_cast<Instruction>(UnderlyingObjVal);
    assert(UnderlyingObjInst && "Underlying object is not an instruction!");
    /// Run AAEDTDataBlockInfo on the underlying object value
    auto *UnderlyingObjDBInfoAA = A.getAAFor<AAEDTDataBlockInfo>(
        *this, IRPosition::value(*UnderlyingObjInst), DepClassTy::OPTIONAL);
    if (!UnderlyingObjDBInfoAA) {
      LLVM_DEBUG(dbgs() << "   - Failed to get AAEDTDataBlockInfo!\n");
      return indicatePessimisticFixpoint();
    }
    if (!UnderlyingObjDBInfoAA->ReachedChildEDTs.isAtFixpoint()) {
      LLVM_DEBUG(dbgs() << "   - Reached ChildEDTs of underlying object "
                           "DataBlock is not at fixpoint!\n");
      return Changed;
    }

    /// Now we have to check whether the underlying object is local (belongs to
    /// the parent EDT) or remote (belongs to another EDT).
    auto *UnderlyingEDT = EDTCache.getEDT(UnderlyingObjInst->getFunction());
    bool IsLocal = (UnderlyingEDT == ParentEDT);
    /// If it is local, clamp the list of ReachedChildEDTs of the DataBlock with
    /// the MaySignalEDTs.
    if (IsLocal) {
      LLVM_DEBUG(
          dbgs() << "   - Underlying object belongs to the parent EDT!\n");
      /// We only care about dependencies if the CalledEDT is synchronous
      if (CalledEDT->isAsync()) {
        LLVM_DEBUG(dbgs() << "   - CalledEDT is asynchronous!\n");
        return indicateOptimisticFixpoint();
      }

      /// Merge the state of the underlying object with the state of the
      /// argument. Continue if it reaches the CalledEDT.
      for (auto *ChildEDT : UnderlyingObjDBInfoAA->ReachedChildEDTs) {
        if (ChildEDT == CalledEDT)
          continue;
        LLVM_DEBUG(dbgs() << "   - MaySignalChildEDTs: EDT #"
                          << ChildEDT->getID() << "\n");
        MaySignalChildEDTs.insert(ChildEDT);
      }
      indicateOptimisticFixpoint();
    }
    /// If it is remote, check whether the DB reaches any childEDTs that the
    /// called EDT can not reach.
    else {
      LLVM_DEBUG(dbgs() << "   - Underlying object does not belong to the "
                           "parent EDT!. It belongs to EDT #"
                        << UnderlyingEDT->getID() << "\n");
      auto *CalledEDTAA = A.getAAFor<AAEDTInfo>(
          *this, IRPosition::function(*CalledEDT->getFn()), DepClassTy::NONE);

      for (auto *ChildEDT : UnderlyingObjDBInfoAA->ReachedChildEDTs) {
        auto *ChildEDTAA = A.getAAFor<AAEDTInfo>(
            *this, IRPosition::function(*ChildEDT->getFn()), DepClassTy::NONE);
        /// Continue if the childEDT is an ancestor of the called EDT
        if (ChildEDTAA->canReachChild(CalledEDT)) {
          LLVM_DEBUG(dbgs() << "   - EDT #" << ChildEDT->getID()
                            << " can reach " << CalledEDT->getID() << "\n");
          continue;
        }
        if (CalledEDTAA->ChildEDTs.size() == 0) {
          LLVM_DEBUG(dbgs() << "   - DependentEDTs: EDT #" << ChildEDT->getID()
                            << "\n");
          DependentEDTs.insert(ChildEDT);
          /// Add data dependency to the graph
          EDTCache.getGraph().addDataEdge(CalledEDT, ChildEDT, UnderlyingObjVal);
        } else {
          LLVM_DEBUG(dbgs() << "   - MaySignalChildEDTs: EDT #"
                            << ChildEDT->getID() << "\n");
          MaySignalChildEDTs.insert(ChildEDT);
        }
      }
      indicateOptimisticFixpoint();
    }

    /// If we got to this point, we know all the dependencies of the value
    // return indicateOptimisticFixpoint();
    Changed = StateBefore == getState() ? ChangeStatus::UNCHANGED
                                        : ChangeStatus::CHANGED;
    LLVM_DEBUG(dbgs() << "[AAEDTInfoCallsiteArg::updateImpl] EDT #"
                      << EDTInfo->getID() << " Changed: "
                      << (Changed == ChangeStatus::CHANGED ? "YES" : "NO")
                      << "\n");
    return Changed;
  }

  /// See AbstractAttribute::manifest(Attributor &A).
  ChangeStatus manifest(Attributor &A) override {
    return ChangeStatus::UNCHANGED;
  }
};

/// The EDTDataBlock info abstract attribute, basically, what can we say
/// about a DataBlock with regards to the EDTDataBlockInfoState.
struct AAEDTDataBlockInfoVal : AAEDTDataBlockInfo {
  AAEDTDataBlockInfoVal(const IRPosition &IRP, Attributor &A)
      : AAEDTDataBlockInfo(IRP, A) {}

  /// See AbstractAttribute::initialize(...).
  void initialize(Attributor &A) override {
    DBVal = &getAssociatedValue();
    auto *Inst = dyn_cast<Instruction>(DBVal);
    assert(isa<Instruction>(Inst) && "Value is not an instruction!");

    /// Set EDT DataBlock info
    auto &EDTCache = static_cast<EDTInfoCache &>(A.getInfoCache());
    ParentEDT = EDTCache.getEDT(Inst->getFunction());
    /// NOTE: Later on, create EDT here based on the mode.
    /// ReadWrite if the values is a dependency
    /// ReadOnly if the value is a parameter
    LLVM_DEBUG(dbgs() << "[AAEDTDataBlockInfoVal::initialize] Value #" << *DBVal
                      << "\n");
  }

  void updateReachedEDTs(Attributor &A) {
    if (ReachedChildEDTs.isAtFixpoint())
      return;

    /// Run AAPointerInfo on the value
    const AAPointerInfo *PI = A.getAAFor<AAPointerInfo>(
        *this, IRPosition::value(*DBVal), DepClassTy::OPTIONAL);
    if (!PI || !PI->getState().isValidState()) {
      LLVM_DEBUG(dbgs() << "   - AAPointerInfo Failed or is invalid!\n");
      indicatePessimisticFixpoint();
    }

    if (!PI->getState().isAtFixpoint()) {
      LLVM_DEBUG(dbgs() << "   - AAPointerInfo is not at fixpoint!\n");
    }

    auto &EDTCache = static_cast<EDTInfoCache &>(A.getInfoCache());
    for (auto PIItr = PI->begin(); PIItr != PI->end(); ++PIItr) {
      const auto &It = PI->begin();
      // const AA::RangeTy &Range = It->first;
      const SmallSet<unsigned, 4> &AccessIndex = It->second;
      /// Analyze all accesses
      for (auto AI : AccessIndex) {
        const AAPointerInfo::Access &Acc = PI->getAccess(AI);
        Instruction *LocalInst = Acc.getLocalInst();
        /// Analyze dependencies on Call Instruction
        if (auto *CBTo = dyn_cast<CallBase>(LocalInst)) {
          /// Check if the CallInst is a call to an EDT
          auto *ToEDT = EDTCache.getEDT(CBTo->getCalledFunction());
          if (!ToEDT)
            continue;

          if (ReachedChildEDTs.insert(ToEDT)) {
            LLVM_DEBUG(dbgs() << "   - ReachedChildEDTs: EDT #"
                              << ToEDT->getID() << "\n");
          }
        }
      }
    }

    /// If we got to this point, we know all the ReachedChildEDTs of the value
    ReachedChildEDTs.indicateOptimisticFixpoint();
  }

  /// See AbstractAttribute::updateImpl(Attributor &A).
  ChangeStatus updateImpl(Attributor &A) override {
    LLVM_DEBUG(dbgs() << "[AAEDTDataBlockInfoVal::updateImpl] "
                      << getAssociatedValue() << "\n");
    ChangeStatus Changed = ChangeStatus::UNCHANGED;
    EDTDataBlockInfoState StateBefore = getState();
    updateReachedEDTs(A);

    /// Keep going as long as we are not at fixpoint
    if (!ReachedChildEDTs.isValidState())
      return indicatePessimisticFixpoint();

    if (!ReachedChildEDTs.isAtFixpoint())
      return Changed;

    /// Analyze data movements based on the ReachedChildEDTs
    // for (auto *ChildEDT : ReachedChildEDTs) {
    //   auto *ChildEDTAA = A.getAAFor<AAEDTInfo>(
    //       *this, IRPosition::function(*ChildEDT->getFn()), DepClassTy::NONE);
    //   if (!ChildEDTAA)
    //     continue;
    // }
    indicateOptimisticFixpoint();
    return StateBefore == getState() ? ChangeStatus::UNCHANGED
                                     : ChangeStatus::CHANGED;
  }

  /// See AbstractAttribute::manifest(Attributor &A).
  ChangeStatus manifest(Attributor &A) override {
    return ChangeStatus::UNCHANGED;
  }
};

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
  case IRPosition::IRP_ARGUMENT:
  case IRPosition::IRP_RETURNED:
  case IRPosition::IRP_CALL_SITE_RETURNED:
  case IRPosition::IRP_CALL_SITE_ARGUMENT:
  case IRPosition::IRP_CALL_SITE:
  case IRPosition::IRP_FUNCTION:
    llvm_unreachable(
        "EDTDataBlockInfo can only be created for floating position!");
  case IRPosition::IRP_FLOAT:
    AA = new (A.Allocator) AAEDTDataBlockInfoVal(IRP, A);
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
    /// Get all functions and EDTs of the module
    LLVM_DEBUG(dbgs() << TAG << "Creating and Initializing EDTs: \n");
    EDTSet EDTs;
    SetVector<Function *> Functions;
    DenseMap<EDTFunction *, EDT *> FunctionEDTMap;
    for (Function &Fn : M) {
      if (Fn.isDeclaration() && !Fn.hasLocalLinkage())
        continue;
      Functions.insert(&Fn);
      if (EDTMetadata *MD = EDTMetadata::getMetadata(Fn)) {
        EDT *CurrentEDT = EDT::get(MD);
        /// Change name if it is not the main function
        // Fn.setName("carts_edt." + std::to_string(CurrentEDT->getID()));
        EDTs.insert(CurrentEDT);
        FunctionEDTMap[&Fn] = CurrentEDT;
      }
    }
    LLVM_DEBUG(dbgs() << "\n\n" << TAG << "Initializing AAEDTInfo: \n");
    /// Create attributor
    FunctionAnalysisManager &FAM =
        AM.getResult<FunctionAnalysisManagerModuleProxy>(M).getManager();
    AnalysisGetter AG(FAM);
    CallGraphUpdater CGUpdater;
    BumpPtrAllocator Allocator;
    EDTInfoCache InfoCache(M, AG, Allocator, Functions, EDTs, FunctionEDTMap);
    AttributorConfig AC(CGUpdater);
    AC.DefaultInitializeLiveInternals = false;
    AC.IsModulePass = true;
    AC.RewriteSignatures = false;
    AC.MaxFixpointIterations = 32;
    AC.DeleteFns = false;
    AC.PassName = DEBUG_TYPE;
    Attributor A(Functions, InfoCache, AC);

    /// Register AAs
    for (EDT *E : EDTs) {
      A.getOrCreateAAFor<AAEDTInfo>(IRPosition::function(*E->getFn()),
                                    /* QueryingAA */ nullptr, DepClassTy::NONE,
                                    /* ForceUpdate */ false,
                                    /* UpdateAfterInit */ false);
    }
    ChangeStatus Changed = A.run();
    LLVM_DEBUG(dbgs() << "[Attributor] Done with " << Functions.size()
                      << " functions, result: " << Changed << ".\n");

    /// Print module info
    LLVM_DEBUG(dbgs() << "\n"
                      << "-------------------------------------------------\n");
    LLVM_DEBUG(dbgs() << TAG << "Process has finished\n");
    InfoCache.getGraph().print();
    LLVM_DEBUG(dbgs() << "\n"
                      << "-------------------------------------------------\n");
    return PreservedAnalyses::all();
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