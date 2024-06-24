// Description: Main file for the ARTS Analysis pass.
#include "llvm/Analysis/CallGraph.h"
#include "llvm/Analysis/MemorySSA.h"
#include "llvm/Analysis/ValueTracking.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Pass.h"
#include "llvm/Support/Debug.h"
#include "llvm/Transforms/IPO/Attributor.h"
#include "llvm/Transforms/Utils/CallGraphUpdater.h"

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
  EDTGraphNode *EDTNode;

  /// Flag to track if we reached a fixpoint.
  bool IsAtFixpoint = false;

  /// The EDTs regions (identified by the outlined EDT functions) that
  /// can be reached from the associated EDT Function.
  BooleanStateWithPtrSetVector<EDTGraphNode, /* InsertInvalidates */ false>
      ReachedKnownEDTs;

  /// State to track what parallel region we might reach.
  BooleanStateWithPtrSetVector<EDTGraphNode> ReachedUnknownEDTs;

  EDT *getEDT() const { return EDTNode->getEDT(); }

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
    ReachedKnownEDTs.indicatePessimisticFixpoint();
    ReachedUnknownEDTs.indicatePessimisticFixpoint();
    return ChangeStatus::CHANGED;
  }

  /// See AbstractState::indicateOptimisticFixpoint(...)
  ChangeStatus indicateOptimisticFixpoint() override {
    IsAtFixpoint = true;
    ReachedKnownEDTs.indicateOptimisticFixpoint();
    ReachedUnknownEDTs.indicateOptimisticFixpoint();
    return ChangeStatus::UNCHANGED;
  }

  /// Return the assumed state
  EDTInfoState &getAssumed() { return *this; }
  const EDTInfoState &getAssumed() const { return *this; }

  bool operator==(const EDTInfoState &RHS) const {
    if (ReachedKnownEDTs != RHS.ReachedKnownEDTs)
      return false;
    if (ReachedUnknownEDTs != RHS.ReachedUnknownEDTs)
      return false;
    return true;
  }

  /// Returns true if this kernel contains any OpenMP parallel regions.
  // bool mayContainParallelRegion() {
  //   return !ReachedKnownEDTs.empty() ||
  //          !ReachedUnknownEDTs.empty();
  // }

  /// Return empty set as the best state of potential values.
  static EDTInfoState getBestState() { return EDTInfoState(true); }

  static EDTInfoState getBestState(EDTInfoState &KIS) { return getBestState(); }

  /// Return full set as the worst state of potential values.
  static EDTInfoState getWorstState() { return EDTInfoState(false); }

  /// "Clamp" this state with \p KIS.
  EDTInfoState operator^=(const EDTInfoState &KIS) {
    // Do not merge two different _init and _deinit call sites.
    // if (KIS.KernelInitCB) {
    //   if (KernelInitCB && KernelInitCB != KIS.KernelInitCB)
    //     llvm_unreachable("Kernel that calls another kernel violates
    //     OpenMP-Opt "
    //                      "assumptions.");
    //   KernelInitCB = KIS.KernelInitCB;
    // }
    // if (KIS.KernelDeinitCB) {
    //   if (KernelDeinitCB && KernelDeinitCB != KIS.KernelDeinitCB)
    //     llvm_unreachable("Kernel that calls another kernel violates
    //     OpenMP-Opt "
    //                      "assumptions.");
    //   KernelDeinitCB = KIS.KernelDeinitCB;
    // }
    // SPMDCompatibilityTracker ^= KIS.SPMDCompatibilityTracker;
    ReachedKnownEDTs ^= KIS.ReachedKnownEDTs;
    ReachedUnknownEDTs ^= KIS.ReachedUnknownEDTs;
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
    if (!isValidState())
      return "<invalid>";
    return (ReachedKnownEDTs.isValidState()
                ? std::to_string(ReachedKnownEDTs.size())
                : "<invalid>") +
           ", #Unknown PRs: " +
           (ReachedUnknownEDTs.isValidState()
                ? std::to_string(ReachedUnknownEDTs.size())
                : "<invalid>");
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
               SetVector<Function *> &Functions, EDTSet &EDTs)
      : InformationCache(M, AG, Allocator, &Functions), EDTs(EDTs) {}

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


  /// Collection of known EDTs in the module
  EDTSet &EDTs;
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
    // This is a high-level transform that might change the constant arguments
    // of the init and dinit calls. We need to tell the Attributor about this
    // to avoid other parts using the current constant value for simpliication.
    auto &EDTCache = static_cast<EDTInfoCache &>(A.getInfoCache());

    // &>(A.getInfoCache());

    EDTFunction Fn = getAnchorScope();
    // if (!OMPInfoCache.Kernels.count(Fn))
    //   return;
  }

  /// Modify the IR based on the EDTInfoState as the fixpoint iteration is
  /// finished now.
  ChangeStatus manifest(Attributor &A) override {
    // // If we are not looking at a kernel with __kmpc_target_init and
    // // __kmpc_target_deinit call we cannot actually manifest the information.
    // if (!KernelInitCB || !KernelDeinitCB)
    //   return ChangeStatus::UNCHANGED;

    // // If we can we change the execution mode to SPMD-mode otherwise we build
    // a
    // // custom state machine.

    // if (!changeToSPMDMode(A, Changed))
    //   return buildCustomStateMachine(A);
    ChangeStatus Changed = ChangeStatus::UNCHANGED;
    return Changed;
  }

  /// Fixpoint iteration update function. Will be called every time a dependence
  /// changed its state (and in the beginning).
  ChangeStatus updateImpl(Attributor &A) override {
    EDTInfoState StateBefore = getState();

    // Callback to check a read/write instruction.
    auto CheckRWInst = [&](Instruction &I) {
      // We handle calls later.
      if (isa<CallBase>(I))
        return true;
      // We only care about write effects.
      if (!I.mayWriteToMemory())
        return true;
      if (auto *SI = dyn_cast<StoreInst>(&I)) {
        SmallVector<const Value *> Objects;
        getUnderlyingObjects(SI->getPointerOperand(), Objects);
        if (llvm::all_of(Objects,
                         [](const Value *Obj) { return isa<AllocaInst>(Obj); }))
          return true;
        // Check for AAHeapToStack moved objects which must not be guarded.
        auto &HS = A.getAAFor<AAHeapToStack>(
            *this, IRPosition::function(*I.getFunction()),
            DepClassTy::OPTIONAL);
        if (llvm::all_of(Objects, [&HS](const Value *Obj) {
              auto *CB = dyn_cast<CallBase>(Obj);
              if (!CB)
                return false;
              return HS.isAssumedHeapToStack(*CB);
            })) {
          return true;
        }
      }

      // Insert instruction that needs guarding.
      // SPMDCompatibilityTracker.insert(&I);
      return true;
    };

    bool UsedAssumedInformationInCheckRWInst = false;
    // if (!SPMDCompatibilityTracker.isAtFixpoint())
    if (!A.checkForAllReadWriteInstructions(
            CheckRWInst, *this, UsedAssumedInformationInCheckRWInst)) {
    }
    // SPMDCompatibilityTracker.indicatePessimisticFixpoint();
    return StateBefore == getState() ? ChangeStatus::UNCHANGED
                                     : ChangeStatus::CHANGED;
  }

private:
  /// Update info regarding reaching kernels.
  void updateReachingKernelEntries(Attributor &A,
                                   bool &AllReachingKernelsKnown) {
    //   auto PredCallSite = [&](AbstractCallSite ACS) {
    //     Function *Caller = ACS.getInstruction()->getFunction();

    //     assert(Caller && "Caller is nullptr");

    //     auto &CAA =
    //     A.getOrCreateAAFor<AAEDTInfo>(IRPosition::function(*Caller),
    //                                               this,
    //                                               DepClassTy::REQUIRED);
    //     if (CAA.ReachingKernelEntries.isValidState()) {
    //       ReachingKernelEntries ^= CAA.ReachingKernelEntries;
    //       return true;
    //     }

    //     // We lost track of the caller of the associated function, any kernel
    //     // could reach now.
    //     ReachingKernelEntries.indicatePessimisticFixpoint();

    //     return true;
    //   };

    //   if (!A.checkForAllCallSites(PredCallSite, *this,
    //                               true /* RequireAllCallSites */,
    //                               AllReachingKernelsKnown))
    //     ReachingKernelEntries.indicatePessimisticFixpoint();
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
  case IRPosition::IRP_CALL_SITE:
    llvm_unreachable("EDTInfo can only be created for function position!");
  // case IRPosition::IRP_CALL_SITE:
  // AA = new (A.Allocator) AAEDTInfoCallSite(IRP, A);
  // break;
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
  LLVM_DEBUG(dbgs() << "\n-------------------------------------------------\n");
  EDTGraphCache Cache(M, this);
  /// Get all functions and EDTs of the module
  EDTFunctionSet EDTFunctions;
  EDTSet EDTs;
  SetVector<Function *> Functions;
  for (Function &Fn : M) {
    if (Fn.isDeclaration() && !Fn.hasLocalLinkage())
      continue;
    Functions.insert(&Fn);
    if (EDTMetadata *MD = EDTMetadata::getMetadata(Fn)) {
      EDTs.insert(EDT::get(Cache, MD));
      EDTFunctions.insert(&Fn);
    }
  }

  /// Create Attributor
  AnalysisGetter AG;
  BumpPtrAllocator Allocator;
  CallGraphUpdater CGUpdater;
  const unsigned MaxFixpointIterations = 32;
  EDTInfoCache InfoCache(M, AG, Allocator, Functions, EDTs);
  Attributor A(Functions, InfoCache, CGUpdater, nullptr, false, true,
               MaxFixpointIterations, nullptr, DEBUG_TYPE);

  /// Register AAs
  for (EDT *E : EDTs) {
    A.getOrCreateAAFor<AAEDTInfo>(IRPosition::function(*E->getFn()),
                                  /* QueryingAA */ nullptr, DepClassTy::NONE,
                                  /* ForceUpdate */ false,
                                  /* UpdateAfterInit */ false);
  }
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