///==========================================================================///
/// File: Db.cpp
/// Pass for DB optimizations and memory management.
///==========================================================================///
///
///===----------------------------------------------------------------------===///
/// Twin-Diff Policy for Distributed Datablock Synchronization
///===----------------------------------------------------------------------===///
///
/// In distributed execution, when multiple workers write to a shared datablock,
/// we must ensure data consistency. Twin-diff is a runtime mechanism that:
///   1. Allocates a shadow copy ("twin") of the datablock
///   2. Tracks which regions were modified during EDT execution
///   3. Sends only the changed regions to other nodes (bandwidth optimization)
///
/// Policy:
///   - DEFAULT: twin_diff = TRUE (safe, handles potential overlap at runtime)
///   - DISABLE: Only when we can PROVE non-overlapping access patterns
///
/// Proof Methods (in order of application):
///   1. Fine-grained allocation with DbControlOps (from OpenMP depend clauses)
///      - Set in CreateDbs.cpp when DbControlOps guarantee isolation
///   2. Successful partitioning with proven disjoint acquires
///      - Checked in partitionDb() after successful chunk promotion
///   3. DbAliasAnalysis proving disjoint acquires via offset/size analysis
///      - Uses constant slice extraction and range comparison
///
/// Safety Guarantee:
///   When twin-diff is DISABLED but workers write overlapping regions,
///   DATA CORRUPTION will occur. Therefore, we use a conservative default
///   (twin_diff=TRUE) and only disable when we have positive proof of safety.
///
///===----------------------------------------------------------------------===///

/// Dialects
#include "mlir/Dialect/Affine/IR/AffineOps.h"
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
#include "arts/Analysis/Db/DbAnalysis.h"
#include "arts/Analysis/Graphs/Db/DbGraph.h"
#include "arts/Analysis/Loop/LoopAnalysis.h"
#include "arts/Analysis/Loop/LoopNode.h"
#include "arts/ArtsDialect.h"
#include "arts/Passes/ArtsPasses.h"
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
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/raw_ostream.h"
#include <algorithm>
#include <cstdint>

#include "arts/Transforms/DbTransforms.h"
#include "arts/Utils/ArtsDebug.h"
#include "arts/Utils/ArtsUtils.h"
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

  /// Stencil bounds analysis
  void analyzeStencilBounds();
  void generateBoundsValid(DbAcquireOp acquireOp,
                           ArrayRef<int64_t> boundsCheckFlags, Value loopIV);

  /// Graph rebuild
  void invalidateAndRebuildGraph();
  FailureOr<DbAllocOp> promoteAllocForChunking(DbAllocOp allocOp,
                                               DbAllocNode *allocNode);
  void updateAcquireForChunk(DbAcquireOp acqOp, Value chunkOffset,
                             Value chunkSize);

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

  /// Always adjust DB modes
  changed |= adjustDbModes();

  /// Partitio DBs and analyze stencil patterns for bounds checking
  if (enablePartitioning) {
    changed |= partitionDb();
    analyzeStencilBounds();
  }

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
      bool hasLoads = acqNode->hasLoads();
      bool hasStores = acqNode->hasStores();

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

///===----------------------------------------------------------------------===///
/// partitionDb - Partition datablocks and set twin-diff attributes
///===----------------------------------------------------------------------===///
///
/// This function performs two key tasks:
/// 1. PARTITIONING: Attempts to promote coarse-grained allocations to
///    fine-grained ones by computing chunk info (offset, size) for each
///    acquire. This allows workers to access disjoint regions of the DB.
///
/// 2. TWIN-DIFF ASSIGNMENT: Sets the twin_diff attribute on each DbAcquireOp
///    based on whether overlapping access can be ruled out:
///    - twin_diff=FALSE: Only when partitioning succeeds AND we can prove
///      that all acquires access disjoint regions. This is safe because
///      workers won't write to overlapping memory.
///    - twin_diff=TRUE: When partitioning fails OR we cannot prove disjoint
///      access. The runtime will use the twin-diff mechanism to track changes
///      and merge updates correctly.
/// Decision Flow:
///   1. Collect candidates for partitioning (eligible acquires with chunk info)
///   2. For each allocation, attempt promotion and update acquires
///   3. If chunk rewrite succeeds → twin_diff=FALSE (proven disjoint)
///   4. If chunk rewrite fails → twin_diff=TRUE (potential overlap)
///   5. Final pass: any unset acquires get twin_diff=TRUE (safe default)
///===----------------------------------------------------------------------===///
bool DbPass::partitionDb() {
  ARTS_DEBUG_HEADER(PartitionDb);
  bool changed = false;
  OpBuilder attrBuilder(module.getContext());

  /// Helper lambda to set twin-diff attribute on a DbAcquireOp.
  auto setTwinAttr = [&](DbAcquireOp acq, bool useTwinDiff) {
    if (!acq)
      return;
    // Apply single-node heuristic
    if (AM->getHeuristicsConfig().shouldDisableTwinDiff())
      useTwinDiff = false;

    if (acq.hasTwinDiff() && acq.getTwinDiff() == useTwinDiff)
      return;
    acq.setTwinDiff(useTwinDiff);
    ARTS_DEBUG("  Set twin_diff=" << (useTwinDiff ? "true" : "false"));
    changed = true;
  };
  llvm::SmallPtrSet<Operation *, 8> partitionAttempt, partitionSuccess,
      partitionFailed;

  /// Collect all (alloc, acquire, offset, size) tuples
  using AcqChunkInfo = std::tuple<DbAcquireOp, Value, Value>;
  DenseMap<DbAllocOp, std::pair<DbAllocNode *, SmallVector<AcqChunkInfo, 4>>>
      allocsToPromote;

  /// Walk the module and check each allocation using hierarchical validation.
  /// DbAllocNode::canBePartitioned() recursively validates all acquire
  /// children.
  module.walk([&](func::FuncOp func) {
    DbGraph &graph = AM->getDbGraph(func);

    /// Iterate through allocations and use unified canBePartitioned() API
    graph.forEachAllocNode([&](DbAllocNode *allocNode) {
      if (!allocNode)
        return;

      DbAllocOp allocOp = allocNode->getDbAllocOp();
      if (!allocOp)
        return;

      partitionAttempt.insert(allocOp.getOperation());

      /// Hierarchical validation
      ARTS_DEBUG("Checking allocation: " << allocOp);
      if (!allocNode->canBePartitioned()) {
        ARTS_DEBUG("  SKIP: canBePartitioned() returned false");
        partitionFailed.insert(allocOp.getOperation());
        return;
      }

      /// H1: Skip partitioning for read-only allocations on single-node.
      if (AM->getHeuristicsConfig().shouldPreferCoarseForReadOnly(
              allocOp.getMode())) {
        ARTS_DEBUG(
            "  SKIP: H1 heuristic - read-only on single-node, keep coarse");
        return;
      }

      /// All validation passed
      bool allValid = true;
      SmallVector<std::tuple<DbAcquireNode *, Value, Value>, 4> validatedAcqs;

      allocNode->forEachChildNode([&](NodeBase *child) {
        if (!allValid)
          return;
        auto *acqNode = dyn_cast<DbAcquireNode>(child);
        if (!acqNode)
          return;

        Value chunkOffset, chunkSize;
        if (failed(acqNode->computeChunkInfo(chunkOffset, chunkSize))) {
          ARTS_DEBUG("  Failed to compute chunk info for acquire "
                     << acqNode->getDbAcquireOp());
          allValid = false;
          return;
        }
        ARTS_DEBUG("  Computed chunk info for acquire "
                   << acqNode->getDbAcquireOp() << " offset=" << chunkOffset
                   << " size=" << chunkSize);

        validatedAcqs.push_back({acqNode, chunkOffset, chunkSize});
      });

      if (!allValid) {
        ARTS_DEBUG("  Skipping alloc " << allocOp
                                       << ": not all acquires could compute "
                                          "chunk info");
        partitionFailed.insert(allocOp.getOperation());
        return;
      }

      /// All acquires validated - add to promotion list
      auto &[nodePtr, acquireList] = allocsToPromote[allocOp];
      if (!nodePtr)
        nodePtr = allocNode;
      for (auto &[node, offset, size] : validatedAcqs) {
        acquireList.push_back({node->getDbAcquireOp(), offset, size});
      }
    });
  });

  /// Now promote allocations and update acquires
  SmallVector<std::tuple<DbAllocOp, DbAllocNode *,
                         SmallVector<std::tuple<DbAcquireOp, Value, Value>, 4>>>
      allocsToProcess;
  for (auto &[allocOpPtr, nodeAndList] : allocsToPromote) {
    auto &[allocNode, acquireList] = nodeAndList;
    allocsToProcess.push_back(
        {cast<DbAllocOp>(allocOpPtr), allocNode, std::move(acquireList)});
  }

  /// Clear map to avoid using dangling pointers
  allocsToPromote.clear();

  for (auto &[allocOp, allocNode, acquireList] : allocsToProcess) {
    ARTS_DEBUG("Promoting alloc with " << acquireList.size() << " acquires");

    auto promotedOr = promoteAllocForChunking(allocOp, allocNode);
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

    bool chunkRewrite = false;

    /// Update all acquires for this alloc
    for (auto &[acqOp, chunkOffset, chunkSize] : acquireList) {
      if (!acqOp)
        continue;

      updateAcquireForChunk(acqOp, chunkOffset, chunkSize);
      changed = true;
      chunkRewrite = true;
      setTwinAttr(acqOp, false);

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

  /// AFTER partition attempts, BEFORE final default pass:
  /// Analyze acquires that weren't partitioned using DbAliasAnalysis and
  /// partition hints. This allows us to disable twin-diff for allocations
  /// where we can PROVE non-overlapping access even without full partitioning.
  ARTS_DEBUG("  Overlap analysis for unpartitioned allocations:");
  module.walk([&](func::FuncOp func) {
    DbGraph &graph = AM->getDbGraph(func);

    graph.forEachAllocNode([&](DbAllocNode *allocNode) {
      DbAllocOp allocOp = allocNode->getDbAllocOp();
      if (!allocOp)
        return;

      /// Skip if already handled by successful partitioning
      if (partitionSuccess.contains(allocOp.getOperation())) {
        ARTS_DEBUG("    Skipping alloc " << allocNode->getHierId()
                                         << " - already partitioned");
        return;
      }

      if (allocNode->getAcquireNodesSize() == 0)
        return;

      ARTS_DEBUG("    Analyzing alloc " << allocNode->getHierId() << " with "
                                        << allocNode->getAcquireNodesSize()
                                        << " acquires");

      /// Analyze using node's canProveNonOverlapping method
      bool allDisjoint = allocNode->canProveNonOverlapping();

      if (allDisjoint) {
        /// Set twin_diff=false on all acquires for this allocation
        allocNode->forEachChildNode([&](NodeBase *child) {
          if (auto *acqNode = dyn_cast<DbAcquireNode>(child)) {
            DbAcquireOp acqOp = acqNode->getDbAcquireOp();
            if (acqOp && !acqOp.hasTwinDiff()) {
              setTwinAttr(acqOp, false);
              ARTS_DEBUG("      acquire "
                         << acqNode->getHierId()
                         << ": proven disjoint -> twin_diff=false");
            }
          }
        });
      } else {
        ARTS_DEBUG("      Potential overlap detected → will use default "
                   "twin_diff=true");
      }
    });
  });

  /// Safe default - enable twin-diff for any unanalyzed acquires
  ARTS_DEBUG(
      "  Final pass: Setting safe default twin_diff=true for unset acquires");
  module.walk([&](DbAcquireOp acq) {
    if (!acq.hasTwinDiff()) {
      setTwinAttr(acq, true);
      ARTS_DEBUG("    Default twin_diff=true for unset acquire");
    }
  });

  /// Rebuild graph if any promotions occurred
  if (changed) {
    ARTS_DEBUG("Rebuilding graph after promotions");
    invalidateAndRebuildGraph();
  }

  ARTS_DEBUG_FOOTER(PartitionDb);
  return changed;
}

/// Promote coarse allocation for chunking.
/// sizes=[1], elementSizes=[N] -> sizes=[N], elementSizes=[1]
FailureOr<DbAllocOp> DbPass::promoteAllocForChunking(DbAllocOp allocOp,
                                                     DbAllocNode *allocNode) {
  if (!allocOp || !allocOp.getOperation())
    return failure();

  if (allocOp.getElementSizes().empty())
    return failure();

  // Already fine-grained - no promotion needed
  if (allocNode && allocNode->isFineGrained())
    return allocOp;

  OpBuilder builder(allocOp);
  DbTransforms transforms;

  /// TODO: Derive promoteCount from partition info dimensions.
  /// Currently we always promote 1 dimension for the common chunking pattern.
  /// For multi-dimensional partitioning, examine partition info dimensions.
  auto newAlloc = transforms.promoteAllocation(allocOp, 1, builder,
                                               /*trimLeadingOnes=*/true);
  if (!newAlloc)
    return failure();

  if (failed(transforms.rewriteAllUses(allocOp, newAlloc, builder))) {
    newAlloc.erase();
    return failure();
  }

  return newAlloc;
}

void DbPass::updateAcquireForChunk(DbAcquireOp acqOp, Value chunkOffset,
                                   Value chunkSize) {
  if (!acqOp)
    return;

  SmallVector<Value> offsets{chunkOffset};
  SmallVector<Value> sizes{chunkSize};
  acqOp.getOffsetsMutable().assign(offsets);
  acqOp.getSizesMutable().assign(sizes);
  /// Provide hints mirroring the chosen slice so downstream passes can keep
  /// worker-aware chunking stable.
  acqOp.getOffsetHintsMutable().assign(offsets);
  acqOp.getSizeHintsMutable().assign(sizes);

  /// Rebase users to local coordinates
  OpBuilder builder(acqOp.getContext());
  DbTransforms transforms;
  (void)transforms.rebaseAllUsersToAcquireView(acqOp, builder);
}

///===----------------------------------------------------------------------===///
/// Analyze stencil patterns to detect out-of-bounds indices
///
/// For stencil patterns like depend(in: u[i-1], u[i], u[i+1]), the boundary
/// iterations may access out-of-bounds indices (e.g., u[-1] when i=0).
/// This analysis detects such patterns by:
/// 1. Finding DbAcquireOps with indexed dependencies
/// 2. Using LoopAnalysis::collectEnclosingLoops() to find enclosing loops
/// 3. Using LoopNode::analyzeIndexExpr to detect offset patterns (e.g., i-1,
/// i+1)
/// 4. Generating runtime bounds checks and replacing DbAcquireOps with
/// bounds_valid
///===----------------------------------------------------------------------===///

/// Check if op is in else branch of `if (iv == 0)` where iv is the loop IV.
/// If so, we know iv > 0 (since loop starts at 0), so idx = iv-1 >= 0.
static bool isLowerBoundGuaranteedByControlFlow(Operation *op, Value loopIV) {
  if (!loopIV)
    return false;

  /// Helper: check if value matches loopIV (directly or via index_cast)
  auto matchesIV = [&loopIV](Value v) {
    if (v == loopIV)
      return true;
    if (auto cast = v.getDefiningOp<arith::IndexCastOp>())
      return cast.getIn() == loopIV;
    return false;
  };

  /// Helper: check if value is constant 0
  auto isZero = [](Value v) {
    if (auto c = v.getDefiningOp<arith::ConstantIntOp>())
      return c.value() == 0;
    if (auto c = v.getDefiningOp<arith::ConstantIndexOp>())
      return c.value() == 0;
    return false;
  };

  /// Walk up parent chain looking for scf.if with condition `iv == 0`
  for (Operation *p = op->getParentOp(); p; p = p->getParentOp()) {
    auto ifOp = dyn_cast<scf::IfOp>(p);
    if (!ifOp || ifOp.getElseRegion().empty())
      continue;

    /// Check if we're in else region
    if (!ifOp.getElseRegion().isAncestor(op->getParentRegion()))
      continue;

    /// Check if condition is `iv == 0`
    auto cmp = ifOp.getCondition().getDefiningOp<arith::CmpIOp>();
    if (!cmp || cmp.getPredicate() != arith::CmpIPredicate::eq)
      continue;

    Value lhs = cmp.getLhs(), rhs = cmp.getRhs();
    if ((matchesIV(lhs) && isZero(rhs)) || (matchesIV(rhs) && isZero(lhs))) {
      ARTS_DEBUG("  Lower bound guaranteed by control flow (else of iv==0)");
      return true;
    }
  }
  return false;
}

void DbPass::analyzeStencilBounds() {
  ARTS_DEBUG_HEADER(AnalyzeStencilBounds);

  LoopAnalysis &loopAnalysis = AM->getLoopAnalysis();

  /// Collect DbAcquireOps that need bounds checking along with flags and loopIV
  SmallVector<std::tuple<DbAcquireOp, SmallVector<int64_t>, Value>> toModify;

  module.walk([&](DbAcquireOp acquireOp) {
    /// Skip if already has bounds_valid
    if (acquireOp.getBoundsValid())
      return;

    auto indices = acquireOp.getIndices();
    if (indices.empty())
      return;

    /// Use LoopAnalysis to find enclosing loops
    SmallVector<LoopNode *> enclosingLoops;
    loopAnalysis.collectEnclosingLoops(acquireOp, enclosingLoops);
    if (enclosingLoops.empty())
      return;

    /// Use innermost enclosing loop for IV analysis
    LoopNode *loopNode = enclosingLoops.back();

    /// Compute boundsCheckFlags using analyzeIndexExpr (ONCE per index)
    SmallVector<int64_t> boundsCheckFlags;
    bool needsBoundsCheck = false;

    for (Value idx : indices) {
      int64_t flag = 0;
      LoopNode::IVExpr expr = loopNode->analyzeIndexExpr(idx);
      if (expr.isAnalyzable() && expr.offset.has_value() && *expr.offset != 0) {
        flag = 1;
        needsBoundsCheck = true;
        ARTS_DEBUG("  Index with offset " << *expr.offset
                                          << " needs bounds check");
      } else if (expr.dependsOnIV && !expr.isAnalyzable()) {
        flag = 1;
        needsBoundsCheck = true;
        ARTS_DEBUG("  Complex IV-dependent index needs bounds check");
      }
      boundsCheckFlags.push_back(flag);
    }

    if (needsBoundsCheck) {
      Value loopIV = loopNode->getInductionVar();
      toModify.push_back({acquireOp, std::move(boundsCheckFlags), loopIV});
    }
  });

  /// Generate boundsValid and update DbAcquireOps
  for (auto &[acquireOp, flags, loopIV] : toModify) {
    generateBoundsValid(acquireOp, flags, loopIV);
  }

  ARTS_DEBUG_FOOTER(AnalyzeStencilBounds);
}

/// Generate runtime bounds checks and create new DbAcquireOp with bounds_valid
void DbPass::generateBoundsValid(DbAcquireOp acquireOp,
                                 ArrayRef<int64_t> boundsCheckFlags,
                                 Value loopIV) {
  Location loc = acquireOp.getLoc();
  OpBuilder builder(acquireOp);

  auto indices = acquireOp.getIndices();
  SmallVector<Value> sourceSizes = getSizesFromDb(acquireOp.getSourcePtr());

  /// Check if lower bound is guaranteed by control flow (inside else of `if
  /// iv==0`)
  bool lowerBoundGuarded =
      isLowerBoundGuaranteedByControlFlow(acquireOp, loopIV);

  Value zero = builder.create<arith::ConstantIndexOp>(loc, 0);
  Value boundsValid;

  for (size_t i = 0; i < boundsCheckFlags.size() && i < indices.size(); ++i) {
    if (boundsCheckFlags[i] != 0 && i < sourceSizes.size()) {
      Value idx = indices[i];
      Value size = sourceSizes[i];
      Value dimValid;

      if (lowerBoundGuarded) {
        /// Only check upper bound: idx < size (lower bound guaranteed by
        /// control flow)
        dimValid = builder.create<arith::CmpIOp>(loc, arith::CmpIPredicate::slt,
                                                 idx, size);
      } else {
        /// Full check: 0 <= idx < size
        Value geZero = builder.create<arith::CmpIOp>(
            loc, arith::CmpIPredicate::sge, idx, zero);
        Value ltSize = builder.create<arith::CmpIOp>(
            loc, arith::CmpIPredicate::slt, idx, size);
        dimValid = builder.create<arith::AndIOp>(loc, geZero, ltSize);
      }

      boundsValid =
          boundsValid
              ? builder.create<arith::AndIOp>(loc, boundsValid, dimValid)
              : dimValid;
    }
  }

  if (!boundsValid)
    return;

  /// Recreate DbAcquireOp with bounds_valid
  SmallVector<Value> indicesVec(indices.begin(), indices.end());
  SmallVector<Value> offsetsVec(acquireOp.getOffsets().begin(),
                                acquireOp.getOffsets().end());
  SmallVector<Value> sizesVec(acquireOp.getSizes().begin(),
                              acquireOp.getSizes().end());
  SmallVector<Value> offsetHintsVec(acquireOp.getOffsetHints().begin(),
                                    acquireOp.getOffsetHints().end());
  SmallVector<Value> sizeHintsVec(acquireOp.getSizeHints().begin(),
                                  acquireOp.getSizeHints().end());

  auto newAcquire = builder.create<DbAcquireOp>(
      loc, acquireOp.getMode(), acquireOp.getSourceGuid(),
      acquireOp.getSourcePtr(), indicesVec, offsetsVec, sizesVec,
      offsetHintsVec, sizeHintsVec, boundsValid);
  transferAttributes(acquireOp, newAcquire);

  acquireOp.getGuid().replaceAllUsesWith(newAcquire.getGuid());
  acquireOp.getPtr().replaceAllUsesWith(newAcquire.getPtr());
  acquireOp.erase();

  ARTS_DEBUG("Added bounds_valid to DbAcquireOp: " << newAcquire);
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
