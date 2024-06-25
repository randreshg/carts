// Description: Main file for the ARTS Analysis pass.
#include "llvm/Analysis/CallGraph.h"
#include "llvm/Analysis/MemorySSA.h"
#include "llvm/Analysis/ValueTracking.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Pass.h"
#include "llvm/Support/Debug.h"
#include "llvm/Transforms/IPO/Attributor.h"
#include "llvm/Transforms/Utils/CallGraphUpdater.h"
#include <cassert>
#include <cstdint>

#include "carts/analysis/ARTSAnalysisPass.h"
#include "carts/analysis/graph/EDTGraph.h"
#include "carts/utils/ARTS.h"
#include "noelle/core/Noelle.hpp"

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

struct EDTInfoState : AbstractState {
  /// EDT Node
  EDT *EDTNode = nullptr;

  /// Flag to track if we reached a fixpoint.
  bool IsAtFixpoint = false;

  /// The EDTS we know are reached from the associated EDT.
  BooleanStateWithPtrSetVector<EDT, /* InsertInvalidates */ false> ReachedEDTs;

  /// The EDTs we know are created by the associated EDT.
  BooleanStateWithPtrSetVector<EDT, /* InsertInvalidates */ false> ChildEDTs;

  EDT *getEDT() const { return EDTNode; }

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
    ReachedEDTs.indicatePessimisticFixpoint();
    ChildEDTs.indicatePessimisticFixpoint();
    return ChangeStatus::CHANGED;
  }

  /// See AbstractState::indicateOptimisticFixpoint(...)
  ChangeStatus indicateOptimisticFixpoint() override {
    IsAtFixpoint = true;
    ReachedEDTs.indicateOptimisticFixpoint();
    ChildEDTs.indicateOptimisticFixpoint();
    return ChangeStatus::UNCHANGED;
  }

  /// Return the assumed state
  EDTInfoState &getAssumed() { return *this; }
  const EDTInfoState &getAssumed() const { return *this; }

  bool operator==(const EDTInfoState &RHS) const {
    if (ReachedEDTs != RHS.ReachedEDTs)
      return false;
    if (ChildEDTs != RHS.ChildEDTs)
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
    ReachedEDTs ^= KIS.ReachedEDTs;
    return *this;
  }

  EDTInfoState operator&=(const EDTInfoState &KIS) { return (*this ^= KIS); }

  ///}
};

struct AAEDTInfo : public StateWrapper<EDTInfoState, AbstractAttribute> {
  using Base = StateWrapper<EDTInfoState, AbstractAttribute>;
  AAEDTInfo(const IRPosition &IRP, Attributor &A) : Base(IRP) {}

  /// Statistics are tracked as part of manifest for now.
  void trackStatistics() const override {}

  /// See AbstractAttribute::getAsStr()
  const std::string getAsStr() const override {
    if (!isValidState() || !EDTNode)
      return "<invalid>";
    std::string EDTStr = "EDT #" + std::to_string(EDTNode->getID()) + " -> ";

    auto ReachedEDTStr = [&]() {
      if (!ReachedEDTs.isValidState())
        return "<invalid> with " + std::to_string(ReachedEDTs.size()) + " EDTs";
      std::string Str;
      for (EDT *E : ReachedEDTs)
        Str += (Str.empty() ? "" : ", ") + std::to_string(E->getID());
      Str = "#Reached EDTs: " + std::to_string(ReachedEDTs.size()) + "{" + Str +
            "}";
      return Str;
    };

    auto ChildEDTStr = [&]() {
      if (!ChildEDTs.isValidState())
        return "<invalid> with " + std::to_string(ChildEDTs.size()) + " EDTs";
      std::string Str;
      for (EDT *E : ChildEDTs)
        Str += (Str.empty() ? "" : ", ") + std::to_string(E->getID());
      Str =
          "#Child EDTs: " + std::to_string(ChildEDTs.size()) + "{" + Str + "}";
      return Str;
    };

    return (EDTStr + ReachedEDTStr() + ", " + ChildEDTStr());
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

/// EDTInfoCache
struct EDTInfoCache : public InformationCache {
  EDTInfoCache(Module &M, AnalysisGetter &AG, BumpPtrAllocator &Allocator,
               SetVector<Function *> &Functions, EDTSet &EDTs,
               DenseMap<EDTFunction, EDT *> &FunctionEDTMap)
      : InformationCache(M, AG, Allocator, &Functions), EDTs(EDTs),
        FunctionEDTMap(FunctionEDTMap) {}

  void insertEDT(Value *V, EDT *E) { Values[V].insert(E); }

  bool isValueInEDT(Value *V, EDT *E) {
    auto Itr = Values.find(V);
    if (Itr == Values.end())
      return false;
    return Itr->second.count(E);
  }

  SetVector<EDT *> getEDTs(Value *V) const {
    auto Itr = Values.find(V);
    if (Itr == Values.end())
      return SetVector<EDT *>();
    return Itr->second;
  }

  EDT *getEDT(Function *F) const {
    auto Itr = FunctionEDTMap.find(F);
    if (Itr == FunctionEDTMap.end())
      return nullptr;
    return Itr->second;
  }

  /// Collection of known EDTs in the module
  EDTSet &EDTs;
  DenseMap<EDTFunction, EDT *> &FunctionEDTMap;
  EDTGraph *Graph;
  /// Values that may be modified by the EDTs (Calls)
  DenseMap<Value *, SetVector<EDT *>> Values;
};

/// The function kernel info abstract attribute, basically, what can we say
/// about a function with regards to the EDTInfoState.
struct AAEDTInfoFunction : AAEDTInfo {
  AAEDTInfoFunction(const IRPosition &IRP, Attributor &A) : AAEDTInfo(IRP, A) {}

  /// See AbstractAttribute::initialize(...).
  void initialize(Attributor &A) override {
    auto &EDTCache = static_cast<EDTInfoCache &>(A.getInfoCache());
    EDTFunction Fn = getAnchorScope();
    EDTNode = EDTCache.getEDT(Fn);
    if (!EDTNode) {
      indicatePessimisticFixpoint();
      return;
    }

    LLVM_DEBUG(dbgs() << "AAEDTInfoFunction::initialize-> EDT #"
                      << EDTNode->getID() << " for function \"" << Fn->getName()
                      << "\"\n");
  }

  /// Fixpoint iteration update function. Will be called every time a
  /// dependence changed its state (and in the beginning).
  ChangeStatus updateImpl(Attributor &A) override {
    auto &EDTCache = static_cast<EDTInfoCache &>(A.getInfoCache());
    // LLVM_DEBUG(dbgs() << "AAEDTInfoFunction::updateImpl-> EDT #"
    //                   << EDTNode->getID() << "\n";);
    EDTInfoState StateBefore = getState();
    /// Callback to check a call instruction.
    bool AllEDTChildrenWereFixed = true;
    auto CheckCallInst = [&](Instruction &I) {
      auto &CB = cast<CallBase>(I);
      auto *CalledFn = CB.getCalledFunction();
      if (!CalledFn)
        return true;
      auto *CalledEDT = EDTCache.getEDT(CalledFn);
      if (!CalledEDT)
        return true;
      auto &CBAA = A.getAAFor<AAEDTInfo>(
          *this, IRPosition::callsite_function(CB), DepClassTy::OPTIONAL);
      /// Merge the state of the associated function with the state of the
      /// call to child
      // LLVM_DEBUG(dbgs() << "  - Checking call instruction: "
      //                   << CB.getCalledFunction()->getName() << "\n");
      // auto FixPoint = CBAA.isAtFixpoint() ? "true" : "false";
      // LLVM_DEBUG(dbgs() << "    - Fixpoint: " << FixPoint << "\n");
      AllEDTChildrenWereFixed &= CBAA.isAtFixpoint();
      getState() ^= CBAA.getState();

      /// Add child and reached EDTs to the set
      if (auto *Child = CBAA.getEDT()) {
        ChildEDTs.insert(Child);
        ReachedEDTs.insert(Child);
      }
      return true;
    };

    bool UsedAssumedInfoInCheckCallInst = false;
    if (!A.checkForAllCallLikeInstructions(CheckCallInst, *this,
                                           UsedAssumedInfoInCheckCallInst)) {
      LLVM_DEBUG(dbgs() << "AAEDTInfoFunction::updateImpl: "
                        << "Failed to visit all call-like instructions!\n";);
      return indicatePessimisticFixpoint();
    }

    /// If we were able to check all call instructions we can indicate that
    /// we are at a fixpoint with the ChildEDTs.
    ChildEDTs.indicateOptimisticFixpoint();

    // Fix the state if all children were fixed
    if (AllEDTChildrenWereFixed) {
      LLVM_DEBUG(dbgs() << "AAEDTInfoFunction::updateImpl: "
                        << "All children were fixed for EDT #"
                        << EDTNode->getID() << " --> Optimistic FixPoint\n");
      indicateOptimisticFixpoint();
    }

    return StateBefore == getState() ? ChangeStatus::UNCHANGED
                                     : ChangeStatus::CHANGED;
  }

  /// Modify the IR based on the EDTInfoState as the fixpoint iteration is
  /// finished now.
  ChangeStatus manifest(Attributor &A) override {
    /// Debug output info
    LLVM_DEBUG(dbgs() << "AAEDTInfoFunction::manifest: " << getAsStr() << "\n");
    ChangeStatus Changed = ChangeStatus::UNCHANGED;
    return Changed;
  }
};

struct AAEDTInfoChild : AAEDTInfo {
  AAEDTInfoChild(const IRPosition &IRP, Attributor &A) : AAEDTInfo(IRP, A) {}

  /// See AbstractAttribute::initialize(...).
  void initialize(Attributor &A) override {
    auto &EDTCache = static_cast<EDTInfoCache &>(A.getInfoCache());
    /// Get the EDTChildNode
    auto &CB = cast<CallBase>(getAnchorValue());
    auto *CalledFunction = CB.getCalledFunction();
    EDTNode = EDTCache.getEDT(CalledFunction);
    assert(EDTNode && "EDTNode is null!");

    LLVM_DEBUG(dbgs() << "AAEDTInfoChild::initialize: EDT #" << EDTNode->getID()
                      << "\n");
  }

  /// See AbstractAttribute::updateImpl(Attributor &A).
  ChangeStatus updateImpl(Attributor &A) override {
    EDTInfoState StateBefore = getState();
    /// What do we know about the child EDT?
    auto &CB = cast<CallBase>(getAnchorValue());
    auto &CBAA = A.getAAFor<AAEDTInfo>(
        *this, IRPosition::function(*CB.getCalledFunction()),
        DepClassTy::OPTIONAL);
    getState() ^= CBAA.getState();
    if (CBAA.isAtFixpoint())
      indicateOptimisticFixpoint();
    return StateBefore == getState() ? ChangeStatus::UNCHANGED
                                     : ChangeStatus::CHANGED;
  }

  /// See AbstractAttribute::manifest(Attributor &A).
  ChangeStatus manifest(Attributor &A) override {
    return ChangeStatus::UNCHANGED;
  }
};

const char AAEDTInfo::ID = 0;

AAEDTInfo &AAEDTInfo::createForPosition(const IRPosition &IRP, Attributor &A) {
  AAEDTInfo *AA = nullptr;
  switch (IRP.getPositionKind()) {
  case IRPosition::IRP_INVALID:
  case IRPosition::IRP_FLOAT:
  case IRPosition::IRP_ARGUMENT:
  case IRPosition::IRP_RETURNED:
  case IRPosition::IRP_CALL_SITE_RETURNED:
  case IRPosition::IRP_CALL_SITE_ARGUMENT:
    llvm_unreachable("EDTInfo can only be created for function position!");
  case IRPosition::IRP_CALL_SITE:
    AA = new (A.Allocator) AAEDTInfoChild(IRP, A);
    break;
  case IRPosition::IRP_FUNCTION:
    AA = new (A.Allocator) AAEDTInfoFunction(IRP, A);
    break;
  }

  return *AA;
}

/// ------------------------------------------------------------------- ///
///                        ARTSAnalysisPass                             ///
/// ------------------------------------------------------------------- ///

namespace arts {

ARTSAnalysisPass::ARTSAnalysisPass() : ModulePass(ID) {}

bool ARTSAnalysisPass::doInitialization(Module &M) { return false; }

bool ARTSAnalysisPass::runOnModule(Module &M) {
  LLVM_DEBUG(dbgs() << "\n-------------------------------------------------\n");
  LLVM_DEBUG(dbgs() << TAG << "Running ARTS Analysis pass on Module: \n"
                    << M.getName() << "\n");
  LLVM_DEBUG(dbgs() << "-------------------------------------------------\n");
  /// Get all functions and EDTs of the module
  LLVM_DEBUG(dbgs() << TAG << "Creating and Initializing EDTs: \n");
  SetVector<Function *> Functions;
  EDTSet EDTs;
  DenseMap<EDTFunction, EDT *> FunctionEDTMap;
  for (Function &Fn : M) {
    if (Fn.isDeclaration() && !Fn.hasLocalLinkage())
      continue;
    Functions.insert(&Fn);
    if (EDTMetadata *MD = EDTMetadata::getMetadata(Fn)) {
      auto *CurrentEDT = EDT::get(MD);
      EDTs.insert(CurrentEDT);
      FunctionEDTMap[&Fn] = CurrentEDT;
    }
  }

  LLVM_DEBUG(dbgs() << "\n\n" << TAG << "Initializing AAEDTInfo: \n");
  /// Create Attributor
  AnalysisGetter AG;
  BumpPtrAllocator Allocator;
  CallGraphUpdater CGUpdater;
  const unsigned MaxFixpointIterations = 32;
  EDTInfoCache InfoCache(M, AG, Allocator, Functions, EDTs, FunctionEDTMap);
  Attributor A(Functions, InfoCache, CGUpdater, nullptr, false, true,
               MaxFixpointIterations, nullptr, DEBUG_TYPE);

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
  LLVM_DEBUG(dbgs() << "\n-------------------------------------------------\n");
  // LLVM_DEBUG(dbgs() << TAG << "Process has finished\n\n" << M << "\n");
  LLVM_DEBUG(dbgs() << TAG << "Process has finished\n");
  LLVM_DEBUG(dbgs() << "\n-------------------------------------------------\n");
  return false;
}

Noelle &ARTSAnalysisPass::getNoelle() { return getAnalysis<Noelle>(); }

MemorySSA &ARTSAnalysisPass::getMemorySSA(Function &F) {
  return getAnalysis<MemorySSAWrapperPass>(F).getMSSA();
}

void ARTSAnalysisPass::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addPreserved<MemorySSAWrapperPass>();
  AU.addRequired<MemorySSAWrapperPass>();
  AU.addRequired<Noelle>();
  ModulePass::getAnalysisUsage(AU);
}

} // namespace arts

// Next there is code to register your pass to "opt"
char ARTSAnalysisPass::ID = 0;
static RegisterPass<ARTSAnalysisPass> X("ARTSAnalysisPass",
                                        "ARTS Analysis Pass", false, false);