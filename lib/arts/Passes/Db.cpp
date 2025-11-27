///==========================================================================///
/// File: Db.cpp
/// Pass for DB optimizations and memory management.
///==========================================================================///

/// Dialects
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/BuiltinAttributes.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/Value.h"
#include "mlir/Support/LLVM.h"
/// Arts
#include "ArtsPassDetails.h"
#include "arts/Analysis/ArtsAnalysisManager.h"
#include "arts/Analysis/Graphs/Db/DbGraph.h"
#include "arts/Analysis/Loop/LoopAnalysis.h"
#include "arts/Analysis/Metadata/ArtsMetadataManager.h"
#include "arts/ArtsDialect.h"
#include "arts/Passes/ArtsPasses.h"
#include "arts/Utils/OperationAttributes.h"
/// Other
#include "mlir/IR/Dominance.h"
#include "mlir/IR/OpDefinition.h"
#include "mlir/IR/Operation.h"
#include "mlir/IR/Visitors.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Support/LogicalResult.h"
/// Debug
#include "arts/Analysis/Graphs/Db/DbNode.h"
#include "arts/Analysis/Graphs/Edt/EdtGraph.h"
#include "arts/Analysis/Graphs/Edt/EdtNode.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/raw_ostream.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iterator>
#include <limits>

#include "arts/Transforms/DbRewriter.h"
#include "arts/Utils/ArtsDebug.h"
#include "arts/Utils/ArtsUtils.h"
#include "arts/Utils/Metadata/ArtsMetadata.h"
#include "arts/Utils/Metadata/LoopMetadata.h"
#include "arts/Utils/Metadata/MemrefMetadata.h"
ARTS_DEBUG_SETUP(db);

using namespace mlir;
using namespace mlir::func;
using namespace mlir::arith;
using namespace mlir::polygeist;
using namespace mlir::arts;

///===----------------------------------------------------------------------===///
// Pass Implementation
// Transform/optimize Datablocks using
///===----------------------------------------------------------------------===///
namespace {
struct DbPass : public arts::DbBase<DbPass> {
  DbPass(ArtsAnalysisManager *AM, bool enablePartitioning)
      : AM(AM), enablePartitioning(enablePartitioning) {
    assert(AM && "ArtsAnalysisManager must be provided externally");
  }

  void runOnOperation() override;

  /// Optimizations
  bool adjustDbModes();
  bool partitionDb();

  /// Graph rebuild
  void invalidateAndRebuildGraph();
  bool isEligibleForSlicing(DbAcquireNode *node);
  FailureOr<DbAllocOp> promoteAllocForChunking(DbAllocOp allocOp);
  bool updateAcquireForChunk(DbAcquireOp acqOp, DbAllocOp promotedAlloc,
                             Value chunkOffset, Value chunkSize);
  bool normalizeRootAcquires(DbAllocOp allocOp);
  bool rewriteAcquireUsersForChunk(DbAcquireOp acqOp, Value chunkOffset);

private:
  ModuleOp module;
  ArtsAnalysisManager *AM = nullptr;
  bool enablePartitioning = true;

  /// Helper functions
  ArtsMode inferModeFromMetadata(Operation *allocOp);
  std::optional<int64_t> suggestChunkSize(Operation *loopOp,
                                          Operation *allocOp);
};
} // namespace

void DbPass::runOnOperation() {
  bool changed = false;
  module = getOperation();

  ARTS_INFO_HEADER(DbPass);
  ARTS_DEBUG_REGION(module.dump(););

  assert(AM && "ArtsAnalysisManager must be provided externally");

  /// Graph construction and analysis
  invalidateAndRebuildGraph();

  changed |= adjustDbModes();
  changed |= partitionDb();

  if (changed) {
    /// If the module has changed, adjust the db modes again
    // changed |= adjustDbModes();
    ARTS_INFO(" Module has changed, invalidating analyses");
    AM->invalidate();
  }

  ARTS_INFO_FOOTER(DbPass);
  ARTS_DEBUG_REGION(module.dump(););
}

///===----------------------------------------------------------------------===///
/// Adjust DB modes based on accesses.
/// Based on the collected loads and stores, adjust the DB mode to in, out, or
/// inout.
///===----------------------------------------------------------------------===///
bool DbPass::adjustDbModes() {
  ARTS_DEBUG_HEADER(AdjustDBModes);
  bool changed = false;

  module.walk([&](func::FuncOp func) {
    DbGraph &graph = AM->getDbGraph(func);

    /// First, adjust per-acquire modes
    graph.forEachAcquireNode([&](DbAcquireNode *acqNode) {
      bool hasLoads = !acqNode->getLoads().empty();
      bool hasStores = !acqNode->getStores().empty();

      DbAcquireOp acqOp = acqNode->getDbAcquireOp();
      ArtsMode newMode = ArtsMode::in;
      if (hasLoads && hasStores)
        newMode = ArtsMode::inout;
      else if (hasStores)
        newMode = ArtsMode::out;
      else
        newMode = ArtsMode::in;

      /// Each DbAcquireNode's mode is derived from its own accesses only.
      /// Nested acquires will be processed separately by forEachAcquireNode.
      if (newMode == acqOp.getMode())
        return;

      ARTS_DEBUG("AcquireOp: " << acqOp << " from " << acqOp.getMode() << " to "
                               << newMode);
      acqOp.setModeAttr(ArtsModeAttr::get(acqOp.getContext(), newMode));
      changed = true;
    });

    /// Then, adjust alloc dbMode - collect modes from all acquires in hierarchy
    graph.forEachAllocNode([&](DbAllocNode *allocNode) {
      ArtsMode maxMode = ArtsMode::in;

      /// Recursive helper to collect modes from all acquire levels
      std::function<void(DbAcquireNode *)> collectModes =
          [&](DbAcquireNode *acqNode) {
            ArtsMode mode = acqNode->getDbAcquireOp().getMode();
            maxMode = combineAccessModes(maxMode, mode);
            acqNode->forEachChildNode([&](NodeBase *child) {
              if (auto *nestedAcq = dyn_cast<DbAcquireNode>(child))
                collectModes(nestedAcq);
            });
          };

      allocNode->forEachChildNode([&](NodeBase *child) {
        if (auto *acqNode = dyn_cast<DbAcquireNode>(child))
          collectModes(acqNode);
      });

      /// Use metadata from allocNode's info
      if (allocNode->accessStats.readWriteRatio) {
        double ratio = *allocNode->accessStats.readWriteRatio;
        ArtsMode metadataMode = ArtsMode::inout;
        if (ratio > 0.9)
          metadataMode = ArtsMode::in;
        else if (ratio < 0.1)
          metadataMode = ArtsMode::out;
        maxMode = combineAccessModes(maxMode, metadataMode);
        ARTS_DEBUG("  Using metadata read/write ratio: " << ratio << " -> "
                                                         << metadataMode);
      }

      /// Update the alloc mode
      DbAllocOp allocOp = allocNode->getDbAllocOp();
      ArtsMode currentDbMode = allocOp.getMode();
      if (currentDbMode == maxMode)
        return;
      ARTS_DEBUG("AllocOp: " << allocOp << " from " << currentDbMode << " to "
                             << maxMode);
      allocOp.setModeAttr(ArtsModeAttr::get(allocOp.getContext(), maxMode));

      changed = true;
    });
  });

  ARTS_DEBUG_FOOTER(AdjustDBModes);
  return changed;
}

bool DbPass::partitionDb() {
  if (!enablePartitioning)
    return false;
  ARTS_DEBUG_HEADER(PartitionDb);
  bool changed = false;
  OpBuilder attrBuilder(module.getContext());
  llvm::SmallPtrSet<Operation *, 8> partitionAttempt;
  llvm::SmallPtrSet<Operation *, 8> partitionSuccess;
  llvm::SmallPtrSet<Operation *, 8> partitionFailed;

  /// Collect all (alloc, acquire, offset, size) tuples
  using AcqChunkInfo = std::tuple<DbAcquireOp, Value, Value>;
  DenseMap<DbAllocOp, SmallVector<AcqChunkInfo, 4>> allocsToPromote;

  /// Walk the module,
  module.walk([&](func::FuncOp func) {
    DbGraph &graph = AM->getDbGraph(func);

    SmallVector<DbAcquireNode *, 16> candidates;
    /// Walk the graph, and collect the eligible acquire nodes
    graph.forEachAcquireNode([&](DbAcquireNode *acqNode) {
      if (!isEligibleForSlicing(acqNode))
        return;

      /// If the parent allocation node is not eligible for slicing, skip
      DbAllocNode *allocNode = acqNode->getRootAlloc();
      if (!allocNode || !allocNode->shouldSliceAlloc())
        return;

      /// Add the acquire node to the candidates list
      candidates.push_back(acqNode);
    });

    /// For each eligible acquire node, compute the chunk info and promote the
    /// allocation
    for (DbAcquireNode *node : candidates) {
      Value chunkOffset, chunkSize;
      auto [cachedOffset, cachedSize] = node->getPartitionInfo();
      DbAllocNode *allocNode = node->getRootAlloc();
      if (allocNode && allocNode->getDbAllocOp())
        partitionAttempt.insert(allocNode->getDbAllocOp().getOperation());
      if (cachedOffset && cachedSize) {
        chunkOffset = cachedOffset;
        chunkSize = cachedSize;
        ARTS_DEBUG("  Using cached chunk info for acquire "
                   << node->getDbAcquireOp() << " offset=" << chunkOffset
                   << " size=" << chunkSize);
      } else if (failed(node->computeChunkInfo(chunkOffset, chunkSize))) {
        ARTS_DEBUG("  Failed to compute chunk info for acquire "
                   << node->getDbAcquireOp());
        if (allocNode && allocNode->getDbAllocOp())
          partitionFailed.insert(allocNode->getDbAllocOp().getOperation());
        continue;
      } else {
        node->setPartitionInfo(chunkOffset, chunkSize);
        ARTS_DEBUG("  Computed chunk info for acquire "
                   << node->getDbAcquireOp() << " offset=" << chunkOffset
                   << " size=" << chunkSize);
      }

      if (!allocNode)
        continue;

      DbAllocOp allocOp = allocNode->getDbAllocOp();
      DbAcquireOp acqOp = node->getDbAcquireOp();

      /// Collect this (acquire, offset, size) tuple for this alloc
      allocsToPromote[allocOp].push_back(
          std::make_tuple(acqOp, chunkOffset, chunkSize));
    }
  });

  /// Now promote allocations and update acquires (OUTSIDE the walk)
  /// Copy to vector to avoid iterator invalidation when keys (Operation*) get
  /// erased
  SmallVector<std::pair<DbAllocOp,
                        SmallVector<std::tuple<DbAcquireOp, Value, Value>, 4>>>
      allocsToProcess;
  for (auto &[allocOpPtr, acquireList] : allocsToPromote) {
    allocsToProcess.push_back(
        {cast<DbAllocOp>(allocOpPtr), std::move(acquireList)});
  }
  allocsToPromote.clear(); /// Clear map to avoid using dangling pointers

  for (auto &[allocOp, acquireList] : allocsToProcess) {
    ARTS_DEBUG("Promoting alloc with " << acquireList.size() << " acquires");

    auto promotedOr = promoteAllocForChunking(allocOp);
    if (failed(promotedOr)) {
      partitionFailed.insert(allocOp.getOperation());
      continue;
    }

    DbAllocOp promoted = promotedOr.value();
    partitionAttempt.insert(promoted.getOperation());
    if (promoted.getOperation() == allocOp.getOperation()) {
      ARTS_DEBUG("  Alloc already promoted, updating acquires");
    } else {
      /// allocOp has been erased, can't access it
      ARTS_DEBUG("  Promoted alloc to sizes=" << promoted.getSizes().size());
      changed = true;
    }

    changed |= normalizeRootAcquires(promoted);
    bool chunkRewrite = false;

    /// Update all acquires for this alloc
    for (auto &[acqOp, chunkOffset, chunkSize] : acquireList) {
      if (!acqOp)
        continue;

      if (updateAcquireForChunk(acqOp, promoted, chunkOffset, chunkSize)) {
        changed = true;
        chunkRewrite = true;
      }

      /// Verify the transformation
      if (acqOp.getOffsets().empty() || acqOp.getSizes().empty()) {
        ARTS_DEBUG("  WARNING: Acquire offsets or sizes empty after update");
      } else {
        ARTS_DEBUG("  Verified: Acquire has offsets="
                   << acqOp.getOffsets().size()
                   << " sizes=" << acqOp.getSizes().size());
      }

      Operation *underlyingAlloc =
          arts::getUnderlyingDbAlloc(acqOp.getSourcePtr());
      if (underlyingAlloc && underlyingAlloc != promoted.getOperation()) {
        ARTS_DEBUG(
            "  WARNING: Acquire source ptr doesn't match promoted alloc");
      }
    }
    if (chunkRewrite)
      partitionSuccess.insert(promoted.getOperation());
    else
      partitionFailed.insert(promoted.getOperation());
  }

  auto setTwinAttr = [&](DbAllocOp alloc, bool useTwinDiff) {
    if (!alloc)
      return;
    BoolAttr newAttr = attrBuilder.getBoolAttr(useTwinDiff);
    if (auto existing = alloc->getAttrOfType<BoolAttr>(
            AttrNames::Operation::ArtsTwinDiff))
      if (existing == newAttr)
        return;
    alloc->setAttr(AttrNames::Operation::ArtsTwinDiff, newAttr);
    changed = true;
  };

  for (Operation *op : partitionSuccess)
    setTwinAttr(DbAllocOp(op), false);

  for (Operation *op : partitionFailed) {
    if (partitionSuccess.contains(op))
      continue;
    setTwinAttr(DbAllocOp(op), true);
  }

  module.walk([&](DbAllocOp alloc) {
    if (!alloc->hasAttr(AttrNames::Operation::ArtsTwinDiff))
      setTwinAttr(alloc, false);
  });

  /// Rebuild graph if any promotions occurred
  if (changed) {
    ARTS_DEBUG("Rebuilding graph after promotions");
    invalidateAndRebuildGraph();
  }

  ARTS_DEBUG_FOOTER(PartitionDb);
  return changed;
}

bool DbPass::isEligibleForSlicing(DbAcquireNode *node) {
  if (!node)
    return false;

  /// Use the node's method to check eligibility
  return node->isEligibleForSlicing();
}

FailureOr<DbAllocOp> DbPass::promoteAllocForChunking(DbAllocOp allocOp) {
  if (!allocOp || !allocOp.getOperation())
    return failure();

  /// Example of the promotion performed here:
  ///   coarse alloc : sizes=[1], elementSizes=[N]
  ///   normalized   : sizes=[N], elementSizes=[1]
  /// Root acquires are then normalized to offsets=[0], sizes=[N] so worker
  /// acquires can safely slice chunks by rewriting offsets/sizes only.
  ValueRange elementSizes = allocOp.getElementSizes();
  ValueRange sizes = allocOp.getSizes();
  if (elementSizes.empty())
    return failure();

  bool elementsAreUnit = llvm::all_of(elementSizes.drop_front(), [](Value v) {
    int64_t cst;
    return arts::getConstantIndex(v, cst) && cst == 1;
  });
  bool alreadyPromoted = !sizes.empty() && !elementsAreUnit;
  if (alreadyPromoted)
    return allocOp;

  OpBuilder builder(allocOp);
  Location loc = allocOp.getLoc();
  Value one = builder.create<arith::ConstantIndexOp>(loc, 1);

  SmallVector<Value> newSizes;
  for (Value size : sizes) {
    int64_t cst;
    if (newSizes.empty() && arts::getConstantIndex(size, cst) && cst == 1)
      continue;
    newSizes.push_back(size);
  }
  newSizes.push_back(elementSizes.front());

  SmallVector<Value> newElementSizes;
  newElementSizes.append(elementSizes.begin() + 1, elementSizes.end());
  if (newElementSizes.empty())
    newElementSizes.push_back(one);

  Value address = allocOp.getAddress();
  auto newAlloc = builder.create<DbAllocOp>(
      loc, allocOp.getMode(), allocOp.getRoute(), allocOp.getAllocType(),
      allocOp.getDbMode(), allocOp.getElementType(), address, newSizes,
      newElementSizes);
  for (auto attr : allocOp->getAttrs()) {
    if (attr.getName().getValue() == "operandSegmentSizes")
      continue;
    newAlloc->setAttr(attr.getName(), attr.getValue());
  }

  DbRewriter::Config rewriterConfig;
  rewriterConfig.preserveSemantics = false;
  DbRewriter rewriter(module.getContext(), rewriterConfig);
  if (failed(rewriter.rewriteAllUses(allocOp, newAlloc))) {
    newAlloc.erase();
    return failure();
  }

  return newAlloc;
}

bool DbPass::normalizeRootAcquires(DbAllocOp allocOp) {
  if (!allocOp)
    return false;

  ValueRange allocSizes = allocOp.getSizes();
  if (allocSizes.empty())
    return false;

  /// Normalization ensures that the root acquire mirrors the promoted alloc
  /// (offset 0 / full size). Worker acquires can then safely slice without
  /// guessing the original extent.
  SmallVector<DbAcquireOp> acquires;
  for (Operation *user : allocOp.getPtr().getUsers())
    if (auto acq = dyn_cast<DbAcquireOp>(user))
      acquires.push_back(acq);

  if (acquires.empty())
    return false;

  bool changed = false;
  OpBuilder builder(allocOp.getContext());
  for (DbAcquireOp acq : acquires) {
    auto [edt, blockArg] = arts::getEdtBlockArgumentForAcquire(acq);
    /// Skip worker EDT acquires; they will be chunked separately.
    if (edt && edt.getType() == EdtType::task)
      continue;

    bool needsRewrite = acq.getSizes().size() != allocSizes.size() ||
                        !std::equal(acq.getSizes().begin(),
                                    acq.getSizes().end(), allocSizes.begin());
    if (!needsRewrite)
      continue;

    builder.setInsertionPoint(acq);
    Value zero = builder.create<arith::ConstantIndexOp>(acq.getLoc(), 0);
    SmallVector<Value> offsets(allocSizes.size(), zero);
    SmallVector<Value> sizes(allocSizes.begin(), allocSizes.end());
    acq.getOffsetsMutable().assign(offsets);
    acq.getSizesMutable().assign(sizes);
    acq.getOffsetHintsMutable().assign(offsets);
    acq.getSizeHintsMutable().assign(sizes);
    changed = true;
    ARTS_DEBUG("  Normalized acquire " << acq << " to full allocation extents");
  }

  return changed;
}

bool DbPass::rewriteAcquireUsersForChunk(DbAcquireOp acqOp, Value chunkOffset) {
  auto [edt, blockArg] = arts::getEdtBlockArgumentForAcquire(acqOp);
  if (!blockArg)
    return false;

  /// Try to reuse an existing DbRef result type if available to keep types
  /// consistent with surrounding code.
  Type targetType;
  for (Operation *user : blockArg.getUsers()) {
    if (auto ref = dyn_cast<DbRefOp>(user)) {
      targetType = ref.getResult().getType();
      break;
    }
  }
  if (!targetType) {
    targetType = blockArg.getType();
    if (auto outerMemref = targetType.dyn_cast<MemRefType>())
      if (auto innerMemref =
              outerMemref.getElementType().dyn_cast<MemRefType>())
        targetType = innerMemref;
  }

  if (!targetType || !targetType.isa<MemRefType>()) {
    ARTS_DEBUG("  Skipping chunk user rewrite for acquire "
               << acqOp << " (type mismatch)");
    return false;
  }

  SmallVector<Value> chunkOffsets{chunkOffset};
  llvm::SetVector<Operation *> opsToRemove;
  OpBuilder builder(acqOp.getContext());
  SmallVector<Operation *> users(blockArg.getUsers().begin(),
                                 blockArg.getUsers().end());
  bool rewritten = false;
  for (Operation *user : users) {
    if (rewriteDbUserOperation(user, targetType, blockArg, acqOp.getOperation(),
                               acqOp.getIndices().size(), chunkOffsets, builder,
                               opsToRemove))
      rewritten = true;
  }

  for (Operation *op : opsToRemove)
    op->erase();

  return rewritten;
}

bool DbPass::updateAcquireForChunk(DbAcquireOp acqOp, DbAllocOp promotedAlloc,
                                   Value chunkOffset, Value chunkSize) {
  if (!acqOp)
    return false;

  SmallVector<Value> offsets{chunkOffset};
  SmallVector<Value> sizes{chunkSize};
  acqOp.getOffsetsMutable().assign(offsets);
  acqOp.getSizesMutable().assign(sizes);
  /// Provide hints mirroring the chosen slice so downstream passes can keep
  /// worker-aware chunking stable.
  acqOp.getOffsetHintsMutable().assign(offsets);
  acqOp.getSizeHintsMutable().assign(sizes);

  (void)rewriteAcquireUsersForChunk(acqOp, chunkOffset);
  return true;
}

///===----------------------------------------------------------------------===///
/// Invalidate and rebuild the graph
///===----------------------------------------------------------------------===///
void DbPass::invalidateAndRebuildGraph() {
  module.walk([&](func::FuncOp func) {
    AM->invalidateFunction(func);
    (void)AM->getDbGraph(func);
  });
}

////===----------------------------------------------------------------------===////
/// Pass creation
////===----------------------------------------------------------------------===////
namespace mlir {
namespace arts {
std::unique_ptr<Pass> createDbPass(ArtsAnalysisManager *AM,
                                   bool enablePartitioning) {
  return std::make_unique<DbPass>(AM, enablePartitioning);
}
} // namespace arts
} // namespace mlir
