// Description: Main file for the ARTS Analysis pass.
#include <cassert>
#include <cstdint>

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
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

#include "carts/analysis/graph/ARTSEdge.h"
#include "carts/analysis/graph/ARTSGraph.h"
#include "carts/codegen/ARTSCodegen.h"
#include "carts/utils/ARTS.h"

#include "carts/utils/ARTSCache.h"
#include "carts/utils/ARTSUtils.h"

using namespace llvm;
using namespace arts;

/// DEBUG
#define DEBUG_TYPE "arts-analysis"
#if !defined(NDEBUG)
static constexpr auto TAG = "[" DEBUG_TYPE "] ";
#endif

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
  /// Pointer to the ARTSCache
  ARTSCache *Cache = nullptr;

  /// EDT Information
  EDT *ContextEDT = nullptr;

  /// Flag to track if we reached a fixpoint.
  bool IsAtFixpoint = false;

  /// ParentSyncEDT
  BooleanStatePtr<EDT> ParentSyncEDT;

  /// The EDTs we know are created by the associated EDT.
  BooleanStateWithPtrSetVector<EDT, /* InsertInvalidates */ false> ChildEDTs;

  /// The EDTs we know are reached from the associated EDT.
  BooleanStateWithPtrSetVector<EDT, /* InsertInvalidates */ false>
      DescendantEDTs;

  /// Insert
  bool insertChildEDT(EDT *ChildEDT) {
    bool Inserted = ChildEDTs.insert(ChildEDT);
    if (Inserted)
      Cache->getGraph().insertCreationEdge(ContextEDT, ChildEDT);
    return Inserted;
  }

  /// Getters
  EDT *getEDT() const { return ContextEDT; }
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
    ParentSyncEDT.indicatePessimisticFixpoint();
    return ChangeStatus::CHANGED;
  }

  /// See AbstractState::indicateOptimisticFixpoint(...)
  ChangeStatus indicateOptimisticFixpoint() override {
    IsAtFixpoint = true;
    ChildEDTs.indicateOptimisticFixpoint();
    DescendantEDTs.indicateOptimisticFixpoint();
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
    return *this;
  }

  EDTInfoState operator&=(const EDTInfoState &KIS) { return (*this ^= KIS); }
  ///}
};

/// ContextEDT
struct AAEDTInfo : public StateWrapper<EDTInfoState, AbstractAttribute> {
  using Base = StateWrapper<EDTInfoState, AbstractAttribute>;
  AAEDTInfo(const IRPosition &IRP, Attributor &A) : Base(IRP) {}

  /// Statistics are tracked as part of manifest for now.
  void trackStatistics() const override {}

  /// See AbstractAttribute::getAsStr()
  const std::string getAsStr(Attributor *) const override {
    if (!isValidState() || !ContextEDT)
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

    std::string EDTStr =
        "\nState for EDT #" + std::to_string(ContextEDT->getID()) + " -> \n";
    EDT *ParentEDT = ContextEDT->getParent();
    if (ParentEDT)
      EDTStr +=
          "     -ParentEDT: EDT #" + std::to_string(ParentEDT->getID()) + "\n";
    return (EDTStr + ParentSyncEDTStr() + ChildEDTStr() + DescendantEDTstr());
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
/// - - - - - - - - - - -  DataBlockInfoState  - - - - - - - - - - - ///
/// ------------------------------------------------------------------- ///
/// All the information we have about an EDT data block
struct AADataBlockInfo;

struct DataBlockInfoState : AbstractState {
  /// Pointer to the ARTSCache
  ARTSCache *Cache = nullptr;
  /// Flag to track if we reached a fixpoint.
  bool IsAtFixpoint = false;
  /// DataBlock
  DataBlock *DB = nullptr;
  /// The EDT that is associated with the context of the data block
  EDT *ContextEDT = nullptr;

  /// Determines if the EDT must signal the DataBlock
  bool MustSignal = false;
  BooleanStatePtr<EDT> SignalEDT;

  /// DataBlock (slot) where the signal is sent. It is only filled
  /// in SyncEDTs
  BooleanStatePtr<DataBlock> SignalDB;

  /// For SyncEDTs
  BooleanStatePtr<EDT> DependentSiblingEDT;

  /// Set of ChildDBs that have signaled to the EDTDone of the associated
  /// ContextEDT of the DataBlock
  BooleanStateWithPtrSetVector<DataBlock, false> SignaledDBs;

  /// Set of ChildDBs that have been signaled to my EDTDone
  BooleanStateWithPtrSetVector<DataBlock, false> SignaledDBsToEDTDone;

  /// What we know about the DependentSiblingEDTs
  BooleanStateWithPtrSetVector<EDT, false> DependentSiblingEDTs;
  SetVector<AA::ValueAndContext> DependentSiblingValAndCtx;

  /// What we know about the DependentChildEDTs
  BooleanStateWithPtrSetVector<EDT, false> DependentChildEDTs;
  SetVector<AA::ValueAndContext> DependentChildValAndCtx;

  /// Map of DataMovements
  DenseMap<EDT *, EDTSet> DataMovements;

  /// Size

  /// Usability

  /// Location

  /// Getters
  DataBlock *getDataBlock() const { return DB; }
  Value *getValue() const { return DB->getValue(); }
  EDT *getContextEDT() const { return ContextEDT; }

  /// Abstract State interface
  ///{
  DataBlockInfoState() {}
  DataBlockInfoState(bool BestState) {
    if (!BestState)
      indicatePessimisticFixpoint();
  }

  ChangeStatus hasChanged(DataBlockInfoState &StateBefore) {
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
    DependentSiblingEDT.indicatePessimisticFixpoint();
    SignalDB.indicatePessimisticFixpoint();
    SignaledDBs.indicatePessimisticFixpoint();
    SignaledDBsToEDTDone.indicatePessimisticFixpoint();
    DependentSiblingEDTs.indicatePessimisticFixpoint();
    DependentChildEDTs.indicatePessimisticFixpoint();
    return ChangeStatus::CHANGED;
  }

  /// See AbstractState::indicateOptimisticFixpoint(...)
  ChangeStatus indicateOptimisticFixpoint() override {
    IsAtFixpoint = true;
    SignalEDT.indicateOptimisticFixpoint();
    DependentSiblingEDT.indicateOptimisticFixpoint();
    SignaledDBs.indicateOptimisticFixpoint();
    SignaledDBsToEDTDone.indicateOptimisticFixpoint();
    DependentSiblingEDTs.indicateOptimisticFixpoint();
    DependentChildEDTs.indicateOptimisticFixpoint();
    return ChangeStatus::UNCHANGED;
  }

  /// Return the assumed state
  DataBlockInfoState &getAssumed() { return *this; }
  const DataBlockInfoState &getAssumed() const { return *this; }

  bool operator==(const DataBlockInfoState &RHS) const {
    if (DependentChildEDTs != RHS.DependentChildEDTs)
      return false;
    if (DependentSiblingEDTs != RHS.DependentSiblingEDTs)
      return false;
    return true;
  }

  /// Return empty set as the best state of potential values.
  static DataBlockInfoState getBestState() { return DataBlockInfoState(true); }

  static DataBlockInfoState getBestState(DataBlockInfoState &KIS) {
    return getBestState();
  }

  /// Return full set as the worst state of potential values.
  static DataBlockInfoState getWorstState() {
    return DataBlockInfoState(false);
  }
  ///}
};

/// DataBlockInfo
struct AADataBlockInfo
    : public StateWrapper<DataBlockInfoState, AbstractAttribute> {
  using Base = StateWrapper<DataBlockInfoState, AbstractAttribute>;
  AADataBlockInfo(const IRPosition &IRP, Attributor &A) : Base(IRP) {}

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

    std::string DBStr = "DataBlock ->\n";
    assert(DB && "DataBlock is null!");
    /// Context EDT
    DBStr += "     - Context: EDT #" + std::to_string(ContextEDT->getID()) +
             " / Slot " + std::to_string(DB->getSlot()) + "\n";
    /// SignalEDT
    if (SignalEDT.get())
      DBStr += "     - SignalEDT: EDT #" +
               std::to_string(SignalEDT.get()->getID()) + "\n";
    /// Parent
    if (DB->getParent())
      DBStr += "     - ParentCtx: EDT #" +
               std::to_string(DB->getParent()->getContextEDT()->getID()) +
               " / Slot " + std::to_string(DB->getParent()->getSlot()) + "\n";
    /// ParentSync
    if (DB->getParentSync())
      DBStr += "     - ParentSync: EDT #" +
               std::to_string(DB->getParentSync()->getContextEDT()->getID()) +
               " / Slot " + std::to_string(DB->getParentSync()->getSlot()) +
               "\n";
    /// ChildDBs
    auto &ChildDBs = DB->getChildDBs();
    if (!ChildDBs.empty()) {
      DBStr += "     - ChildDBss: " + std::to_string(ChildDBs.size()) + "\n";
      for (auto *ChildDB : ChildDBs) {
        DBStr += "      - EDT #" +
                 std::to_string(ChildDB->getContextEDT()->getID()) +
                 " / Slot " + std::to_string(ChildDB->getSlot()) + "\n";
      }
    }
    /// Dependency info
    if (DependentSiblingEDT.get() && SignalDB.get())
      DBStr += "     - DependentSiblingEDT: EDT #" +
               std::to_string(DependentSiblingEDT.get()->getID()) +
               " / Slot #" + std::to_string(SignalDB.get()->getSlot()) + "\n";
    return (DBStr + DependentChildEDTsStr() + DependentSiblingEDTsStr());
  }

  /// Create an abstract attribute biew for the position \p IRP.
  static AADataBlockInfo &createForPosition(const IRPosition &IRP,
                                            Attributor &A);

  /// See AbstractAttribute::getName()
  const std::string getName() const override { return "AADataBlockInfo"; }

  /// See AbstractAttribute::getIdAddr()
  const char *getIdAddr() const override { return &ID; }

  /// This function should return true if the type of the \p AA is
  /// AADataBlockInfo
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
    Cache = &static_cast<ARTSCache &>(A.getInfoCache());
    EDTFunction *EDTFn = getAnchorScope();
    ContextEDT = Cache->getOrCreateEDT(EDTFn);
    if (!ContextEDT) {
      LLVM_DEBUG(dbgs() << "[AAEDTInfoFunction::initialize] No context EDT "
                           "for the function.\n");
      indicatePessimisticFixpoint();
      return;
    }

    LLVM_DEBUG(dbgs() << "\n[AAEDTInfoFunction::initialize] EDT #"
                      << ContextEDT->getID() << " for function \""
                      << EDTFn->getName() << "\"\n");
    Cache->insertGraphNode(ContextEDT);

    EDTCallBase *EDTCall = nullptr;

    /// For now, we only support EDTs with a single call
    assert(EDTFn->hasOneUse() && "EDTFunction should have a single use!");
    EDTCall = dyn_cast<EDTCallBase>(*EDTFn->users().begin());
    if (!EDTCall) {
      LLVM_DEBUG(dbgs() << "   - EDT #" << ContextEDT->getID()
                        << " is dead. It can be safely removed\n");
      indicatePessimisticFixpoint();
      return;
    }

    LLVM_DEBUG(dbgs() << "   - Call to EDTFunction:\n    " << *EDTCall << "\n");
    EDT *CalledEDT = Cache->getOrCreateEDT(EDTCall);
    assert(CalledEDT == ContextEDT &&
           "EDTCall doesn't correspond to the ContextEDT of the function!");
    ContextEDT->setCall(EDTCall);

    /// Set ParentEDT - Is the EDT called from another EDT?
    EDT *ParentEDT = Cache->getOrCreateEDT(EDTCall->getCaller());
    if (!ParentEDT) {
      /// The only EDT without a ParentEDT is the MainEDT
      assert(ContextEDT->isMain() && "ParentEDT is null!");
      LLVM_DEBUG(dbgs() << "   - The ContextEDT is the MainEDT\n");
      ParentSyncEDT.indicateOptimisticFixpoint();
      return;
    }
    /// Assumption: All the EDTs are called from another EDT
    assert(ParentEDT && "EDT is not called from another EDT");
    ContextEDT->setParent(ParentEDT);

    /// AsyncEDTs do not have a SiblingDoneEDT
    if (ContextEDT->isAsync()) {
      /// If the parent is the MainEDT, we don't need a ParentSyncEDT
      if (ParentEDT->isMain())
        ParentSyncEDT.indicateOptimisticFixpoint();
      return;
    }

    /// If it is a SyncEDT, it must have a SiblingDoneEDT
    /// It must be the first instruction of the next BB.
    BranchInst *BI =
        dyn_cast<BranchInst>(EDTCall->getParent()->getTerminator());
    assert((BI && BI->getNumSuccessors() == 1) &&
           "EDTCall is not a BranchInst with a single successor!");
    CallBase *CB = dyn_cast<CallBase>(&BI->getSuccessor(0)->front());
    assert(CB && "Next instruction is not a CallBase!");
    EDT *DoneEDT = Cache->getOrCreateEDT(CB);
    assert(
        DoneEDT &&
        "DoneEDT is null! It should have been found in the next BasicBlock!");
    LLVM_DEBUG(dbgs() << "   - DoneEDT: EDT #" << DoneEDT->getID() << "\n");
    ContextEDT->setDoneSync(DoneEDT);

    /// Sync EDTs do not have a ParentSyncEDT
    ParentSyncEDT.indicateOptimisticFixpoint();
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
      EDT *CalledEDT = Cache->getEDT(&EDTCall);
      if (!CalledEDT)
        return true;

      LLVM_DEBUG(dbgs() << "   - EDT #" << CalledEDT->getID()
                        << " is a child of EDT #" << ContextEDT->getID()
                        << "\n");
      insertChildEDT(CalledEDT);

      /// Run AAEDTInfo on the ChildEDT
      auto *ChildEDTAA = A.getOrCreateAAFor<AAEDTInfo>(
          IRPosition::function(*CalledEDT->getFn()), this, DepClassTy::OPTIONAL,
          false, false);
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

    /// If we used assumed information in the check, we cannot be sure
    if (UsedAssumedInformationInCheckCallInst)
      return false;

    /// If we got to this point, we know that all children were created
    ChildEDTs.indicateOptimisticFixpoint();
    /// Indicate Optimistic Fixpoint if all the DescendantEDTs are fixed
    if (!AllReachedEDTsWereFixed)
      return false;
    LLVM_DEBUG(dbgs() << "   - All ReachedEDTs are fixed for EDT #"
                      << ContextEDT->getID() << "\n");
    DescendantEDTs.indicateOptimisticFixpoint();
    return true;
  }

  /// Recursively check the ancestors of the ParentSyncEDTInfo to find the
  /// ParentSyncEDT
  EDT *getParentSyncEDTInfo(EDT *ParentSyncEDTInfo, uint32_t Depth = 0) {
    /// The ParentSyncEDT had to eventually created the current EDT.
    /// Analyze the ParentSyncEDTInfo and its ancestors to find the
    /// ParentSyncEDT
    if (!ParentSyncEDTInfo)
      return nullptr;

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
                      << ContextEDT->getID() << "\n";);
    EDTInfoState StateBefore = getState();

    /// Wait until the reachability info is at a fixpoint
    if (!updateChildEDTs(A))
      return hasChanged(StateBefore);

    /// All EDTs must have an EDTCall
    EDTCallBase *EDTCall = ContextEDT->getCall();
    assert(EDTCall && "EDTCall is null!");

    /// Set the ParentSyncEDT if it is not set
    if (!ParentSyncEDT.isAtFixpoint()) {
      LLVM_DEBUG(dbgs() << "   - Getting ParentSyncEDT\n");
      if (EDT *ParentSyncEDTInfo =
              getParentSyncEDTInfo(ContextEDT->getParent())) {
        LLVM_DEBUG(dbgs() << "     - ParentSyncEDT: EDT #"
                          << ParentSyncEDTInfo->getID() << "\n");
        ParentSyncEDT.set(ParentSyncEDTInfo);
        ParentSyncEDT.indicateOptimisticFixpoint();
        ContextEDT->setParentSync(ParentSyncEDT.get());
      } else {
        llvm_unreachable("ParentSyncEDT could not be found!");
      }
    }

    /// Run AAEDTInfo on the EDTCall
    // auto *EDTCallAA =
    A.getOrCreateAAFor<AAEDTInfo>(IRPosition::callsite_function(*EDTCall), this,
                                  DepClassTy::OPTIONAL, false, false);
    // getState() *= EDTCallAA->getState();
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
                      << "\n");
    return ChangeStatus::UNCHANGED;
  }
};

/// The EDTCallsite info abstract attribute, basically, what can we now about an
/// EDT from its corresponding callsite.
struct AAEDTInfoCallsite : AAEDTInfo {
  AAEDTInfoCallsite(const IRPosition &IRP, Attributor &A) : AAEDTInfo(IRP, A) {}

  /// See AbstractAttribute::initialize(...).
  void initialize(Attributor &A) override {
    Cache = &static_cast<ARTSCache &>(A.getInfoCache());
    /// Get the Called EDT
    CallBase &EDTCall = cast<CallBase>(getAnchorValue());
    ContextEDT = Cache->getEDT(EDTCall.getCalledFunction());
    assert(ContextEDT && "ContextEDT is null!");
    LLVM_DEBUG(dbgs() << "\n[AAEDTInfoCallsite::initialize] EDT #"
                      << ContextEDT->getID() << "\n");
  }

  /// See AbstractAttribute::updateImpl(Attributor &A).
  ChangeStatus updateImpl(Attributor &A) override {
    LLVM_DEBUG(dbgs() << "\n[AAEDTInfoCallsite::updateImpl] EDT #"
                      << ContextEDT->getID() << "\n");

    CallBase &EDTCall = cast<CallBase>(getAnchorValue());
    /// Run AADataBlockInfo on each argument of the call instruction
    bool AllDBsWereFixed = true;
    const unsigned EDTCallArgSize = EDTCall.data_operands_size();
    for (uint32_t CallArgItr = 0; CallArgItr < EDTCallArgSize; ++CallArgItr) {
      auto *ArgDataBlockAA = A.getOrCreateAAFor<AAEDTInfo>(
          IRPosition::callsite_argument(EDTCall, CallArgItr), this,
          DepClassTy::OPTIONAL, false, false);
      if (!ArgDataBlockAA->isAtFixpoint()) {
        AllDBsWereFixed = false;
        continue;
      }
    }

    if (!AllDBsWereFixed)
      return ChangeStatus::UNCHANGED;

    LLVM_DEBUG(dbgs() << "   - All DataBlocks were fixed for EDT #"
                      << ContextEDT->getID() << "\n");
    indicateOptimisticFixpoint();
    return ChangeStatus::CHANGED;
  }

  /// See AbstractAttribute::manifest(Attributor &A).
  ChangeStatus manifest(Attributor &A) override {
    /// Set Control dependency with DoneEDT
    if (EDT *DoneEDT = ContextEDT->getDoneSync()) {
      // Cache->getGraph().addControlEdge(ContextEDT, DoneEDT);
      /// I need to know the Guid of the DoneEDT from the parent EDT
      Cache->getGraph().insertCreationEdgeGuid(ContextEDT->getParent(),
                                               ContextEDT, DoneEDT);
    }
    return ChangeStatus::UNCHANGED;
  }
};

struct AAEDTInfoCallsiteArg : AAEDTInfo {
  AAEDTInfoCallsiteArg(const IRPosition &IRP, Attributor &A)
      : AAEDTInfo(IRP, A) {}

  /// See AbstractAttribute::initialize(...).
  void initialize(Attributor &A) override {
    Cache = &static_cast<ARTSCache &>(A.getInfoCache());

    /// Get the called EDT
    CallBase &EDTCall = cast<CallBase>(getAnchorValue());
    ContextEDT = Cache->getEDT(EDTCall.getCalledFunction());
    assert(ContextEDT && "CalledEDT is null!");

    auto CallArgItr = getCallSiteArgNo();
    LLVM_DEBUG(dbgs() << "\n[AAEDTInfoCallsiteArg::initialize] CallArg #"
                      << CallArgItr << " from EDT #" << ContextEDT->getID()
                      << "\n");

    /// Get the value associated with the argument. In case of load/store, get
    /// the pointer operand.
    ArgVal = &getAssociatedValue();
    assert(ArgVal && "ArgVal is null!");

    /// Create DataBlockInfo for the argument
    AA::ValueAndContext VAC(*ArgVal, EDTCall);
    DataBlock *DB = Cache->getOrCreateDataBlock(VAC, CallArgItr);

    /// If the value is a not a dependency of the EDT, add the parameter to the
    /// graph and indicate optimistic fixpoint
    if (!ContextEDT->isDep(CallArgItr)) {
      insertParameterEdge(ContextEDT->getParent(), ContextEDT, ArgVal);
      indicateOptimisticFixpoint();
    } else {
      /// Each dependency has associated a DepSlot
      uint32_t InputSlot = ContextEDT->insertDepSlot(CallArgItr);
      DB->setSlot(InputSlot);
      LLVM_DEBUG(dbgs() << "   - Creating DepSlot #" << InputSlot
                        << " for value: " << *ArgVal << "\n");
      /// and it has to be sent from the parent EDT to the called EDT unless it
      /// is a DoneEDT (because all its input dependencies of a Done EDT must be
      /// sent by the sibling SyncEDT)
      if (!ContextEDT->isDoneEDT()) {
        insertDataBlockGraphEdge(ContextEDT->getParent(), ContextEDT, InputSlot,
                                 DB);
      }
    }
  }

  /// See AbstractAttribute::updateImpl(Attributor &A).
  ChangeStatus updateImpl(Attributor &A) override {
    EDT *CalledEDT = getEDT();
    LLVM_DEBUG(dbgs() << "\n[AAEDTInfoCallsiteArg::updateImpl] "
                      << getAssociatedValue() << " from EDT #"
                      << CalledEDT->getID() << "\n");
    /// Run AADataBlockInfo on the associated value
    CallBase &EDTCall = cast<CallBase>(getAnchorValue());

    auto *ArgValDBInfoAA = A.getOrCreateAAFor<AADataBlockInfo>(
        IRPosition::callsite_argument(EDTCall, getCallSiteArgNo()), this,
        DepClassTy::OPTIONAL, false, false);
    if (!ArgValDBInfoAA || !ArgValDBInfoAA->getState().isValidState()) {
      LLVM_DEBUG(dbgs() << "     - Failed to get AADataBlockInfo for value: "
                        << *ArgVal << "\n");
      return indicatePessimisticFixpoint();
    }

    /// Wait until fixpoint is reached
    if (!ArgValDBInfoAA->isAtFixpoint())
      return ChangeStatus::UNCHANGED;

    /// Add Dependency if any
    DataBlock *SignalDB = ArgValDBInfoAA->SignalDB.get();
    if (!SignalDB) {
      assert(CalledEDT->isAsync() && "SignalDB is null for non AsyncEDTs!");
      return indicateOptimisticFixpoint();
    }

    // If we got to this point, it has to be a SyncEDT
    assert(CalledEDT->isSync() && "SignalDB is only for SyncEDTs!");

    EDT *SignalEDT = CalledEDT->getDoneSync();
    DataBlock *DB = ArgValDBInfoAA->getDataBlock();
    /// If the parent signals the value, add the DBEdge
    if (ArgValDBInfoAA->MustSignal) {
      insertDataBlockGraphEdge(CalledEDT, SignalEDT, SignalDB->getSlot(), DB);
      return indicateOptimisticFixpoint();
    }

    /// Otherwise, check if the value is signaled by a child/descendent EDT
    for (DataBlock *SignaledDB : ArgValDBInfoAA->SignaledDBsToEDTDone) {
      insertDataBlockGraphEdge(SignaledDB->getContextEDT(), SignalEDT,
                               SignalDB->getSlot(), SignaledDB);
    }

    return indicateOptimisticFixpoint();
  }

  void insertDataBlockGraphEdge(EDT *From, EDT *To, uint32_t Slot,
                                DataBlock *DB) {
    Cache->getGraph().insertDataBlockEdge(From, To, Slot, DB);
  }

  void insertParameterEdge(EDT *From, EDT *To, EDTValue *Parameter) {
    Cache->getGraph().insertCreationEdgeParameter(From, To, Parameter);
  }

  /// See AbstractAttribute::manifest(Attributor &A).
  ChangeStatus manifest(Attributor &A) override {
    return ChangeStatus::UNCHANGED;
  }

private:
  /// Value to analyze
  Value *ArgVal = nullptr;
};

/// ------------------------------------------------------------------- ///
/// - - - - - - - - - - - - -  AADataBlockInfo - - - - - - - - - - - ///
/// ------------------------------------------------------------------- ///
/// The DataBlock info abstract attribute, basically, what can we say
/// about a DataBlock with regards to the DataBlockInfoState.
struct AADataBlockInfoCtxAndVal : AADataBlockInfo {
  AADataBlockInfoCtxAndVal(const IRPosition &IRP, Attributor &A)
      : AADataBlockInfo(IRP, A) {}

  /// See AbstractAttribute::initialize(...).
  void initialize(Attributor &A) override {
    /// Set EDT DataBlock info
    Cache = &static_cast<ARTSCache &>(A.getInfoCache());

    /// Create DataBlockInfo for the argument
    CallBase &EDTCall = cast<CallBase>(getAnchorValue());
    Value *ArgVal = &getAssociatedValue();
    AA::ValueAndContext VAC(*ArgVal, EDTCall);
    DB = Cache->getOrCreateDataBlock(VAC, getCallSiteArgNo());

    /// Set ContextEDT
    ContextEDT = Cache->getEDT(&EDTCall);
    LLVM_DEBUG(dbgs() << "\n[AADataBlockInfoCtxAndVal::initialize] "
                      << *DB->getValue() << " from EDT #"
                      << Cache->getEDT(EDTCall.getCalledFunction())->getID()
                      << "\n");
  }

  /// For Sync EDTs, analyze the dependencies with the siblings
  ChangeStatus handleSyncEDTs(Attributor &A) {
    /// If we are at a fixpoint, we are done here
    if (DependentSiblingEDT.isAtFixpoint())
      return ChangeStatus::UNCHANGED;

    /// Otherwise, compute the set of DependentSiblingEDTs
    if (!setDependentSiblingEDTs(A))
      return ChangeStatus::UNCHANGED;

    /// There must be only one sibling EDT
    assert(DependentSiblingEDTs.size() == 1 &&
           "Sync EDTs must have only one DoneEDT!");

    /// Verify that the DoneEDT is the same as the sibling EDT
    assert((ContextEDT->getDoneSync() == DependentSiblingEDTs[0]) &&
           "DoneEDT is not the same as the sibling EDT!");

    /// Set the dependency with the DoneEDT
    DependentSiblingEDT.set(DependentSiblingEDTs[0]);
    DependentSiblingEDT.indicateOptimisticFixpoint();

    /// Set Done
    DataBlock *DoneDB =
        Cache->getOrCreateDataBlock(DependentSiblingValAndCtx[0]);
    SignalDB.set(DoneDB);
    SignalDB.indicateOptimisticFixpoint();

    return ChangeStatus::CHANGED;
  }

  /// For Async EDTs, analyze the dependencies with respect to the
  /// sync parent context
  ChangeStatus handleAsyncEDTs(Attributor &A) {
    /// Compute only if the DependentSiblingEDTs is not at a fixpoint
    if (DependentSiblingEDT.isAtFixpoint())
      return ChangeStatus::UNCHANGED;

    /// If CtxEDT has no children, it must signal the value to the DoneEDT
    AAEDTInfo *CtxEDTAA = A.lookupAAFor<AAEDTInfo>(
        IRPosition::function(*ContextEDT->getFn()), this, DepClassTy::NONE);
    if (CtxEDTAA->ChildEDTs.empty()) {
      MustSignal = true;
      SignalEDT.set(ContextEDT->getDoneSync());
      indicateOptimisticFixpoint();
      return ChangeStatus::CHANGED;
    }

    /// AsyncEDTs do not have a DependentSiblingEDT
    DependentSiblingEDT.indicateOptimisticFixpoint();

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

    /// If none of the CtxEDT childEDTs depend on the value, it must signal it
    EDT *EDTDoneSync = ContextEDT->getDoneSync();
    if (DependentChildEDTs.empty()) {
      MustSignal = true;
      SignalEDT.set(EDTDoneSync);
      DependentChildEDTs.indicateOptimisticFixpoint();
      return ChangeStatus::CHANGED;
    }

    /// Otherwise, we have to check if any of the DependentChildEDTs
    /// signals the DB
    bool AllChildEDTsWereFixed = true;
    MustSignal = true;
    for (AA::ValueAndContext ChildVAC : DependentChildValAndCtx) {
      const CallBase &ChildCall = *cast<CallBase>(ChildVAC.getCtxI());
      auto ChildCallArg = Cache->getArgNo(ChildVAC);
      auto *ChildEDTAA = A.lookupAAFor<AADataBlockInfo>(
          IRPosition::callsite_argument(ChildCall, ChildCallArg), this,
          DepClassTy::OPTIONAL);
      assert(ChildEDTAA && "ChildEDTAA is null!");

      if (!ChildEDTAA->isAtFixpoint()) {
        AllChildEDTsWereFixed = false;
        continue;
      }

      /// Add ChildDB
      DataBlock *ChildDB = ChildEDTAA->getDataBlock();
      DB->addChildDB(ChildDB);

      /// If the value is signaled by any ChildEDT, or any of its descendants,
      /// we do not have to signal it. Clamp the set of SignaledChildEDTs
      if (ChildEDTAA->SignalEDT.get() == EDTDoneSync) {
        SignaledDBs.insert(ChildDB);
        SignaledDBsToEDTDone.insert(ChildDB);
        MustSignal = false;
      }
      /// If the value is signaled by any of the descendants, analyze it
      else if (!ChildEDTAA->SignaledDBs.empty()) {
        SignaledDBs ^= ChildEDTAA->SignaledDBs;
        /// Check if any of the SignaledChildEDTs signals the value to my
        /// EDTDoneSync
        for (DataBlock *SignaledChildDB : ChildEDTAA->SignaledDBs) {
          EDT *SignaledContextEDT = SignaledChildDB->getContextEDT();
          if (SignaledContextEDT->getDoneSync() != EDTDoneSync)
            continue;
          MustSignal = false;
          SignaledDBsToEDTDone.insert(SignaledChildDB);
        }
      }
    }

    if (!AllChildEDTsWereFixed)
      return ChangeStatus::UNCHANGED;

    /// If none of the ChildEDTs signal the value, it must signal it
    if (MustSignal)
      SignalEDT.set(EDTDoneSync);

    /// If EDT is synchronous, set ParentDB to the the set of DBs that signal
    /// the value to the DoneEDT
    if (ContextEDT->isSync()) {
      for (DataBlock *SignaledDB : SignaledDBsToEDTDone) {
        assert(!SignaledDB->getParentSync() &&
               "ParentSync of SignaledDB was set before!");
        SignaledDB->setParentSync(DB);
      }
    }

    DependentChildEDTs.indicateOptimisticFixpoint();
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
      const CallBase *CBTo = cast<CallBase>(VAC.getCtxI());
      EDT *ToEDT = Cache->getEDT(CBTo->getCalledFunction());
      if (!ToEDT || (CBTo == &EDTCall))
        continue;

      /// If the CBTo is not reachable from the EDTCall, ignore it.
      /// e.g. EDTCall is after CBTo
      if (CheckReachability &&
          !AA::isPotentiallyReachable(A, EDTCall, *CBTo, *this))
        continue;

      /// If the value is not a dependency, continue
      if (!Cache->isEDTDep(VAC))
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
    EDTValue &PointerValue = *Cache->getPointerVal(&EDTVal);
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
    Value &ArgVal = *EDTCall.getCalledFunction()->getArg(Cache->getArgNo(VAC));

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
    LLVM_DEBUG(CallBase &EDTCall = cast<CallBase>(getAnchorValue());
               dbgs() << "\n[AADataBlockInfoCtxAndVal::updateImpl] "
                      << getAssociatedValue() << " from EDT #"
                      << Cache->getEDT(EDTCall.getCalledFunction())->getID()
                      << "\n");

    ChangeStatus Changed = ChangeStatus::UNCHANGED;
    Changed |= (ContextEDT->isAsync()) ? handleAsyncEDTs(A) : handleSyncEDTs(A);
    Changed |= handleChildEDTs(A);

    if (DependentChildEDTs.isAtFixpoint() &&
        DependentSiblingEDTs.isAtFixpoint())
      indicateOptimisticFixpoint();
    return Changed;
  }

  /// See AbstractAttribute::manifest(Attributor &A).
  ChangeStatus manifest(Attributor &A) override {
    LLVM_DEBUG(dbgs() << "-------------------------------\n");
    LLVM_DEBUG(dbgs() << "[AADataBlockInfoCtxAndVal::manifest] "
                      << *DB->getValue() << " from EDT #" << ContextEDT->getID()
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
const char AADataBlockInfo::ID = 0;

AAEDTInfo &AAEDTInfo::createForPosition(const IRPosition &IRP, Attributor &A) {
  AAEDTInfo *AA = nullptr;
  switch (IRP.getPositionKind()) {
  case IRPosition::IRP_INVALID:
  case IRPosition::IRP_ARGUMENT:
  case IRPosition::IRP_RETURNED:
  case IRPosition::IRP_CALL_SITE_RETURNED:
  case IRPosition::IRP_FLOAT:
    llvm_unreachable("ContextEDT can only be created for this IR position!");
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

AADataBlockInfo &AADataBlockInfo::createForPosition(const IRPosition &IRP,
                                                    Attributor &A) {
  AADataBlockInfo *AA = nullptr;
  switch (IRP.getPositionKind()) {
  case IRPosition::IRP_INVALID:
  case IRPosition::IRP_RETURNED:
  case IRPosition::IRP_CALL_SITE_RETURNED:
  case IRPosition::IRP_CALL_SITE:
  case IRPosition::IRP_FUNCTION:
  case IRPosition::IRP_ARGUMENT:
  case IRPosition::IRP_FLOAT:
    llvm_unreachable(
        "DataBlockInfo can only be created for floating position!");
  case IRPosition::IRP_CALL_SITE_ARGUMENT:
    AA = new (A.Allocator) AADataBlockInfoCtxAndVal(IRP, A);
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
    ARTSCache Cache(M, AG, Allocator, Functions);

    computeGraph(M, Cache);
    generateCode(M, Cache);
    LLVM_DEBUG(dbgs() << "\n"
                      << "-------------------------------------------------\n");
    LLVM_DEBUG(dbgs() << TAG << "Process has finished\n");
    LLVM_DEBUG(dbgs() << "\n" << M << "\n");
    LLVM_DEBUG(dbgs() << "\n"
                      << "-------------------------------------------------\n");
    return PreservedAnalyses::all();
  }

  void computeGraph(Module &M, ARTSCache &Cache) {
    LLVM_DEBUG(dbgs() << "- - - - - - - - - - - - - - - - - - - - - - - - \n");
    LLVM_DEBUG(dbgs() << TAG << "Computing Graph\n");
    LLVM_DEBUG(dbgs() << "[Attributor] Initializing AAEDTInfo: \n");
    /// Create attributor
    SetVector<Function *> &Functions = Cache.getFunctions();
    CallGraphUpdater CGUpdater;
    AttributorConfig AC(CGUpdater);
    AC.DefaultInitializeLiveInternals = false;
    AC.IsModulePass = true;
    AC.RewriteSignatures = false;
    AC.MaxFixpointIterations = 32;
    AC.DeleteFns = false;
    AC.PassName = DEBUG_TYPE;
    Attributor A(Functions, Cache, AC);

    /// Register AAs
    for (EDT *E : Cache.getEDTs()) {
      A.getOrCreateAAFor<AAEDTInfo>(IRPosition::function(*E->getFn()),
                                    /* QueryingAA */ nullptr, DepClassTy::NONE,
                                    /* ForceUpdate */ false,
                                    /* UpdateAfterInit */ false);
    }
    ChangeStatus Changed = A.run();
    LLVM_DEBUG(dbgs() << "[Attributor] Done with " << Functions.size()
                      << " functions, result: " << Changed << ".\n");
    Cache.getGraph().print();
  }

  void generateCode(Module &M, ARTSCache &Cache) {
    ARTSCodegen &CG = Cache.getCG();
    ARTSGraph &Graph = Cache.getGraph();
    /// Reserve GUIDs for all EDTs
    Graph.forEachEDTNode([&](EDTGraphNode *EDTNode) {
      EDT &CurrentEDT = *EDTNode->getEDT();
      CG.getOrCreateEDTFunction(CurrentEDT);
      CG.getOrCreateEDTGuid(CurrentEDT);
    });
    LLVM_DEBUG(dbgs() << "\nAll EDT Guids have been reserved\n");

    /// Insert EDT Entry and Call iterating from parent to child EDTs to make
    /// sure all GUIDs are in the EDTEntry of the EDT Function
    EDTGraphNode *EntryEDTNode = Graph.getEntryNode();
    assert(EntryEDTNode && "EntryEDTNode is null!");
    SetVector<EDTGraphNode *> WorkList;
    WorkList.insert(EntryEDTNode);
    while (!WorkList.empty()) {
      EDTGraphNode *CurrentEDTNode = WorkList.pop_back_val();
      EDT &CurrentEDT = *CurrentEDTNode->getEDT();
      LLVM_DEBUG(dbgs() << "\nGenerating Code for EDT #" << CurrentEDT.getID()
                        << "\n");
      CG.insertEDTEntry(CurrentEDT);
      CG.insertEDTCall(CurrentEDT);
      /// Add all the children to the WorkList
      Graph.forEachOutgoingCreationEdge(
          CurrentEDTNode,
          [&](CreationGraphEdge *Edge) { WorkList.insert(Edge->getTo()); });
    }
    LLVM_DEBUG(dbgs() << "\nAll EDT Entries and Calls have been inserted\n");

    /// Signal EDTs
    LLVM_DEBUG(dbgs() << "\nInserting EDT Signals\n");
    Graph.forEachEDTNode([&](EDTGraphNode *EDTNode) {
      EDT &CurrentEDT = *EDTNode->getEDT();
      CG.insertEDTSignals(CurrentEDT);
    });

    /// Insert init functions
    CG.insertInitFunctions();
    CG.insertARTSShutdownFn();
    /// Debug Module
    // LLVM_DEBUG(dbgs() << "\n" << M << "\n");
    /// Remove values
    CG.rewireValues();
    utils::removeValues();
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