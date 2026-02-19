///==========================================================================///
/// File: SerialEdtifyPass.cpp
///
/// Outlines eligible host-side serial loops into arts.edt<task> regions so
/// they flow through the existing distributed ForLowering pipeline.
///
/// Example:
///   Before:
///     scf.for %i = ... { ... }
///
///   After:
///     arts.edt <task> {
///       scf.for %i = ... { ... }
///     }
///==========================================================================///

#include "../ArtsPassDetails.h"
#include "arts/Analysis/ArtsAnalysisManager.h"
#include "arts/Analysis/DistributionHeuristics.h"
#include "arts/Analysis/Loop/LoopAnalysis.h"
#include "arts/ArtsDialect.h"
#include "arts/Passes/ArtsPasses.h"
#include "arts/Utils/Metadata/LoopMetadata.h"
#include "arts/Utils/OperationAttributes.h"
#include "arts/Utils/ValueUtils.h"

#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/IRMapping.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Support/LogicalResult.h"

#include "llvm/ADT/SmallVector.h"
#include <cassert>

#include "arts/Utils/ArtsDebug.h"
ARTS_DEBUG_SETUP(serial_edtify);

using namespace mlir;
using namespace mlir::arts;

namespace {

static bool hasSafeLoopMetadata(scf::ForOp loop, LoopAnalysis *loopAnalysis) {
  if (loopAnalysis) {
    if (LoopNode *loopNode = loopAnalysis->getOrCreateLoopNode(loop)) {
      return loopNode->isParallelizableByMetadata();
    }
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

static bool hasEligibleStores(scf::ForOp loop) {
  bool hasStoreToStackOrHeapAlloc = false;
  bool hasUnsupportedStore = false;
  Operation *singleStoreRoot = nullptr;

  loop.walk([&](memref::StoreOp storeOp) {
    Operation *root = ValueUtils::getUnderlyingOperation(storeOp.getMemRef());
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

    hasStoreToStackOrHeapAlloc = true;
    return WalkResult::advance();
  });

  return hasStoreToStackOrHeapAlloc && !hasUnsupportedStore;
}

static bool hasAnyMemoryReads(scf::ForOp loop) {
  bool hasReads = false;
  loop.walk([&](Operation *op) {
    if (isa<memref::LoadOp, affine::AffineLoadOp>(op)) {
      hasReads = true;
      return WalkResult::interrupt();
    }
    return WalkResult::advance();
  });
  return hasReads;
}

static bool hasOnlyConstantStoreValues(scf::ForOp loop) {
  bool hasStores = false;
  bool hasNonConstantStore = false;
  loop.walk([&](memref::StoreOp storeOp) {
    hasStores = true;
    Value storedValue = ValueUtils::stripNumericCasts(storeOp.getValue());
    if (!storedValue.getDefiningOp<arith::ConstantOp>()) {
      hasNonConstantStore = true;
      return WalkResult::interrupt();
    }
    return WalkResult::advance();
  });
  return hasStores && !hasNonConstantStore;
}

static Operation *getSingleStoreRoot(scf::ForOp loop) {
  Operation *storeRoot = nullptr;
  loop.walk([&](memref::StoreOp storeOp) {
    storeRoot = ValueUtils::getUnderlyingOperation(storeOp.getMemRef());
    return WalkResult::interrupt();
  });
  return storeRoot;
}

static bool hasExternalStoreToRoot(scf::ForOp loop, Operation *storeRoot) {
  if (!loop || !storeRoot)
    return false;

  func::FuncOp parentFunc = loop->getParentOfType<func::FuncOp>();
  if (!parentFunc)
    return false;

  bool foundExternalStore = false;
  parentFunc.walk([&](memref::StoreOp storeOp) {
    if (foundExternalStore)
      return WalkResult::interrupt();

    Operation *root = ValueUtils::getUnderlyingOperation(storeOp.getMemRef());
    if (root != storeRoot)
      return WalkResult::advance();

    if (loop->isAncestor(storeOp.getOperation()))
      return WalkResult::advance();

    foundExternalStore = true;
    return WalkResult::interrupt();
  });

  return foundExternalStore;
}

static bool isEligibleSerialLoop(scf::ForOp loop, LoopAnalysis *loopAnalysis) {
  if (!loop)
    return false;

  /// Only edtify host loops. Nested loops are captured when cloning the
  /// outermost loop body.
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

  if (!hasEligibleStores(loop)) {
    ARTS_DEBUG("Skip loop (store target unsupported): " << loop);
    return false;
  }

  Operation *storeRoot = getSingleStoreRoot(loop);
  /// Keep distributed auto-outlining focused on update-form outputs.
  /// Pure init loops for read-only inputs are prone to over-partitioned
  /// inter-node writes and are not required for distributed ownership.
  if (!hasExternalStoreToRoot(loop, storeRoot)) {
    ARTS_DEBUG("Skip loop (store target has no external writer use): " << loop);
    return false;
  }

  /// Keep auto-outlining conservative for distributed mode: only outline
  /// store-only host loops (e.g. initialization). Read-modify/write kernels
  /// are lowered by the regular parallel pipeline and are error-prone when
  /// force-outlined through this path.
  if (hasAnyMemoryReads(loop)) {
    ARTS_DEBUG("Skip loop (contains memory reads): " << loop);
    return false;
  }

  /// Keep auto-outlining strictly to constant-fill initialization loops.
  /// Computed-value fills (e.g., 2mm's D init expression) can diverge after
  /// distribution when ownership/partitioning does not fully align.
  if (!hasOnlyConstantStoreValues(loop)) {
    ARTS_DEBUG("Skip loop (non-constant store values): " << loop);
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

static void edtifyLoop(scf::ForOp loop, const ArtsAbstractMachine *machine) {
  Location loc = loop.getLoc();
  OpBuilder builder(loop);

  /// Preserve original program order: the outlined EDT must complete before
  /// subsequent host operations observe loop results.
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

  ARTS_DEBUG("Outlined serial scf.for into arts.edt<task>: " << loop);
  loop.erase();
}

struct SerialEdtifyPass : public arts::SerialEdtifyBase<SerialEdtifyPass> {
  explicit SerialEdtifyPass(ArtsAnalysisManager *AM) : AM(AM) {}

  void runOnOperation() override {
    ModuleOp module = getOperation();
    LoopAnalysis *loopAnalysis = nullptr;
    const ArtsAbstractMachine *machine = nullptr;
    if (AM) {
      loopAnalysis = &AM->getLoopAnalysis();
      loopAnalysis->invalidate();
      machine = &AM->getAbstractMachine();
    }

    unsigned numCandidates = 0;
    unsigned numConverted = 0;

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
        /// Task-dependency kernels already carry explicit DbControl hints.
        /// Auto-outlining host loops in these functions perturbs DB
        /// partitioning and can explode dependency fan-in at runtime.
        if (hasDbControlOps) {
          ARTS_DEBUG("Skip loop (function has DbControlOps): " << loop);
          return;
        }
        if (!isEligibleSerialLoop(loop, loopAnalysis))
          return;
        candidates.push_back(loop);
      });
    });

    numCandidates = static_cast<unsigned>(candidates.size());

    for (scf::ForOp loop : candidates) {
      if (!loop || !loop->getBlock())
        continue;
      edtifyLoop(loop, machine);
      ++numConverted;
    }

    ARTS_INFO("SerialEdtify outlined " << numConverted << " / " << numCandidates
                                       << " eligible serial loop(s)");
  }

private:
  ArtsAnalysisManager *AM = nullptr;
};

} // namespace

namespace mlir {
namespace arts {
std::unique_ptr<Pass> createSerialEdtifyPass(ArtsAnalysisManager *AM) {
  return std::make_unique<SerialEdtifyPass>(AM);
}
} // namespace arts
} // namespace mlir
