///==========================================================================///
/// File: DistributedHostLoopOutliningPass.cpp
///
/// Outlines eligible host-side loops into arts.edt<task> regions so they can
/// flow through the normal distributed arts.for lowering path. This is used
/// for multinode execution in general and is also relied on by
/// --distributed-db mode.
///==========================================================================///

#include "../ArtsPassDetails.h"
#include "arts/Analysis/ArtsAnalysisManager.h"
#include "arts/Analysis/DistributionHeuristics.h"
#include "arts/Analysis/Loop/LoopAnalysis.h"
#include "arts/ArtsDialect.h"
#include "arts/Passes/ArtsPasses.h"
#include "arts/Utils/DatablockUtils.h"
#include "arts/Utils/Metadata/LoopMetadata.h"
#include "arts/Utils/OperationAttributes.h"
#include "arts/Utils/ValueUtils.h"

#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/IRMapping.h"
#include "mlir/Interfaces/SideEffectInterfaces.h"
#include "mlir/Pass/Pass.h"

#include "llvm/ADT/SmallVector.h"
#include <cassert>

#include "arts/Utils/ArtsDebug.h"
ARTS_DEBUG_SETUP(distributed_host_loop_outlining);

using namespace mlir;
using namespace mlir::arts;

namespace {

static bool hasSafeLoopMetadata(scf::ForOp loop, LoopAnalysis *loopAnalysis) {
  if (loopAnalysis) {
    if (LoopNode *loopNode = loopAnalysis->getOrCreateLoopNode(loop))
      return loopNode->isParallelizableByMetadata();
  }

  auto metadata =
      loop->getAttrOfType<LoopMetadataAttr>(AttrNames::LoopMetadata::Name);
  if (!metadata)
    return true;

  if (metadata.getPotentiallyParallel() &&
      !metadata.getPotentiallyParallel().getValue())
    return false;
  if (metadata.getHasInterIterationDeps() &&
      metadata.getHasInterIterationDeps().getValue())
    return false;
  if (metadata.getHasReductions() && metadata.getHasReductions().getValue())
    return false;
  if (metadata.getParallelClassification() &&
      metadata.getParallelClassification().getInt() ==
          static_cast<int64_t>(
              LoopMetadata::ParallelClassification::Sequential))
    return false;

  return true;
}

static bool hasNestedIterArgLoops(scf::ForOp loop) {
  bool hasIterArgs = false;
  loop.walk([&](scf::ForOp nested) {
    if (!nested.getInitArgs().empty()) {
      hasIterArgs = true;
      return WalkResult::interrupt();
    }
    return WalkResult::advance();
  });
  return hasIterArgs;
}

static Operation *getSingleStoreRoot(scf::ForOp loop) {
  bool hasStores = false;
  bool hasUnsupportedStore = false;
  Operation *singleStoreRoot = nullptr;

  loop.walk([&](Operation *op) {
    auto access = DatablockUtils::getMemoryAccessInfo(op);
    if (!access || !access->isWrite())
      return WalkResult::advance();

    Operation *root = ValueUtils::getUnderlyingOperation(access->memref);
    if (!root || !isa<memref::AllocOp, memref::AllocaOp>(root)) {
      hasUnsupportedStore = true;
      return WalkResult::interrupt();
    }

    if (!singleStoreRoot)
      singleStoreRoot = root;
    else if (singleStoreRoot != root) {
      hasUnsupportedStore = true;
      return WalkResult::interrupt();
    }

    hasStores = true;
    return WalkResult::advance();
  });

  if (!hasStores || hasUnsupportedStore)
    return nullptr;
  return singleStoreRoot;
}

static bool hasOnlySupportedEffects(scf::ForOp loop, Operation *storeRoot) {
  if (!storeRoot)
    return false;

  bool hasStores = false;
  bool hasUnsupportedOp = false;

  loop.walk([&](Operation *op) {
    if (op == loop.getOperation() || isa<scf::YieldOp>(op) ||
        isa<scf::ForOp, scf::IfOp>(op))
      return WalkResult::advance();

    if (auto access = DatablockUtils::getMemoryAccessInfo(op)) {
      Operation *root = ValueUtils::getUnderlyingOperation(access->memref);
      if (!root) {
        hasUnsupportedOp = true;
        return WalkResult::interrupt();
      }

      if (access->isWrite()) {
        if (root != storeRoot) {
          hasUnsupportedOp = true;
          return WalkResult::interrupt();
        }
        hasStores = true;
        return WalkResult::advance();
      }

      /// Keep this path focused on producer loops. Reading the same root would
      /// introduce self-dependence that the original host loop did not model as
      /// a task dependency.
      if (root == storeRoot) {
        hasUnsupportedOp = true;
        return WalkResult::interrupt();
      }
      return WalkResult::advance();
    }

    if (isa<memref::DimOp, memref::DeallocOp>(op))
      return WalkResult::advance();

    if (isMemoryEffectFree(op))
      return WalkResult::advance();

    hasUnsupportedOp = true;
    return WalkResult::interrupt();
  });

  return hasStores && !hasUnsupportedOp;
}

static Value getLoopInductionVar(Operation *op) {
  if (!op)
    return Value();

  if (auto artsFor = dyn_cast<arts::ForOp>(op)) {
    if (artsFor.getRegion().empty())
      return Value();
    Block &block = artsFor.getRegion().front();
    if (block.getNumArguments() == 0)
      return Value();
    return block.getArgument(0);
  }

  if (auto scfFor = dyn_cast<scf::ForOp>(op))
    return scfFor.getInductionVar();

  return Value();
}

static bool edtWillRunInternode(EdtOp edt,
                                const ArtsAbstractMachine *machine) {
  if (!edt)
    return false;
  if (edt.getConcurrency() == EdtConcurrency::internode)
    return true;
  if (!machine || !machine->isDistributed())
    return false;
  return edt.getTypeAttr().getValue() == EdtType::parallel;
}

static Operation *getTopLevelFuncChild(Operation *op, func::FuncOp parentFunc) {
  if (!op || !parentFunc)
    return nullptr;

  Operation *cursor = op;
  while (cursor && cursor->getParentOp() != parentFunc)
    cursor = cursor->getParentOp();
  return cursor;
}

static bool isOrderedAfter(Operation *anchor, Operation *candidate,
                           func::FuncOp parentFunc) {
  Operation *anchorTop = getTopLevelFuncChild(anchor, parentFunc);
  Operation *candidateTop = getTopLevelFuncChild(candidate, parentFunc);
  if (!anchorTop || !candidateTop)
    return false;
  if (anchorTop->getBlock() != candidateTop->getBlock())
    return false;
  return anchorTop->isBeforeInBlock(candidateTop);
}

static bool hasAlignedInternodeForUseAfter(scf::ForOp loop,
                                           Operation *storeRoot,
                                           const ArtsAbstractMachine *machine) {
  if (!loop || !storeRoot)
    return false;

  func::FuncOp parentFunc = loop->getParentOfType<func::FuncOp>();
  if (!parentFunc)
    return false;

  bool foundAlignedUse = false;
  parentFunc.walk([&](Operation *op) {
    if (foundAlignedUse)
      return WalkResult::interrupt();

    auto access = DatablockUtils::getMemoryAccessInfo(op);
    if (!access || access->indices.empty())
      return WalkResult::advance();

    if (ValueUtils::getUnderlyingOperation(access->memref) != storeRoot)
      return WalkResult::advance();
    if (!isOrderedAfter(loop.getOperation(), op, parentFunc))
      return WalkResult::advance();

    auto edtOp = op->getParentOfType<EdtOp>();
    if (!edtOp || !edtWillRunInternode(edtOp, machine))
      return WalkResult::advance();

    Value loopIV;
    if (auto artsFor = op->getParentOfType<arts::ForOp>())
      loopIV = getLoopInductionVar(artsFor.getOperation());
    else if (auto scfFor = op->getParentOfType<scf::ForOp>())
      loopIV = getLoopInductionVar(scfFor.getOperation());
    if (!loopIV)
      return WalkResult::advance();

    Value leadingIdx = ValueUtils::stripNumericCasts(access->indices.front());
    if (ValueUtils::sameValue(leadingIdx, loopIV) ||
        ValueUtils::dependsOn(leadingIdx, loopIV) ||
        ValueUtils::dependsOn(loopIV, leadingIdx)) {
      ARTS_DEBUG("Aligned later multinode use found for store root "
                 << *storeRoot
                                                                     << " via "
                                                                     << *op);
      foundAlignedUse = true;
      return WalkResult::interrupt();
    }

    return WalkResult::advance();
  });

  return foundAlignedUse;
}

static bool isEligibleDistributedHostLoop(scf::ForOp loop,
                                          LoopAnalysis *loopAnalysis,
                                          const ArtsAbstractMachine *machine) {
  if (!loop)
    return false;

  /// Only outline top-level host loops. Nested loops stay inside the cloned
  /// body of the selected outer loop.
  if (loop->getParentOfType<EdtOp>() || loop->getParentOfType<arts::ForOp>() ||
      loop->getParentOfType<scf::ForOp>()) {
    ARTS_DEBUG("Skip loop (not host top-level): " << loop);
    return false;
  }

  if (!loop.getInitArgs().empty()) {
    ARTS_DEBUG("Skip loop (iter_args unsupported): " << loop);
    return false;
  }

  if (!hasSafeLoopMetadata(loop, loopAnalysis)) {
    ARTS_DEBUG("Skip loop (metadata says non-parallelizable): " << loop);
    return false;
  }

  if (hasNestedIterArgLoops(loop)) {
    ARTS_DEBUG("Skip loop (nested iter_args loop): " << loop);
    return false;
  }

  Operation *storeRoot = getSingleStoreRoot(loop);
  if (!storeRoot) {
    ARTS_DEBUG("Skip loop (store target unsupported): " << loop);
    return false;
  }

  /// Allow producer/transform loops between EDTs, not just constant init
  /// loops, but stay conservative on side effects and self-dependences.
  if (!hasOnlySupportedEffects(loop, storeRoot)) {
    ARTS_DEBUG("Skip loop (unsupported reads/writes/effects): " << loop);
    return false;
  }

  /// Only outline loops whose written allocation is later consumed by aligned
  /// internode work. This allows A/C-style GEMM loops and rejects B-style
  /// loops whose leading dimension is not distributed by the downstream task.
  if (!hasAlignedInternodeForUseAfter(loop, storeRoot, machine)) {
    ARTS_DEBUG("Skip loop (no aligned later internode use for store root): "
               << loop);
    return false;
  }

  return true;
}

static void cloneScfLoopBodyIntoArtsFor(scf::ForOp sourceLoop,
                                        arts::ForOp targetLoop,
                                        OpBuilder &builder) {
  Region &targetRegion = targetLoop.getRegion();
  if (targetRegion.empty())
    targetRegion.push_back(new Block());

  Block &targetBlock = targetRegion.front();
  if (targetBlock.getNumArguments() == 0)
    targetBlock.addArgument(builder.getIndexType(), sourceLoop.getLoc());

  IRMapping mapper;
  mapper.map(sourceLoop.getInductionVar(), targetBlock.getArgument(0));

  Block &sourceBlock = sourceLoop.getRegion().front();

  OpBuilder::InsertionGuard guard(builder);
  builder.setInsertionPointToStart(&targetBlock);
  for (Operation &op : sourceBlock.without_terminator())
    builder.clone(op, mapper);
  builder.create<arts::YieldOp>(sourceLoop.getLoc());
}

static void applyMachineWorkerTopology(EdtOp outlinedEdt,
                                       const ArtsAbstractMachine *machine) {
  if (!outlinedEdt || outlinedEdt.getConcurrency() != EdtConcurrency::internode)
    return;

  auto workerConfig =
      DistributionHeuristics::resolveWorkerConfig(outlinedEdt, machine);
  if (!workerConfig)
    return;

  if (workerConfig->totalWorkers > 0)
    setWorkers(outlinedEdt.getOperation(), workerConfig->totalWorkers);
  if (workerConfig->workersPerNode > 0)
    setWorkersPerNode(outlinedEdt.getOperation(), workerConfig->workersPerNode);
}

static void outlineLoop(scf::ForOp loop, const ArtsAbstractMachine *machine) {
  Location loc = loop.getLoc();
  OpBuilder builder(loop);

  /// Preserve program order: the outlined EDT must complete before later host
  /// code observes loop results.
  auto epoch = builder.create<EpochOp>(loc);
  Region &epochRegion = epoch.getRegion();
  if (epochRegion.empty())
    epochRegion.push_back(new Block());
  Block &epochBlock = epochRegion.front();
  OpBuilder epochBuilder = OpBuilder::atBlockBegin(&epochBlock);

  Value routeZero = epochBuilder.create<arith::ConstantIntOp>(loc, 0, 32);
  auto outlinedEdt = epochBuilder.create<EdtOp>(
      loc, EdtType::task, EdtConcurrency::internode, routeZero, ValueRange{});
  outlinedEdt.setNoVerifyAttr(NoVerifyAttr::get(epochBuilder.getContext()));
  applyMachineWorkerTopology(outlinedEdt, machine);

  Block &edtBody = outlinedEdt.getBody().front();
  OpBuilder::InsertionGuard edtGuard(epochBuilder);
  epochBuilder.setInsertionPointToStart(&edtBody);

  auto schedAttr = ForScheduleKindAttr::get(epochBuilder.getContext(),
                                            ForScheduleKind::Static);
  auto outlinedFor = epochBuilder.create<arts::ForOp>(
      loc, ValueRange{loop.getLowerBound()}, ValueRange{loop.getUpperBound()},
      ValueRange{loop.getStep()}, schedAttr, ValueRange{});

  copyArtsMetadataAttrs(loop, outlinedFor);
  cloneScfLoopBodyIntoArtsFor(loop, outlinedFor, epochBuilder);

  epochBuilder.setInsertionPointToEnd(&edtBody);
  epochBuilder.create<arts::YieldOp>(loc);

  epochBuilder.setInsertionPointToEnd(&epochBlock);
  epochBuilder.create<arts::YieldOp>(loc);

  ARTS_DEBUG("Outlined host scf.for into distributed arts.edt<task>: " << loop);
  loop.erase();
}

struct DistributedHostLoopOutliningPass
    : public arts::DistributedHostLoopOutliningBase<
          DistributedHostLoopOutliningPass> {
  explicit DistributedHostLoopOutliningPass(ArtsAnalysisManager *AM) : AM(AM) {}

  void runOnOperation() override {
    ModuleOp module = getOperation();
    LoopAnalysis *loopAnalysis = nullptr;
    const ArtsAbstractMachine *machine = nullptr;
    if (AM) {
      loopAnalysis = &AM->getLoopAnalysis();
      loopAnalysis->invalidate();
      machine = &AM->getAbstractMachine();
    }

    SmallVector<scf::ForOp> candidates;
    module.walk([&](func::FuncOp funcOp) {
      if (funcOp.isDeclaration())
        return;

      bool hasDbControlOps = false;
      funcOp.walk([&](DbControlOp) {
        hasDbControlOps = true;
        return WalkResult::interrupt();
      });

      funcOp.walk([&](scf::ForOp loop) {
        /// Task-dependency kernels already carry explicit DB control hints.
        /// Auto-outlining host loops in those functions would perturb existing
        /// dependency planning.
        if (hasDbControlOps) {
          ARTS_DEBUG("Skip loop (function has DbControlOps): " << loop);
          return;
        }
        if (isEligibleDistributedHostLoop(loop, loopAnalysis, machine))
          candidates.push_back(loop);
      });
    });

    unsigned numConverted = 0;
    const unsigned numCandidates = static_cast<unsigned>(candidates.size());
    for (scf::ForOp loop : candidates) {
      if (!loop || !loop->getBlock())
        continue;
      outlineLoop(loop, machine);
      ++numConverted;
    }

    ARTS_INFO("DistributedHostLoopOutlining outlined "
              << numConverted << " / " << numCandidates
              << " eligible host loop(s)");
  }

private:
  ArtsAnalysisManager *AM = nullptr;
};

} // namespace

namespace mlir {
namespace arts {
std::unique_ptr<Pass>
createDistributedHostLoopOutliningPass(ArtsAnalysisManager *AM) {
  return std::make_unique<DistributedHostLoopOutliningPass>(AM);
}
} // namespace arts
} // namespace mlir
