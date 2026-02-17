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
#include "arts/Analysis/Loop/LoopAnalysis.h"
#include "arts/ArtsDialect.h"
#include "arts/Passes/ArtsPasses.h"
#include "arts/Utils/Metadata/LoopMetadata.h"
#include "arts/Utils/OperationAttributes.h"
#include "arts/Utils/ValueUtils.h"

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

  loop.walk([&](memref::StoreOp storeOp) {
    Operation *root = ValueUtils::getUnderlyingOperation(storeOp.getMemRef());
    if (!root || !isa<memref::AllocOp, memref::AllocaOp>(root)) {
      hasUnsupportedStore = true;
      return WalkResult::interrupt();
    }

    hasStoreToStackOrHeapAlloc = true;
    return WalkResult::advance();
  });

  return hasStoreToStackOrHeapAlloc && !hasUnsupportedStore;
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

static void edtifyLoop(scf::ForOp loop) {
  Location loc = loop.getLoc();
  OpBuilder builder(loop);

  Value routeZero = builder.create<arith::ConstantIntOp>(loc, 0, 32);
  auto outlinedEdt = builder.create<EdtOp>(
      loc, EdtType::task, EdtConcurrency::internode, routeZero, ValueRange{});
  outlinedEdt.setNoVerifyAttr(NoVerifyAttr::get(builder.getContext()));

  Block &edtBody = outlinedEdt.getBody().front();

  OpBuilder::InsertionGuard edtGuard(builder);
  builder.setInsertionPointToStart(&edtBody);

  auto schedAttr =
      ForScheduleKindAttr::get(builder.getContext(), ForScheduleKind::Static);
  auto outlinedFor = builder.create<arts::ForOp>(
      loc, ValueRange{loop.getLowerBound()}, ValueRange{loop.getUpperBound()},
      ValueRange{loop.getStep()}, schedAttr, ValueRange{});

  copyArtsMetadataAttrs(loop, outlinedFor);
  cloneScfLoopBodyIntoArtsFor(loop, outlinedFor, builder);

  builder.setInsertionPointToEnd(&edtBody);
  builder.create<arts::YieldOp>(loc);

  ARTS_DEBUG("Outlined serial scf.for into arts.edt<task>: " << loop);
  loop.erase();
}

struct SerialEdtifyPass : public arts::SerialEdtifyBase<SerialEdtifyPass> {
  explicit SerialEdtifyPass(ArtsAnalysisManager *AM) : AM(AM) {}

  void runOnOperation() override {
    ModuleOp module = getOperation();
    LoopAnalysis *loopAnalysis = nullptr;
    if (AM) {
      loopAnalysis = &AM->getLoopAnalysis();
      loopAnalysis->invalidate();
    }

    unsigned numCandidates = 0;
    unsigned numConverted = 0;

    SmallVector<scf::ForOp> candidates;
    module.walk([&](func::FuncOp funcOp) {
      if (funcOp.isDeclaration())
        return;

      funcOp.walk([&](scf::ForOp loop) {
        if (!isEligibleSerialLoop(loop, loopAnalysis))
          return;
        candidates.push_back(loop);
      });
    });

    numCandidates = static_cast<unsigned>(candidates.size());

    for (scf::ForOp loop : candidates) {
      if (!loop || !loop->getBlock())
        continue;
      edtifyLoop(loop);
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
