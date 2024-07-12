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
  EDT *ParentEDT = nullptr;
  EDT *ParentSyncEDT = nullptr;

  /// For sync EDTs
  EDT *DoneEDT = nullptr;

  /// Flag to track if we reached a fixpoint.
  bool IsAtFixpoint = false;

  /// The EDTs we know are created by the associated EDT.
  BooleanStateWithPtrSetVector<EDT, /* InsertInvalidates */ false> ChildEDTs;

  /// The EDTS we know are reached from the associated EDT.
  BooleanStateWithPtrSetVector<EDT, /* InsertInvalidates */ false>
      ReachedChildEDTs;

  /// The EDTS we know are dependent from the associated EDT.
  BooleanStateWithPtrSetVector<EDT, /* InsertInvalidates */ false>
      DependentEDTs;

  /// The set of EDTs we know we may signal from the associated EDT.
  BooleanStateWithPtrSetVector<EDT, /* InsertInvalidates */ false>
      MaySignalLocalEDTs;
  BooleanStateWithPtrSetVector<EDT, /* InsertInvalidates */ false>
      MaySignalRemoteEDTs;
  DenseMap<EDT *, EDTValueSet> MaySignalEDTValues;

  /// The set of EDTDataBlocks
  // BooleanStateWithPtrSetVector<EDTDataBlock, /* InsertInvalidates */ false>
  //     DataBlocks;

  EDT *getEDT() const { return EDTInfo; }
  EDT *getParentEDT() const { return ParentEDT; }
  EDT *getParentSyncEDT() const { return ParentSyncEDT; }
  EDT *getDoneEDT() const { return DoneEDT; }

  /// Reachability
  bool canReachChild(EDT *E) const { return ReachedChildEDTs.contains(E); }

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
    ReachedChildEDTs.indicatePessimisticFixpoint();
    DependentEDTs.indicatePessimisticFixpoint();
    /// MaySignalEDTs
    MaySignalLocalEDTs.indicatePessimisticFixpoint();
    MaySignalRemoteEDTs.indicatePessimisticFixpoint();
    return ChangeStatus::CHANGED;
  }

  /// See AbstractState::indicateOptimisticFixpoint(...)
  ChangeStatus indicateOptimisticFixpoint() override {
    IsAtFixpoint = true;
    ChildEDTs.indicateOptimisticFixpoint();
    ReachedChildEDTs.indicateOptimisticFixpoint();
    DependentEDTs.indicateOptimisticFixpoint();
    /// MaySignalEDTs
    MaySignalLocalEDTs.indicateOptimisticFixpoint();
    MaySignalRemoteEDTs.indicateOptimisticFixpoint();
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
    if (DependentEDTs != RHS.DependentEDTs)
      return false;
    if (MaySignalLocalEDTs != RHS.MaySignalLocalEDTs)
      return false;
    if (MaySignalRemoteEDTs != RHS.MaySignalRemoteEDTs)
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
    DependentEDTs ^= KIS.DependentEDTs;
    /// MaySignalEDTs
    MaySignalLocalEDTs ^= KIS.MaySignalLocalEDTs;
    MaySignalRemoteEDTs ^= KIS.MaySignalRemoteEDTs;
    for (auto &Itr : KIS.MaySignalEDTValues) {
      MaySignalEDTValues[Itr.first].insert(Itr.second.begin(),
                                           Itr.second.end());
    }
    return *this;
  }

  /// "Clamp" this state with \p KIS, ignoring the ChildEDTs and
  /// ReachedChildEDTs.
  EDTInfoState operator*=(const EDTInfoState &KIS) {
    DependentEDTs ^= KIS.DependentEDTs;
    /// MaySignalEDTs
    MaySignalLocalEDTs ^= KIS.MaySignalLocalEDTs;
    MaySignalRemoteEDTs ^= KIS.MaySignalRemoteEDTs;
    for (auto &Itr : KIS.MaySignalEDTValues) {
      MaySignalEDTValues[Itr.first].insert(Itr.second.begin(),
                                           Itr.second.end());
    }
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
  /// Set of Local EDTs that are reached from the associated EDTDataBlock.
  BooleanStateWithPtrSetVector<EDT, /* InsertInvalidates */ false> ReachedEDTs;
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
  bool canReach(EDT *E) const { return ReachedEDTs.contains(E); }

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
    ReachedEDTs.indicatePessimisticFixpoint();
    return ChangeStatus::CHANGED;
  }

  /// See AbstractState::indicateOptimisticFixpoint(...)
  ChangeStatus indicateOptimisticFixpoint() override {
    IsAtFixpoint = true;
    ReachedEDTs.indicateOptimisticFixpoint();
    return ChangeStatus::UNCHANGED;
  }

  /// Return the assumed state
  EDTDataBlockInfoState &getAssumed() { return *this; }
  const EDTDataBlockInfoState &getAssumed() const { return *this; }

  bool operator==(const EDTDataBlockInfoState &RHS) const {
    if (ReachedEDTs != RHS.ReachedEDTs)
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
    ReachedEDTs ^= KIS.ReachedEDTs;
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

    auto ReachedChildEDTStr = [&]() {
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

    auto MaySignalLocalEDTStr = [&]() {
      if (!MaySignalLocalEDTs.isValidState())
        return "<invalid> with " + std::to_string(MaySignalLocalEDTs.size()) +
               " EDTs\n";
      std::string Str;
      for (EDT *E : MaySignalLocalEDTs)
        Str += (Str.empty() ? "" : ", ") + std::to_string(E->getID());
      Str = "     #MaySignalLocal EDTs: " +
            std::to_string(MaySignalLocalEDTs.size()) + "{" + Str + "}\n";
      return Str;
    };

    auto MaySignalRemoteEDTStr = [&]() {
      if (!MaySignalRemoteEDTs.isValidState())
        return "<invalid> with " + std::to_string(MaySignalRemoteEDTs.size()) +
               " EDTs\n";
      std::string Str;
      for (EDT *E : MaySignalRemoteEDTs)
        Str += (Str.empty() ? "" : ", ") + std::to_string(E->getID());
      Str = "     #MaySignalRemote EDTs: " +
            std::to_string(MaySignalRemoteEDTs.size()) + "{" + Str + "}\n";
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
    return (EDTStr + ChildEDTStr() + ReachedChildEDTStr() +
            MaySignalLocalEDTStr() + MaySignalRemoteEDTStr() + DepEDTStr());
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

    auto ReachedEDTsstr = [&]() {
      if (!ReachedEDTs.isValidState())
        return "<invalid> with " + std::to_string(ReachedEDTs.size()) + " EDTs";
      std::string Str;
      for (EDT *E : ReachedEDTs)
        Str += (Str.empty() ? "" : ", ") + std::to_string(E->getID());
      Str = "#Reached EDTs: " + std::to_string(ReachedEDTs.size()) + "{" + Str +
            "}";
      return Str;
    };

    std::string DBStr = "EDTDataBlock -> ";
    return (DBStr + ReachedEDTsstr());
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
      assert(!CalledEDT->getCall() && "Multiple calls to EDTFunction not supported yet.");
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

  bool updateChildEDTs(Attributor &A) {
    /// No need to update if we are already at a fixpoint
    if (ChildEDTs.isAtFixpoint() && ReachedChildEDTs.isAtFixpoint())
      return true;

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
      assert(ChildEDTAA && "ChildEDTAA is null!");
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
      return false;
    }

    /// If we got to this point, we know that all children were created
    ChildEDTs.indicateOptimisticFixpoint();
    /// Indicate Optimistic Fixpoint if all the ReachedChildEDTs are fixed
    if (AllReachedChildEDTsWereFixed) {
      ReachedChildEDTs.indicateOptimisticFixpoint();
      return true;
    }
    return false;
  }

  void updateDependencies(Attributor &A) {
    /// Analyze all the MaySignal EDTs and check if it is a dependency
    /// or not. If it is a dependency, add it to the DependentEDTs set.
    /// Basically it is a dependency it the value is a not a dependency
    /// of any of my childEDT.
    auto &EDTCache = static_cast<EDTInfoCache &>(A.getInfoCache());
    auto &EDTGraph = EDTCache.getGraph();

    for (auto *MaySignalEDT : MaySignalLocalEDTs) {
      /// Get values to signal from the MaySignalEDTValues map
      auto &ValuesToSignal = MaySignalEDTValues[MaySignalEDT];
      bool IsDependency = true;
      ///
      LLVM_DEBUG(dbgs() << "   - MaySignalEDT #" << MaySignalEDT->getID()
                        << " has " << ValuesToSignal.size() << " values\n");
      for (auto *V : ValuesToSignal) {
        LLVM_DEBUG(dbgs() << "     - Value: " << *V << "\n");
        /// Check if the value is a dependency of any of the ChildEDTs
        // bool IsDependencyOfChildEDTs = false;
        // for(auto *ChildEDT : ChildEDTs) {
        //   auto *ChildEDTAA = A.getAAFor<AAEDTInfo>(
        //       *this, IRPosition::function(*ChildEDT->getFn()),
        //       DepClassTy::OPTIONAL);
        //   assert(ChildEDTAA && "ChildEDTAA is null!");
        //   if (ChildEDTAA->MaySignalEDTs.contains(MaySignalEDT)) {
        //     IsDependencyOfChildEDTs = true;
        //     break;
        //   }
        // }
        // if (!IsDependencyOfChildEDTs) {
        //   IsDependency = false;
        //   break;
        // }
      }
      // for (auto *ChildEDT : ChildEDTs) {
      //   auto *ChildEDTAA = A.getAAFor<AAEDTInfo>(
      //       *this, IRPosition::function(*ChildEDT->getFn()),
      //       DepClassTy::OPTIONAL);
      //   assert(ChildEDTAA && "ChildEDTAA is null!");
      //   if (ChildEDTAA->MaySignalEDTs.contains(MaySignalEDT)) {
      //     IsDependency = false;
      //     break;
      //   }
      // }
      // if (IsDependency)
      //   DependentEDTs.insert(MaySignalEDT);
    }
    /// Check DataMovements map to see if the EDT is a dependency
  }

  /// Fixpoint iteration update function. Will be called every time a
  /// dependence changed its state (and in the beginning).
  ChangeStatus updateImpl(Attributor &A) override {
    LLVM_DEBUG(dbgs() << "[AAEDTInfoFunction::updateImpl] EDT #"
                      << EDTInfo->getID() << "\n";);
    EDTInfoState StateBefore = getState();

    /// Update the ChildEDTs and ReachedChildEDTs of the EDT
    /// It continues only if the ChildEDTs and ReachedChildEDTs is at a fixpoint
    if (!updateChildEDTs(A))
      return hasChanged(StateBefore);

    /// If not EDTCall, return optimistic fixpoint, for now.
    auto *EDTCall = EDTInfo->getCall();
    if (!EDTCall)
      return indicateOptimisticFixpoint();

    /// Run AAEDTInfo on the EDTCall
    auto *EDTCallAA = A.getOrCreateAAFor<AAEDTInfo>(
        IRPosition::callsite_function(*EDTCall), this, DepClassTy::OPTIONAL);
    getState() *= EDTCallAA->getState();
    /// Continue if MaySignalEDTs is at a fixpoint
    if (!EDTCallAA->MaySignalLocalEDTs.isAtFixpoint())
      return hasChanged(StateBefore);

    /// Analyze MaySignalEDTs and check if it is a dependency
    // updateDependencies(A);

    /// For now, Indicate optimistic fixpoint
    indicateOptimisticFixpoint();

    /// Return the change status
    return hasChanged(StateBefore);
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

/// AAEDTInfoCallsite
/// Everything we know about an EDT from its corresponding call site.
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
    bool AllMaySignalEDTsWereFixed = true;
    for (uint32_t CallArgItr = 0; CallArgItr < EDTCall->data_operands_size();
         ++CallArgItr) {
      auto *ArgEDTDataBlockAA = A.getOrCreateAAFor<AAEDTInfo>(
          IRPosition::callsite_argument(*EDTCall, CallArgItr), this,
          DepClassTy::OPTIONAL, false, true);
      AllDBsWereFixed &= ArgEDTDataBlockAA->isAtFixpoint();
      AllMaySignalEDTsWereFixed &=
          ArgEDTDataBlockAA->MaySignalLocalEDTs.isAtFixpoint();
      /// Here we clamp the set of MaySignalEDTs and DependentEDTs
      getState() *= ArgEDTDataBlockAA->getState();
    }

    if (AllMaySignalEDTsWereFixed)
      MaySignalLocalEDTs.indicateOptimisticFixpoint();

    if (AllDBsWereFixed) {
      LLVM_DEBUG(dbgs() << "   - All DataBlocks were fixed for EDT #"
                        << EDTInfo->getID() << "\n");
      indicateOptimisticFixpoint();
    }

    return hasChanged(StateBefore);
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

    /// If the value is a not a dependency of the EDT, indicate optimistic
    /// fixpoint
    if (!EDTInfo->isDep(getCallSiteArgNo()))
      indicateOptimisticFixpoint();

    /// Get the value associated with the argument. In case of load/store, get
    /// the pointer operand.
    Value &V = getAssociatedValue();
    if (auto *I = dyn_cast<Instruction>(&V)) {
      if (auto *LI = dyn_cast<LoadInst>(I))
        ArgVal = LI->getPointerOperand();
      else if (auto *SI = dyn_cast<StoreInst>(I))
        ArgVal = SI->getPointerOperand();
    }
    /// If the value is not an load/store instruction, use the value itself
    if (!ArgVal)
      ArgVal = &V;
  }

  /// See AbstractAttribute::updateImpl(Attributor &A).
  ChangeStatus updateImpl(Attributor &A) override {
    EDT *CalledEDT = getEDT();
    LLVM_DEBUG(dbgs() << "[AAEDTInfoCallsiteArg::updateImpl] "
                      << getAssociatedValue() << " from EDT #"
                      << CalledEDT->getID() << "\n");
    ChangeStatus Changed = ChangeStatus::UNCHANGED;
    Changed |= analyzeLocalDeps(A);
    Changed |= analyzeRemoteDeps(A);
    return Changed;
  }

  ChangeStatus analyzeLocalDeps(Attributor &A) {
    /// Run AAEDTDataBlockInfo on the associated value
    auto *ArgValDBInfoAA = A.getAAFor<AAEDTDataBlockInfo>(
        *this, IRPosition::value(*ArgVal), DepClassTy::OPTIONAL);
    if (!ArgValDBInfoAA) {
      LLVM_DEBUG(dbgs() << "   - Failed to get AAEDTDataBlockInfo for value: "
                        << *ArgVal << "\n");
      return indicatePessimisticFixpoint();
    }
    if (!ArgValDBInfoAA->ReachedEDTs.isAtFixpoint()) {
      return ChangeStatus::UNCHANGED;
    }

    return ChangeStatus::UNCHANGED;
  }

  ChangeStatus analyzeRemoteDeps(Attributor &A) {
    EDTInfoState StateBefore = getState();
    auto &EDTCache = static_cast<EDTInfoCache &>(A.getInfoCache());
    EDT *ParentEDT = EDTCache.getEDT(getAnchorScope());
    EDT *CalledEDT = getEDT();

    /// Run AAUnderlyingObjects (inter/intra procedural analysis) on the value
    /// NOTE: This has to be modified. We do not need the Underlying Object, but
    const auto *UnderlyingObjsAA = A.getAAFor<AAUnderlyingObjects>(
        *this, IRPosition::value(*ArgVal), DepClassTy::OPTIONAL);
    if (!UnderlyingObjsAA) {
      LLVM_DEBUG(dbgs() << "  - Failed to get AAUnderlyingObjects!\n");
      return indicatePessimisticFixpoint();
    }
    if (!UnderlyingObjsAA->getState().isAtFixpoint()) {
      return ChangeStatus::UNCHANGED;
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
      /// NOTE: Here we should probably add control dependencies. For now,
      ///       we just indicate pessimistic fixpoint.
      return indicatePessimisticFixpoint();
    }

    /// Run AAEDTDataBlockInfo on the underlying object value
    auto *UnderlyingObjDBInfoAA = A.getAAFor<AAEDTDataBlockInfo>(
        *this, IRPosition::value(*UnderlyingObjVal), DepClassTy::OPTIONAL);
    if (!UnderlyingObjDBInfoAA) {
      LLVM_DEBUG(
          dbgs()
          << "   - Failed to get AAEDTDataBlockInfo for Underlying object: "
          << *UnderlyingObjVal << "\n");
      return indicatePessimisticFixpoint();
    }
    if (!UnderlyingObjDBInfoAA->ReachedEDTs.isAtFixpoint())
      return ChangeStatus::UNCHANGED;

    /// Now we have to check whether the underlying object belongs to
    /// the parent EDT) or remote (belongs to another EDT).
    EDT *UnderlyingEDT = UnderlyingObjDBInfoAA->getParentEDT();
    if (UnderlyingEDT == ParentEDT) {
      /// If it belongs to parent, clamp the list of ReachedChildEDTs of the
      /// DataBlock with the MaySignalEDTs.
      /// We only care about dependencies if the CalledEDT is synchronous
      if (CalledEDT->isAsync()) {
        LLVM_DEBUG(dbgs() << "   - CalledEDT is asynchronous!\n");
        return indicateOptimisticFixpoint();
      }

      /// Merge the state of the underlying object with the state of the
      /// argument. Continue if it reaches the CalledEDT.
      for (auto *ReachedEDT : UnderlyingObjDBInfoAA->ReachedEDTs) {
        if (ReachedEDT == CalledEDT)
          continue;
        LLVM_DEBUG(dbgs() << "   - MaySignalEDTs: EDT #" << ReachedEDT->getID()
                          << "\n");
        MaySignalRemoteEDTs.insert(ReachedEDT);
        MaySignalEDTValues[ReachedEDT].insert(UnderlyingObjVal);
      }

      /// We found all we needed, indicate optimistic fixpoint
      MaySignalRemoteEDTs.indicateOptimisticFixpoint();
      return hasChanged(StateBefore);
    }

    /// If it is remote, check whether the DB reaches any childEDTs that the
    /// called EDT can not reach.
    LLVM_DEBUG(dbgs() << "   - Underlying object does not belong to the "
                         "parent EDT!. It belongs to EDT #"
                      << UnderlyingEDT->getID() << "\n");
    auto *CalledEDTAA = A.getAAFor<AAEDTInfo>(
        *this, IRPosition::function(*CalledEDT->getFn()), DepClassTy::NONE);

    for (auto *ChildEDT : UnderlyingObjDBInfoAA->ReachedEDTs) {
      /// Get the AAEDTInfo for the childEDT
      auto *ChildEDTAA = A.getAAFor<AAEDTInfo>(
          *this, IRPosition::function(*ChildEDT->getFn()), DepClassTy::NONE);
      /// Continue if the childEDT is an ancestor of the called EDT or if it has
      /// any childEDTs
      if (ChildEDTAA->canReachChild(CalledEDT) ||
          CalledEDTAA->ChildEDTs.size() > 0)
        continue;

      /// We know for sure that there is a dependency if the calledEDT does not
      /// have any ChildEDTs
      LLVM_DEBUG(dbgs() << "   - DependentEDTs: EDT #" << ChildEDT->getID()
                        << "\n");
      DependentEDTs.insert(ChildEDT);
      /// Add data dependency to the graph
      EDTCache.getGraph().addDataEdge(CalledEDT, ChildEDT, UnderlyingObjVal);
    }
    /// Report changes
    return hasChanged(StateBefore);
  }

  /// See AbstractAttribute::manifest(Attributor &A).
  ChangeStatus manifest(Attributor &A) override {
    return ChangeStatus::UNCHANGED;
  }

  /// Value to analyze
  Value *ArgVal = nullptr;
};

/// The EDTDataBlock info abstract attribute, basically, what can we say
/// about a DataBlock with regards to the EDTDataBlockInfoState.
struct AAEDTDataBlockInfoVal : AAEDTDataBlockInfo {
  AAEDTDataBlockInfoVal(const IRPosition &IRP, Attributor &A)
      : AAEDTDataBlockInfo(IRP, A) {}

  /// See AbstractAttribute::initialize(...).
  void initialize(Attributor &A) override {
    DBVal = &getAssociatedValue();

    /// Set EDT DataBlock info
    auto &EDTCache = static_cast<EDTInfoCache &>(A.getInfoCache());
    ParentEDT = EDTCache.getEDT(getAnchorScope());

    /// NOTE: Later on, create EDT here based on the mode.
    /// ReadWrite if the values is a dependency
    /// ReadOnly if the value is a parameter
    LLVM_DEBUG(dbgs() << "[AAEDTDataBlockInfoVal::initialize] Value #" << *DBVal
                      << "\n");
  }

  void updateReachedEDTs(Attributor &A) {
    if (ReachedEDTs.isAtFixpoint())
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
      return;
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
        auto *CBTo = dyn_cast<CallBase>(LocalInst);
        if (!CBTo)
          continue;
        /// Check if the CallInst is a call to an EDT
        auto *ToEDT = EDTCache.getEDT(CBTo->getCalledFunction());
        if (!ToEDT)
          continue;

        if (ReachedEDTs.insert(ToEDT)) {
          LLVM_DEBUG(dbgs()
                     << "   - ReachedEDTs: EDT #" << ToEDT->getID() << "\n");
        }
      }
    }

    /// If we got to this point, we know all the ReachedEDTs of the value
    ReachedEDTs.indicateOptimisticFixpoint();
  }

  /// See AbstractAttribute::updateImpl(Attributor &A).
  ChangeStatus updateImpl(Attributor &A) override {
    LLVM_DEBUG(dbgs() << "[AAEDTDataBlockInfoVal::updateImpl] "
                      << getAssociatedValue() << "\n");
    EDTDataBlockInfoState StateBefore = getState();
    updateReachedEDTs(A);

    /// Keep going as long as we are not at fixpoint
    if (!ReachedEDTs.isValidState())
      return indicatePessimisticFixpoint();

    if (!ReachedEDTs.isAtFixpoint())
      return hasChanged(StateBefore);

    /// Analyze data movements based on the ReachedChildEDTs
    // for (auto *ChildEDT : ReachedChildEDTs) {
    //   auto *ChildEDTAA = A.getAAFor<AAEDTInfo>(
    //       *this, IRPosition::function(*ChildEDT->getFn()), DepClassTy::NONE);
    //   if (!ChildEDTAA)
    //     continue;
    // }
    /// Indicate optimistic fixpoint for now
    indicateOptimisticFixpoint();
    return hasChanged(StateBefore);
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
  case IRPosition::IRP_RETURNED:
  case IRPosition::IRP_CALL_SITE_RETURNED:
  case IRPosition::IRP_CALL_SITE_ARGUMENT:
  case IRPosition::IRP_CALL_SITE:
  case IRPosition::IRP_FUNCTION:
    llvm_unreachable(
        "EDTDataBlockInfo can only be created for floating position!");
  case IRPosition::IRP_ARGUMENT:
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