// Description: Main file for the ARTS Analysis pass.
#include <cassert>
#include <cstdint>

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/ADT/SmallVector.h"
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
struct EDTInfoState;
struct EDTUpdateInfo {
public:
  enum UpdateType { NewParentEDT, NewDependentEDT };
  EDTUpdateInfo(UpdateType Ty, EDTInfoState *EDTUpdate)
      : Ty(Ty), EDTUpdate(EDTUpdate) {}
  ~EDTUpdateInfo();

  /// Getters
  UpdateType getType() const { return Ty; }
  EDTInfoState &getInfo() const { return *EDTUpdate; }

private:
  UpdateType Ty;
  EDTInfoState *EDTUpdate = nullptr;
};

struct CARTSInfoCache : public InformationCache {
  CARTSInfoCache(Module &M, AnalysisGetter &AG, BumpPtrAllocator &Allocator,
                 SetVector<Function *> &Functions, EDTSet &EDTs,
                 DenseMap<EDTFunction *, EDT *> &FunctionEDTMap,
                 EDTGraph &Graph)
      : InformationCache(M, AG, Allocator, &Functions), EDTs(EDTs),
        FunctionEDTMap(FunctionEDTMap), Graph(Graph) {}

  /// Helper functions
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

  /// CARTSInfoCache

  /// EDTGraph
  EDTGraph &getGraph() { return Graph; }
  void insertGraphNode(EDT *E) { Graph.insertNode(E); }

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

  /// Updates Interface
  bool hasUpdate(EDT *E) const { return UpdateMap.count(E); }
  void pushUpdate(EDT *E, EDTUpdateInfo *Info) { UpdateMap[E].push_back(Info); }
  void pushNewParentEDT(EDT *ToUpdateEDT, EDT *ParentEDT);
  void pushNewDependentEDT(EDT *ToUpdateEDT, EDT *DependentEDT);
  EDTUpdateInfo *popUpdate(EDT *E) {
    auto Itr = UpdateMap.find(E);
    if (Itr == UpdateMap.end())
      return nullptr;
    EDTUpdateInfo *Info = Itr->second.pop_back_val();
    /// If the update list is empty, remove the entry
    if (Itr->second.empty())
      UpdateMap.erase(Itr);
    return Info;
  }

  /// Map of Updates
  DenseMap<EDT *, SmallVector<EDTUpdateInfo *, 4>> UpdateMap;
  /// Collection of known EDTs in the module
  EDTSet &EDTs;
  DenseMap<EDTFunction *, EDT *> &FunctionEDTMap;
  /// EDTGraph
  EDTGraph &Graph;
  /// Collection of known DataBlocks in the module
  EDTDataBlockSet DataBlocks;
  DenseMap<EDTValue *, EDTDataBlockSet> ValueToDataBlocks;
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
      ReachedDescendantEDTs;

  /// The EDTs we know are dependent from the associated EDT.
  BooleanStateWithPtrSetVector<EDT, /* InsertInvalidates */ false>
      DependentEDTs;
  DenseMap<EDT *, EDTValueSet> DependentEDTValues;

  /// The set of EDTs we know we may signal from the associated EDT.
  BooleanStateWithPtrSetVector<EDT, /* InsertInvalidates */ false>
      MaySignalLocalEDTs;
  BooleanStateWithPtrSetVector<EDT, /* InsertInvalidates */ false>
      MaySignalRemoteEDTs;
  DenseMap<EDT *, EDTValueSet> MaySignalEDTValues;

  /// Insert
  bool insertChildEDT(EDT *ChildEDT) {
    CARTSCache->getGraph().addCreationEdge(EDTInfo, ChildEDT);
    return ChildEDTs.insert(ChildEDT);
  }

  bool insertDependentEDT(EDT *ToEDT, EDTValue *V) {
    CARTSCache->getGraph().addDataEdge(EDTInfo, ToEDT, V);
    DependentEDTValues[ToEDT].insert(V);
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
    return ReachedDescendantEDTs.contains(E) || ChildEDTs.contains(E);
  }

  bool isReachabilityInfoAtFixpoint() const {
    return ReachedDescendantEDTs.isAtFixpoint() && ChildEDTs.isAtFixpoint();
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
    ReachedDescendantEDTs.indicatePessimisticFixpoint();
    DependentEDTs.indicatePessimisticFixpoint();
    /// MaySignalEDTs
    MaySignalLocalEDTs.indicatePessimisticFixpoint();
    MaySignalRemoteEDTs.indicatePessimisticFixpoint();
    /// Sync Info
    ParentSyncEDT.indicatePessimisticFixpoint();
    return ChangeStatus::CHANGED;
  }

  /// See AbstractState::indicateOptimisticFixpoint(...)
  ChangeStatus indicateOptimisticFixpoint() override {
    IsAtFixpoint = true;
    ChildEDTs.indicateOptimisticFixpoint();
    ReachedDescendantEDTs.indicateOptimisticFixpoint();
    DependentEDTs.indicateOptimisticFixpoint();
    /// MaySignalEDTs
    MaySignalLocalEDTs.indicateOptimisticFixpoint();
    MaySignalRemoteEDTs.indicateOptimisticFixpoint();
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
    if (ReachedDescendantEDTs != RHS.ReachedDescendantEDTs)
      return false;
    if (DependentEDTs != RHS.DependentEDTs)
      return false;
    if (MaySignalLocalEDTs != RHS.MaySignalLocalEDTs)
      return false;
    if (MaySignalRemoteEDTs != RHS.MaySignalRemoteEDTs)
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
    ReachedDescendantEDTs ^= KIS.ReachedDescendantEDTs;
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
  /// ReachedDescendantEDTs.
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

    auto ReachedDescendantEDTstr = [&]() {
      if (!ReachedDescendantEDTs.isValidState())
        return "     -Reached DescendantEDTs invalid with " +
               std::to_string(ReachedDescendantEDTs.size()) + " EDTs\n";
      std::string Str;
      for (EDT *E : ReachedDescendantEDTs)
        Str += (Str.empty() ? "" : ", ") + std::to_string(E->getID());
      Str = "     #Reached DescendantEDTs: " +
            std::to_string(ReachedDescendantEDTs.size()) + "{" + Str + "}\n";
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

    auto MaySignalLocalEDTStr = [&]() {
      if (!MaySignalLocalEDTs.isValidState())
        return "     -MaySignalLocal EDTs invalid with " +
               std::to_string(MaySignalLocalEDTs.size()) + " EDTs\n";
      std::string Str;
      for (EDT *E : MaySignalLocalEDTs)
        Str += (Str.empty() ? "" : ", ") + std::to_string(E->getID());
      Str = "     #MaySignalLocal EDTs: " +
            std::to_string(MaySignalLocalEDTs.size()) + "{" + Str + "}\n";
      return Str;
    };

    auto MaySignalRemoteEDTStr = [&]() {
      if (!MaySignalRemoteEDTs.isValidState())
        return "     -MaySignalRemote EDTs invalid with " +
               std::to_string(MaySignalRemoteEDTs.size()) + " EDTs\n";
      std::string Str;
      for (EDT *E : MaySignalRemoteEDTs)
        Str += (Str.empty() ? "" : ", ") + std::to_string(E->getID());
      Str = "     #MaySignalRemote EDTs: " +
            std::to_string(MaySignalRemoteEDTs.size()) + "{" + Str + "}\n";
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
        "\nEDT #" + std::to_string(EDTInfo->getID()) + " -> \n";
    if (ParentEDT)
      EDTStr +=
          "     -ParentEDT: EDT #" + std::to_string(ParentEDT->getID()) + "\n";
    return (EDTStr + ParentSyncEDTStr() + ChildEDTStr() +
            ReachedDescendantEDTstr() + MaySignalLocalEDTStr() +
            MaySignalRemoteEDTStr() + DepEDTStr());
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
struct EDTDataBlockInfoState : AbstractState {
  /// Pointer to the CARTSInfoCache
  CARTSInfoCache *CARTSCache = nullptr;

  /// NOTE: EDTDataBlock should have a context. Fix this later

  /// The EDT that creates the data block
  EDT *ParentEDT = nullptr;
  Value *DBVal = nullptr;
  /// The data block
  EDTDataBlock *DB = nullptr;
  /// Flag to track if we reached a fixpoint.
  bool IsAtFixpoint = false;
  /// Set of Local EDTs that are reached from the associated EDTDataBlock.
  BooleanStateWithPtrSetVector<EDT, false> ReachedLocalEDTs;
  BooleanStateWithPtrSetVector<EDT, false> ReachedRemoteEDTs;

  /// Set of EDTs MayBeMoved to
  // BooleanStateWithPtrSetVector<EDT, false> MayBeMovedToEDTs;
  // DenseMap<EDT *, EDTSet> MayBeMovedToEDTsMap;
  /// Map of DataMovements
  DenseMap<EDT *, EDTSet> DataMovements;

  /// Size

  /// Usability

  /// Location

  /// INTERFACE
  EDT *getParentEDT() const { return ParentEDT; }
  Value *getValue() const { return DBVal; }
  EDTDataBlock *getDataBlock() const { return DB; }

  /// Reachability
  bool canLocallyReach(EDT *E) const { return ReachedLocalEDTs.contains(E); }
  bool canRemotelyReach(EDT *E) const { return ReachedRemoteEDTs.contains(E); }
  bool canReach(EDT *E) const {
    return canLocallyReach(E) || canRemotelyReach(E);
  }

  bool isReachabilityInfoAtFixpoint() const {
    return ReachedLocalEDTs.isAtFixpoint() && ReachedRemoteEDTs.isAtFixpoint();
  }

  /// MayBeMovedToEDTs
  // bool canBeMovedTo(EDT *E) const { return MayBeMovedToEDTs.contains(E); }

  // void insertMayBeMovedToEDT(EDT *From, EDT *To) {
  //   MayBeMovedToEDTs.insert(To);
  //   MayBeMovedToEDTsMap[To].insert(From);
  // }

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
    ReachedLocalEDTs.indicatePessimisticFixpoint();
    return ChangeStatus::CHANGED;
  }

  /// See AbstractState::indicateOptimisticFixpoint(...)
  ChangeStatus indicateOptimisticFixpoint() override {
    IsAtFixpoint = true;
    ReachedLocalEDTs.indicateOptimisticFixpoint();
    ReachedRemoteEDTs.indicateOptimisticFixpoint();
    return ChangeStatus::UNCHANGED;
  }

  /// Return the assumed state
  EDTDataBlockInfoState &getAssumed() { return *this; }
  const EDTDataBlockInfoState &getAssumed() const { return *this; }

  bool operator==(const EDTDataBlockInfoState &RHS) const {
    if (ReachedLocalEDTs != RHS.ReachedLocalEDTs)
      return false;
    if (ReachedRemoteEDTs != RHS.ReachedRemoteEDTs)
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
    ReachedLocalEDTs ^= KIS.ReachedLocalEDTs;
    ReachedRemoteEDTs ^= KIS.ReachedRemoteEDTs;
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

    auto ReachedLocalEDTsStr = [&]() {
      if (!ReachedLocalEDTs.isValidState())
        return "     - ReachedLocalEDTs invalid with " +
               std::to_string(ReachedLocalEDTs.size()) + " EDTs\n";
      std::string Str;
      for (EDT *E : ReachedLocalEDTs)
        Str += (Str.empty() ? "" : ", ") + std::to_string(E->getID());
      Str = "     - #ReachedLocalEDTs: " +
            std::to_string(ReachedLocalEDTs.size()) + "{" + Str + "}\n";
      return Str;
    };

    auto ReachedRemoteEDTsStr = [&]() {
      if (!ReachedRemoteEDTs.isValidState())
        return "     - ReachedRemoteEDTs invalid with " +
               std::to_string(ReachedRemoteEDTs.size()) + " EDTs";
      std::string Str;
      for (EDT *E : ReachedRemoteEDTs)
        Str += (Str.empty() ? "" : ", ") + std::to_string(E->getID());
      Str = "     - #ReachedRemoteEDTs: " +
            std::to_string(ReachedRemoteEDTs.size()) + "{" + Str + "}\n";
      return Str;
    };

    std::string DBStr = "EDTDataBlock ->\n";
    if (ParentEDT)
      DBStr +=
          "     - ParentEDT: EDT #" + std::to_string(ParentEDT->getID()) + "\n";
    return (DBStr + ReachedLocalEDTsStr() + ReachedRemoteEDTsStr());
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

  /// To trigger updates from the parent EDT to the child EDTs
  bool isQueryAA() const override { return true; }

  /// See AbstractAttribute::initialize(...).
  void initialize(Attributor &A) override {
    CARTSCache = &static_cast<CARTSInfoCache &>(A.getInfoCache());
    EDTFunction *Fn = getAnchorScope();
    EDTInfo = CARTSCache->getEDT(Fn);
    if (!EDTInfo) {
      indicatePessimisticFixpoint();
      return;
    }
    LLVM_DEBUG(dbgs() << "[AAEDTInfoFunction::initialize] EDT #"
                      << EDTInfo->getID() << " for function \"" << Fn->getName()
                      << "\"\n");
    CARTSCache->insertGraphNode(EDTInfo);

    /// Callback to check all the calls to the EDT Function.
    uint32_t EDTCallCounter = 0;
    auto CheckCallSite = [&](AbstractCallSite ACS) {
      EDTCallCounter++;
      /// Verify it is a call to the EDT Function
      EDTCall = cast<CallBase>(ACS.getInstruction());
      auto *CalledEDT = CARTSCache->getEDT(EDTCall);
      assert(CalledEDT == EDTInfo &&
             "EDTCall doesn't correspond to the EDTInfo of the function!");
      assert(EDTCallCounter == 1 &&
             "Multiple calls to EDTFunction not supported yet.");

      /// Set EDTCall
      EDTInfo->setCall(EDTCall);

      /// Set ParentEDT - Is the EDT called from another EDT?
      ParentEDT = CARTSCache->getEDT(EDTCall->getCaller());
      LLVM_DEBUG(dbgs() << "   - ParentEDT: EDT #" << ParentEDT->getID()
                        << "\n");
      assert(ParentEDT && "EDT is not called from another EDT");

      /// Check if it is a sync EDT
      if (EDTInfo->isAsync())
        return true;

      /// SyncEDTs must have a DoneEDT.
      /// It is usually in the first instruction of the next BB.
      /// Start by looking at the next instruction after the EDTCall
      auto *NextBB = EDTCall->getParent()->getNextNode();
      auto *CB = dyn_cast<CallBase>(&NextBB->front());
      assert(CB && "Next instruction is not a CallBase!");
      DoneEDT = CARTSCache->getEDT(CB);
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

  /// Update the ChildEDTs and ReachedDescendantEDTs of the EDT. It returns true
  /// if the ChildEDTs and ReachedDescendantEDTs is at a fixpoint
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
      /// Clamp set of ReachedDescendantEDTs of the ChildEDT
      ReachedDescendantEDTs ^= ChildEDTAA->ReachedDescendantEDTs;
      ReachedDescendantEDTs ^= ChildEDTAA->ChildEDTs;
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
    /// Indicate Optimistic Fixpoint if all the ReachedDescendantEDTs are fixed
    if (!AllReachedEDTsWereFixed)
      return false;
    LLVM_DEBUG(dbgs() << "   - All ReachedEDTs are fixed for EDT #"
                      << EDTInfo->getID() << "\n");
    ReachedDescendantEDTs.indicateOptimisticFixpoint();
    pushParentSyncUpdates(A);
    return true;
  }

  void pushParentSyncUpdates(Attributor &A) {
    /// If the current EDT is sync, push updates to the child and/or descendant
    /// EDTs
    if (EDTInfo->isAsync()) {
      LLVM_DEBUG(dbgs() << "   - EDT #" << EDTInfo->getID()
                        << " is async. No updates to push\n");
      return;
    }

    LLVM_DEBUG(dbgs() << "   - Pushing updates from EDT #" << EDTInfo->getID()
                      << "\n");
    /// Debug EDT Type
    SyncEDT &SyncEDTInfo = *dyn_cast<SyncEDT>(EDTInfo);
    SetVector<EDT *> EDTsToUpdate;
    /// Add the ChildEDTs to the EDTsToUpdate set
    if (SyncEDTInfo.mustSyncChilds()) {
      LLVM_DEBUG(dbgs() << "   - EDT #" << EDTInfo->getID()
                        << " must sync ChildEDTs: " << ChildEDTs.size()
                        << "\n");
      EDTsToUpdate.insert(ChildEDTs.begin(), ChildEDTs.end());
    }

    /// Add the ReachedDescendantEDTs to the EDTsToUpdate set
    if (SyncEDTInfo.mustSyncDescendants()) {
      LLVM_DEBUG(dbgs() << "   - EDT #" << EDTInfo->getID()
                        << " must sync DescendantEDTs: "
                        << ReachedDescendantEDTs.size() << "\n");
      EDTsToUpdate.insert(ReachedDescendantEDTs.begin(),
                          ReachedDescendantEDTs.end());
    }

    for (auto *E : EDTsToUpdate) {
      LLVM_DEBUG(dbgs() << "      - Pushing update to EDT #" << E->getID()
                        << "\n");
      CARTSCache->pushNewParentEDT(E, EDTInfo);
      /// Register update in the AA
      auto *ChildEDTAA =
          A.lookupAAFor<AAEDTInfo>(IRPosition::function(*E->getFn()));
      A.registerForUpdate(*ChildEDTAA);
      // ChildEDTAA->ParentSyncEDT.set(EDTInfo);
    }
  }

  /// Check Cache to get updates if any.
  void getUpdates(Attributor &A) {
    LLVM_DEBUG(dbgs() << "   - Getting updates for EDT #" << EDTInfo->getID()
                      << "\n");
    SetVector<EDTUpdateInfo *> AddBack;
    while (CARTSCache->hasUpdate(EDTInfo)) {
      bool RemoveUpdate = false;
      /// Pop the update
      EDTUpdateInfo *EDTUpdate = CARTSCache->popUpdate(EDTInfo);
      assert(EDTUpdate && "EDTUpdate is null!");
      /// Process the update
      switch (EDTUpdate->getType()) {
      case EDTUpdateInfo::NewParentEDT:
        RemoveUpdate = updateParentSyncEDT(A, *EDTUpdate);
        break;
      case EDTUpdateInfo::NewDependentEDT:
        RemoveUpdate = updateDependentEDTs(A, *EDTUpdate);
        break;
      }

      /// Push the update back if it is not removed
      if (!RemoveUpdate)
        AddBack.insert(EDTUpdate);
      else
        delete EDTUpdate;
    }

    /// Add the updates back
    for (EDTUpdateInfo *EDTUpdate : AddBack)
      CARTSCache->pushUpdate(EDTInfo, EDTUpdate);
    LLVM_DEBUG(dbgs() << "   - Finished getting updates for EDT #"
                      << EDTInfo->getID() << "\n");
  }

  /// Update the ParentSyncEDT. Returns true if the update can be removed
  bool updateParentSyncEDT(Attributor &A, EDTUpdateInfo &EDTUpdate) {
    if (ParentSyncEDT.isAtFixpoint())
      return true;

    EDT *NewParentEDT = EDTUpdate.getInfo().getParentEDT();
    assert(NewParentEDT && "NewParentEDT is null!");
    auto *NewParentEDTAA =
        A.lookupAAFor<AAEDTInfo>(IRPosition::function(*NewParentEDT->getFn()));

    if (!ParentSyncEDT.get()) {
      ParentSyncEDT.set(NewParentEDT);
      DoneEDT = NewParentEDTAA->getDoneEDT();
      return true;
    }

    if (ParentSyncEDT.get() == NewParentEDT)
      return true;

    /// If the new ParentSyncEDT reaches the current ParentSyncEDT, ignore it
    if (!NewParentEDTAA->isReachabilityInfoAtFixpoint()) {
      A.recordDependence(*NewParentEDTAA, *this, DepClassTy::OPTIONAL);
      DoneEDT = NewParentEDTAA->getDoneEDT();
      return false;
    }

    /// If it reaches the current ParentSyncEDT, it means that the current is
    // a descendant of the new one. So, do not update the parent, but remove the
    // update
    if (NewParentEDTAA->canReach(ParentSyncEDT.get()))
      return true;

    ParentSyncEDT.set(NewParentEDT);
    return true;
  }

  /// Update DependentEDTs. Returns true if the update can be removed
  bool updateDependentEDTs(Attributor &A, EDTUpdateInfo &EDTUpdate) {
    llvm_unreachable("Not implemented yet!");
    return true;
  }

  /// Fixpoint iteration update function. Will be called every time a
  /// dependence changed its state (and in the beginning).
  ChangeStatus updateImpl(Attributor &A) override {
    LLVM_DEBUG(dbgs() << "[AAEDTInfoFunction::updateImpl] EDT #"
                      << EDTInfo->getID() << "\n";);
    EDTInfoState StateBefore = getState();

    if (!updateChildEDTs(A))
      return hasChanged(StateBefore);

    /// If not EDTCall, return optimistic fixpoint, for now.
    if (!EDTCall)
      return indicateOptimisticFixpoint();

    /// Attend any external update, if any
    getUpdates(A);

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
    LLVM_DEBUG(dbgs() << "[AAEDTInfoCallsite::initialize] EDT #"
                      << EDTInfo->getID() << "\n");
  }

  /// See AbstractAttribute::updateImpl(Attributor &A).
  ChangeStatus updateImpl(Attributor &A) override {
    EDTInfoState StateBefore = getState();
    LLVM_DEBUG(dbgs() << "[AAEDTInfoCallsite::updateImpl] EDT #"
                      << EDTInfo->getID() << "\n");

    CallBase &EDTCall = cast<CallBase>(getAnchorValue());
    /// Run AAEDTDataBlockInfo on each argument of the call instruction
    bool AllDBsWereFixed = true;
    bool AllMaySignalEDTsWereFixed = true;
    for (uint32_t CallArgItr = 0; CallArgItr < EDTCall.data_operands_size();
         ++CallArgItr) {
      auto *ArgEDTDataBlockAA = A.getOrCreateAAFor<AAEDTInfo>(
          IRPosition::callsite_argument(EDTCall, CallArgItr), this,
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
      // indicateOptimisticFixpoint();
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
    CARTSCache = &static_cast<CARTSInfoCache &>(A.getInfoCache());

    /// Get the called EDT
    CallBase &EDTCall = cast<CallBase>(getAnchorValue());
    EDTInfo = CARTSCache->getEDT(EDTCall.getCalledFunction());
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
    ArgVal = CARTSCache->getPointerVal(&getAssociatedValue());
  }

  /// See AbstractAttribute::updateImpl(Attributor &A).
  ChangeStatus updateImpl(Attributor &A) override {
    EDT *CalledEDT = getEDT();
    LLVM_DEBUG(dbgs() << "[AAEDTInfoCallsiteArg::updateImpl] "
                      << getAssociatedValue() << " from EDT #"
                      << CalledEDT->getID() << "\n");
    ChangeStatus Changed = ChangeStatus::UNCHANGED;

    /// Run AAEDTDataBlockInfo on the associated value
    auto *ArgValDBInfoAA = A.getAAFor<AAEDTDataBlockInfo>(
        *this, IRPosition::value(*ArgVal), DepClassTy::OPTIONAL);
    // A.lookupAAFor<AAEDTDataBlockInfo>(
    //     IRPosition::value(*ArgVal), this, DepClassTy::NONE);
    if (!ArgValDBInfoAA || !ArgValDBInfoAA->getState().isValidState()) {
      LLVM_DEBUG(dbgs() << "     - Failed to get AAEDTDataBlockInfo for value: "
                        << *ArgVal << "\n");
      return indicatePessimisticFixpoint();
    }

    /// Record the dependence only if ReachedEDTs is not at a fixpoint
    if (!ArgValDBInfoAA->isReachabilityInfoAtFixpoint()) {
      A.recordDependence(*ArgValDBInfoAA, *this, DepClassTy::OPTIONAL);
      return ChangeStatus::UNCHANGED;
    }

    Changed |= analyzeLocalDeps(A, ArgValDBInfoAA);
    Changed |= analyzeSyncRemoteDeps(A, ArgValDBInfoAA);
    return Changed;
  }

  ChangeStatus analyzeLocalDeps(Attributor &A,
                                const AAEDTDataBlockInfo *DBInfo) {
    if (MaySignalLocalEDTs.isAtFixpoint())
      return ChangeStatus::UNCHANGED;
    LLVM_DEBUG(dbgs() << "   - Analyzing local dependencies\n");
    /// We are only concerned with local dependencies if syncs EDTs
    EDT *CalledEDT = getEDT();
    if (CalledEDT->isAsync()) {
      LLVM_DEBUG(dbgs() << "     - No local deps, EDT is asynchronous!\n");
      MaySignalLocalEDTs.indicateOptimisticFixpoint();
      return ChangeStatus::CHANGED;
    }

    /// Fill the MaySignalLocalEDTs
    for (auto *ReachedEDT : DBInfo->ReachedLocalEDTs) {
      if (ReachedEDT == CalledEDT)
        continue;
      MaySignalLocalEDTs.insert(ReachedEDT);
      MaySignalEDTValues[ReachedEDT].insert(ArgVal);
    }
    MaySignalLocalEDTs.indicateOptimisticFixpoint();
    return ChangeStatus::CHANGED;
  }

  ChangeStatus analyzeSyncRemoteDeps(Attributor &A,
                                     const AAEDTDataBlockInfo *DBInfo) {
    LLVM_DEBUG(dbgs() << "   - Analyzing sync remote dependencies\n");
    EDT *CalledEDT = getEDT();
    if (CalledEDT->isAsync())
      return ChangeStatus::UNCHANGED;

    auto *CalledEDTAA = A.getAAFor<AAEDTInfo>(
        *this, IRPosition::function(*CalledEDT->getFn()), DepClassTy::OPTIONAL);
    EDT *CalledEDTDone = CalledEDTAA->getDoneEDT();

    EDTGraph &Graph = CARTSCache->getGraph();
    auto *DBVal = DBInfo->getValue();
    SetVector<AAEDTInfo *> DependOnEDTsAA, MayDependOnEDTsAA;
    for (auto *ReachedEDT : DBInfo->ReachedRemoteEDTs) {
      /// Get the AAEDTInfo for the ReachedEDT with an optional dependency
      /// in case the ParentSyncEDT changes
      auto *ReachedEDTAA =
          A.lookupAAFor<AAEDTInfo>(IRPosition::function(*ReachedEDT->getFn()),
                                   this, DepClassTy::OPTIONAL);
      /// We are only concerned with the EDTs that are synced with the called
      /// EDT
      EDT *ParentSyncOfReachedEDT = ReachedEDTAA->ParentSyncEDT.get();
      if (ParentSyncOfReachedEDT != CalledEDT)
        continue;
      /// If reachedEDTAA has no children, it is a dependency
      if (ReachedEDTAA->ChildEDTs.empty()) {
        DependOnEDTsAA.insert(ReachedEDTAA);
        Graph.addDataEdge(ReachedEDT, CalledEDTDone, DBVal);
        LLVM_DEBUG(dbgs() << "     - EDT #" << ReachedEDT->getID()
                          << " must signal the value to EDT #"
                          << CalledEDTDone->getID() << "\n");
      } else {
        MayDependOnEDTsAA.insert(ReachedEDTAA);
      }
    }

    /// Iterate over the MayDependOnEDTs and check if any of the DependOnEDTs
    /// are reachable from them.
    for (auto *MayDependOnEDT : MayDependOnEDTsAA) {
      bool CanReach = false;
      for (auto *DependOnEDT : DependOnEDTsAA) {
        if (MayDependOnEDT->canReach(DependOnEDT->getEDT())) {
          CanReach = true;
          break;
        }
      }
      /// Skip it if it can reach any of the DependOnEDTs
      if (CanReach)
        continue;

      Graph.addDataEdge(MayDependOnEDT->getEDT(), CalledEDTDone, DBVal);
      DependOnEDTsAA.insert(MayDependOnEDT);
      LLVM_DEBUG(dbgs() << "     - EDT #" << MayDependOnEDT->getEDT()->getID()
                        << " must signal the value to EDT #"
                        << CalledEDTDone->getID() << "\n");
    }

    /// Push updates to the DependOnEDTs
    SetVector<EDT *> SetOfDependOnEDTs;
    for (auto *DependOnEDTAA : DependOnEDTsAA) {
      SetOfDependOnEDTs.insert(DependOnEDTAA->getEDT());
      A.registerForUpdate(*DependOnEDTAA);
    }
    // CARTSCache.pushNewDependentsEDT(ReachedEDT, SetOfDependOnEDTs);
    return ChangeStatus::UNCHANGED;
  }

  /// See AbstractAttribute::manifest(Attributor &A).
  ChangeStatus manifest(Attributor &A) override {
    return ChangeStatus::UNCHANGED;
  }

  /// Value to analyze
  Value *ArgVal = nullptr;
};

/// ------------------------------------------------------------------- ///
/// - - - - - - - - - - - - -  AAEDTDataBlockInfo - - - - - - - - - - - ///
/// ------------------------------------------------------------------- ///
/// The EDTDataBlock info abstract attribute, basically, what can we say
/// about a DataBlock with regards to the EDTDataBlockInfoState.
struct AAEDTDataBlockInfoVal : AAEDTDataBlockInfo {
  AAEDTDataBlockInfoVal(const IRPosition &IRP, Attributor &A)
      : AAEDTDataBlockInfo(IRP, A) {}

  /// See AbstractAttribute::initialize(...).
  void initialize(Attributor &A) override {
    DBVal = &getAssociatedValue();

    /// Set EDT DataBlock info
    CARTSCache = &static_cast<CARTSInfoCache &>(A.getInfoCache());
    ParentEDT = CARTSCache->getEDT(getAnchorScope());

    /// NOTE: Later on, create EDT here based on the mode.
    /// ReadWrite if the values is a dependency
    /// ReadOnly if the value is a parameter
    LLVM_DEBUG(dbgs() << "[AAEDTDataBlockInfoVal::initialize] Value #" << *DBVal
                      << "\n");
  }

  ChangeStatus updateLocalReachedEDTs(Attributor &A) {
    if (ReachedLocalEDTs.isAtFixpoint())
      return ChangeStatus::UNCHANGED;

    /// Run AAPointerInfo on the value
    const AAPointerInfo *PI = A.getAAFor<AAPointerInfo>(
        *this, IRPosition::value(*DBVal), DepClassTy::OPTIONAL);
    if (!PI || !PI->getState().isValidState()) {
      LLVM_DEBUG(dbgs() << "   - AAPointerInfo Failed or is invalid!\n");
      return indicatePessimisticFixpoint();
    }
    if (!PI->getState().isAtFixpoint()) {
      LLVM_DEBUG(dbgs() << "   - AAPointerInfo is not at fixpoint!\n");
      return ChangeStatus::UNCHANGED;
    }

    for (auto PIItr = PI->begin(); PIItr != PI->end(); ++PIItr) {
      const auto &It = PI->begin();
      // const AA::RangeTy &Range = It->first;
      const SmallSet<unsigned, 4> &AccessIndex = It->second;
      /// Analyze all accesses
      for (auto AI : AccessIndex) {
        const AAPointerInfo::Access &Acc = PI->getAccess(AI);
        Instruction *LocalInst = Acc.getLocalInst();
        /// Analyze dependencies on Call Instruction
        CallBase *CBTo = dyn_cast<CallBase>(LocalInst);
        if (!CBTo)
          continue;
        /// Check if the CallInst is a call to an EDT
        EDT *ToEDT = CARTSCache->getEDT(CBTo->getCalledFunction());
        if (!ToEDT)
          continue;

        if (ReachedLocalEDTs.insert(ToEDT)) {
          Instruction *RemoteInst = Acc.getRemoteInst();
          RemoteValues.insert(RemoteInst);
          LLVM_DEBUG(dbgs() << "   - ReachedLocalEDTs: EDT #" << ToEDT->getID()
                            << "\n");
        }
      }
    }

    /// If we got to this point, we know all the ReachedEDTs of the value
    ReachedLocalEDTs.indicateOptimisticFixpoint();
    return ChangeStatus::CHANGED;
  }

  ChangeStatus updateRemoteReachedEDTs(Attributor &A) {
    if (!ReachedLocalEDTs.isAtFixpoint())
      return ChangeStatus::UNCHANGED;

    /// Run AAEDTDataBlockInfoVal on the RemoteValue, and clamp the state
    bool AllRemoteValuesWereFixed = true;
    for (auto *RemoteValue : RemoteValues) {
      /// Get the value associated with the argument. In case of load/store, get
      /// the pointer operand.
      Value *Val = CARTSCache->getPointerVal(RemoteValue);
      /// NOTE: Check if later on is require to get the UnderlyingObject
      auto *RemoteValueDBInfoAA = A.getAAFor<AAEDTDataBlockInfo>(
          *this, IRPosition::value(*Val), DepClassTy::OPTIONAL);
      if (!RemoteValueDBInfoAA) {
        LLVM_DEBUG(dbgs() << "   - Failed to get AAEDTDataBlockInfo for value: "
                          << *RemoteValue << "\n");
        return indicatePessimisticFixpoint();
      }

      /// Is it a fixpoint?
      AllRemoteValuesWereFixed &=
          RemoteValueDBInfoAA->isReachabilityInfoAtFixpoint();
      /// Clamp the state
      ReachedRemoteEDTs ^= RemoteValueDBInfoAA->ReachedLocalEDTs;
      ReachedRemoteEDTs ^= RemoteValueDBInfoAA->ReachedRemoteEDTs;
    }

    /// If all the RemoteValues are fixed, we can indicate optimistic fixpoint
    if (AllRemoteValuesWereFixed)
      ReachedRemoteEDTs.indicateOptimisticFixpoint();
    return ChangeStatus::CHANGED;
  }

  ChangeStatus updateReachedEDTs(Attributor &A) {
    ChangeStatus Changed = ChangeStatus::UNCHANGED;
    Changed |= updateLocalReachedEDTs(A);
    Changed |= updateRemoteReachedEDTs(A);
    return Changed;
  }

  /// See AbstractAttribute::updateImpl(Attributor &A).
  ChangeStatus updateImpl(Attributor &A) override {
    LLVM_DEBUG(dbgs() << "[AAEDTDataBlockInfoVal::updateImpl] "
                      << getAssociatedValue() << "\n");
    ChangeStatus Changed = ChangeStatus::UNCHANGED;
    Changed |= updateReachedEDTs(A);

    if (isReachabilityInfoAtFixpoint()) {
      indicateOptimisticFixpoint();
      return ChangeStatus::CHANGED;
    }
    return Changed;
  }

  /// See AbstractAttribute::manifest(Attributor &A).
  ChangeStatus manifest(Attributor &A) override {
    /// Debug output info
    LLVM_DEBUG(dbgs() << "AAEDTInfoFunction::manifest: " << *DBVal << "\n"
                      << getAsStr(&A) << "\n");
    ChangeStatus Changed = ChangeStatus::UNCHANGED;
    return Changed;
  }

private:
  SetVector<Value *> RemoteValues;
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
///                               Updates                               ///
/// ------------------------------------------------------------------- ///
EDTUpdateInfo::~EDTUpdateInfo() {
  if (EDTUpdate)
    delete EDTUpdate;
}

void CARTSInfoCache::pushNewParentEDT(EDT *ToUpdateEDT, EDT *ParentEDT) {
  EDTInfoState *Info = new EDTInfoState();
  Info->ParentEDT = ParentEDT;
  EDTUpdateInfo *Update = new EDTUpdateInfo(EDTUpdateInfo::NewParentEDT, Info);
  pushUpdate(ToUpdateEDT, Update);
}

void CARTSInfoCache::pushNewDependentEDT(EDT *ToUpdateEDT, EDT *DependentEDT) {
  EDTInfoState *Info = new EDTInfoState();
  Info->DependentEDTs.insert(DependentEDT);
  EDTUpdateInfo *Update =
      new EDTUpdateInfo(EDTUpdateInfo::NewDependentEDT, Info);
  pushUpdate(ToUpdateEDT, Update);
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
    EDTGraph CARTSGraph;
    computeGraph(M, AM, CARTSGraph);
    /// 
    generateCode(M, CARTSGraph);
    LLVM_DEBUG(dbgs() << "\n"
                      << "-------------------------------------------------\n");
    LLVM_DEBUG(dbgs() << TAG << "Process has finished\n");
    LLVM_DEBUG(dbgs() << "\n" << M << "\n");
    LLVM_DEBUG(dbgs() << "\n"
                      << "-------------------------------------------------\n");
    return PreservedAnalyses::all();
  }

  void computeGraph(Module &M, ModuleAnalysisManager &AM, EDTGraph &Graph) {
    /// Get all functions and EDTs of the module
    LLVM_DEBUG(dbgs() << TAG << "Creating and Initializing EDTs: \n");
    EDTSet EDTs;
    SetVector<Function *> Functions;
    DenseMap<EDTFunction *, EDT *> FunctionEDTMap;
    for (Function &Fn : M) {
      if (Fn.isDeclaration() && !Fn.hasLocalLinkage())
        continue;
      Functions.insert(&Fn);
      if (EDT *CurrentEDT = EDT::get(&Fn)) {
        EDTs.insert(CurrentEDT);
        FunctionEDTMap[&Fn] = CurrentEDT;
      }
    }

    LLVM_DEBUG(dbgs() << "\n\n[Attributor] Initializing AAEDTInfo: \n");
    /// Create attributor
    FunctionAnalysisManager &FAM =
        AM.getResult<FunctionAnalysisManagerModuleProxy>(M).getManager();
    AnalysisGetter AG(FAM);
    CallGraphUpdater CGUpdater;
    BumpPtrAllocator Allocator;
    CARTSInfoCache InfoCache(M, AG, Allocator, Functions, EDTs, FunctionEDTMap,
                             Graph);
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
    Graph.print();
  }

  void generateCode(Module &M, EDTGraph &CARTSGraph) {
    ARTSCodegen CG(M);
    auto EDTNodes = CARTSGraph.getNodes();
    for (EDTGraphNode *EDTNode : EDTNodes) {
      EDT &CurrentEDT = *EDTNode->getEDT();
      // CG.insertEDTFn(CurrentEDT);
      CG.getOrCreateEDTFunction(CurrentEDT);
      CG.insertEDTEntry(CurrentEDT);
      // switch (CurrentEDT.getTy()) {
      // case EDTType::Task:
      //   CG.generateTaskEDT(CurrentEDT);
      //   break;
      // case EDTType::Sync:
      //   CG.generateSyncEDT(CurrentEDT);
      //   break;
      // case EDTType::Parallel:
      //   CG.generateParallelEDT(CurrentEDT);
      //   break;
      // default:
      //   llvm_unreachable("EDT Type not supported!");
      // }

      // LLVM_DEBUG(dbgs() << "- EDT #" << E->getID() << " - \"" << E->getName()
      //                   << "\"\n");
      // LLVM_DEBUG(dbgs() << "  - Type: " << toString(E->getTy()) << "\n");
      // /// Data environment
      // LLVM_DEBUG(dbgs() << "  - Data Environment:\n");
      // auto &DE = E->getDataEnv();
      // LLVM_DEBUG(dbgs() << "    - " << "Number of ParamV = " <<
      // DE.getParamC()
      //                   << "\n");
      // for (auto &P : DE.ParamV) {
      //   LLVM_DEBUG(dbgs() << "      - " << *P << "\n");
      // }
      // LLVM_DEBUG(dbgs() << "    - " << "Number of DepV = " << DE.getDepC()
      //                   << "\n");
      // for (auto &D : DE.DepV) {
      //   LLVM_DEBUG(dbgs() << "      - " << *D << "\n");
      // }
      /// Dependencies
      // LLVM_DEBUG(dbgs() << "  - Incoming Edges:\n");
      // auto InEdges = CARTSGraph.getIncomingEdges(EDTNode);
      // if (InEdges.size() == 0) {
      //   LLVM_DEBUG(dbgs() << "    - The EDT has no incoming edges\n");
      // } else {
      //   /// Print the incoming edges.
      //   for (auto *DepEdge : InEdges) {
      //     auto *From = DepEdge->getFrom();
      //     auto *FromE = From->getEDT();
      //     // LLVM_DEBUG(dbgs() << "    - [");
      //     // if (DepEdge->isDataEdge()) {
      //     //   LLVM_DEBUG(dbgs() << "data");
      //     // } else if (DepEdge->isControlEdge()) {
      //     //   LLVM_DEBUG(dbgs() << "control");
      //     // }
      //     // if (DepEdge->hasCreationDep()) {
      //     //   LLVM_DEBUG(dbgs() << "/ creation");
      //     // }
      //     // LLVM_DEBUG(dbgs() << "] \"EDT #" << FromE->getID() << "\"\n");
      //   }
      // }

      // LLVM_DEBUG(dbgs() << "  - Outgoing Edges:\n");
      // auto OutEdges = CARTSGraph.getOutgoingEdges(EDTNode);
      // if (OutEdges.size() == 0) {
      //   LLVM_DEBUG(dbgs() << "    - The EDT has no outgoing edges\n");
      // } else {
      //   /// Print the outgoing edges.
      //   for (auto *DepEdge : OutEdges) {
      //     auto *To = DepEdge->getTo();
      //     auto *ToE = To->getEDT();
      //     // LLVM_DEBUG(dbgs() << "    - [");
      //     // if (DepEdge->isDataEdge()) {
      //     //   LLVM_DEBUG(dbgs() << "data");
      //     // } else if (DepEdge->isControlEdge()) {
      //     //   LLVM_DEBUG(dbgs() << "control");
      //     // }
      //     // if (DepEdge->hasCreationDep()) {
      //     //   LLVM_DEBUG(dbgs() << "/ creation");
      //     // }
      //     // LLVM_DEBUG(dbgs() << "] \"EDT #" << ToE->getID() << "\"\n");
      //     // if (DepEdge->isDataEdge()) {
      //     //   auto *DataEdge = cast<EDTGraphDataEdge>(DepEdge);
      //     //   auto Values = DataEdge->getValues();
      //     //   for (auto *V : Values) {
      //     //     LLVM_DEBUG(dbgs() << "        - " << *V << "\n");
      //     //   }
      //     // }
      //   }
      // }
      // LLVM_DEBUG(dbgs() << "\n");
    }
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