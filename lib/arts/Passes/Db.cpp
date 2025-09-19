///==========================================================================
/// File: Db.cpp
/// Pass for DB optimizations and memory management.
///==========================================================================

/// Dialects

#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/Value.h"
#include "mlir/Support/LLVM.h"
/// Arts
#include "ArtsPassDetails.h"
#include "arts/Analysis/ArtsAnalysisManager.h"
#include "arts/Analysis/Graphs/Db/DbGraph.h"
#include "arts/ArtsDialect.h"
#include "arts/Passes/ArtsPasses.h"
/// Other
#include "mlir/IR/Dominance.h"
#include "mlir/IR/OpDefinition.h"
#include "mlir/IR/Operation.h"
#include "mlir/IR/Visitors.h"
#include "mlir/Pass/Pass.h"
/// Debug
#include "arts/Analysis/Graphs/Db/DbNode.h"
#include "llvm/ADT/SmallVector.h"
#include <cstdint>
/// File operations
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/raw_ostream.h"

#include "arts/Utils/ArtsDebug.h"
#include "arts/Utils/ArtsUtils.h"
ARTS_DEBUG_SETUP(db);

using namespace mlir;
using namespace mlir::func;
using namespace mlir::arith;
using namespace mlir::polygeist;
using namespace mlir::arts;

//===----------------------------------------------------------------------===//
// Pass Implementation
// transform/optimize ARTS DB IR using DbGraph/analyses.
//===----------------------------------------------------------------------===//
namespace {
struct DbPass : public arts::DbBase<DbPass> {
  DbPass(ArtsAnalysisManager *AM, bool exportJson) : AM(AM) {
    assert(AM && "ArtsAnalysisManager must be provided externally");
    this->exportJson = exportJson;
  }

  void runOnOperation() override;

  /// DBs with single size, and "in" mode, can be converted into parameters.
  bool convertToParameters();

  /// Analyze DB usage and resize it to the minimum required based on min/max
  /// indices from acquires/releases.
  bool shrinkDb();

  /// Remove unused DB allocations.
  bool deadDbElimination();

  /// Reuse buffers for non-overlapping DB lifetimes.
  bool bufferReuse();

  /// Hoist allocations out of loops if invariant.
  bool hoistAllocs();

  /// Fuse consecutive acquires/releases on the same alloc if no intervening
  /// ops.
  bool fuseAccesses();

  /// Prefetch and overlap: hoist acquires earlier (after operand defs) and
  /// delay releases after last use within a block to overlap transfers.
  bool prefetchOverlap();

  /// Export DB analysis to JSON files in output/ directory.
  void exportToJson();

  /// Reduce datablock dimensionality at allocation level when SAFE.
  ///
  /// Concept: if, for a given `arts.db_alloc` and for every child acquire of
  /// this allocation, the first K leading dimensions are provably pinned to the
  /// SAME compile-time constant index values across the entire EDT body, then
  /// those K dimensions can be dropped from the allocation itself. All child
  /// acquires are rewritten to reference the new alloc, removing the dropped
  /// indices from their explicit `indices` list and trimming their
  /// `offsets`/`sizes` to the new rank when necessary. No per-acquire extra
  /// dimensionality reduction beyond the common prefix is performed here.
  ///
  /// Safety constraints:
  /// - All child acquires must provide K pinned leading indices (some via
  ///   existing `indices`, others via uniform, loop-invariant memref indices
  ///   discovered by analysis) and those Values must be constant indices equal
  ///   across all acquires for each dropped dimension.
  /// - We only drop a dimension if it is common to ALL acquires; otherwise we
  ///   skip (never specializing just one acquire here).
  /// - The transformation updates the corresponding EDT dependency operand and
  ///   its block argument to maintain type and arity consistency.
  bool reduceAllocDimensionality();

  /// Tighten acquire slices inside EDTs.
  ///
  /// Optimization A (offset tightening): For rank-1 acquires, if all accesses
  /// within the EDT target a single constant element, rewrite the acquire to
  /// `sizes=[1]` with `offsets=[base+idx]` and retarget load/store indices by
  /// subtracting the chosen offset (becomes 0).
  ///
  /// Optimization B (split by N elements): For rank-1 acquires with exactly
  /// N distinct constant element indices accessed in the EDT (and no dynamic
  /// indices), split the single acquire into N size-1 acquires (one per
  /// element), update the EDT dependency list and insert a new block argument
  /// for the second acquire, then retarget loads/stores to the corresponding
  /// block argument with index 0. The original release is replaced by N
  /// releases.
  bool tightenAcquireSlices();

private:
  ModuleOp module;
  ArtsAnalysisManager *AM = nullptr;
};
} // namespace

void DbPass::runOnOperation() {
  bool changed = false;
  module = getOperation();

  ARTS_DEBUG_HEADER(DbPass);
  ARTS_DEBUG_REGION(module.dump(););

  assert(AM && "ArtsAnalysisManager must be provided externally");

  /// Export JSON if requested
  if (this->exportJson)
    exportToJson();

  /// Global dimensionality reduction at allocation level
  // bool dimReduced = reduceAllocDimensionality();
  // changed |= dimReduced;

  // /// Per-acquire slice tightening (offset narrowing and splitting)
  // bool tightened = tightenAcquireSlices();
  // changed |= tightened;

  /// bool ctpChanged = convertToParameters();
  /// changed |= ctpChanged;
  /// if (ctpChanged) {
  ///   module.walk([&](func::FuncOp func) {
  ///     (void)dbAnalysis.invalidateGraph(func);
  ///     (void)dbAnalysis.getOrCreateGraph(func);
  ///   });
  /// }

  /// bool ctpChanged = convertToParameters();
  /// changed |= prefetchOverlap();
  /// changed |= bufferReuse();
  /// changed |= hoistAllocs();
  /// changed |= fuseAccesses();
  /// changed |= inlineAllocs();

  if (!changed) {
    ARTS_INFO("No changes made to the module");
  }

  ARTS_DEBUG_FOOTER(DbPass);
  ARTS_DEBUG_REGION(module.dump(););
}

//===----------------------------------------------------------------------===//
// Shrink DBs
// Shrink datablocks to the minimum required size based on acquires/releases.
// The create dbpass creates extremely fine-grained DB acquires (e.g., size-1
// along the last dimension), which can saturate the network. The correct way
// to coarsen is to normalize DB granularity at allocation time (DbAllocOp),
// e.g., switch from NxMxP DBs to N DBs of size MxP, then rewrite
// acquires/releases, EDT operands accordingly and DB uses.
//
// Example:
//   Before:
//     %db = arts.db_alloc %ptr sizes(%cN,%cM,%cP)
//     acquires over (i,j,k)
//   After (conceptual):
//     for i in 0..N-1:
//       %db_i = arts.db_alloc %ptr_i sizes(%cM,%cP)
//     acquires only over (j,k) on %db_i chosen by i
//===----------------------------------------------------------------------===//
bool DbPass::shrinkDb() {
  ARTS_INFO("Shrinking DBs");
  bool changed = false;

  module.walk([&](func::FuncOp func) {
    auto &graph = AM->getDbGraph(func);

    /// Iterate directly over acquire nodes from the graph
    graph.forEachAcquireNode([&](DbAcquireNode *acqNode) {
      auto acqOp = acqNode->getDbAcquireOp();
      const DbAcquireInfo &info = acqNode->getInfo();
      const uint64_t rank = info.getRank();

      /// Early exit: skip if rank is 0 (already scalar)
      if (rank == 0)
        return;

      /// Compute invariant indices for this acquire
      SmallVector<Value, 4> invariantIndices =
          acqNode->computeInvariantIndices();
      const uint64_t pinnedCount = invariantIndices.size();

      /// Early exit: skip if there are no invariant indices
      if (pinnedCount == 0)
        return;

      /// Early exit: skip if all dimensions are pinned (no shrinking possible)
      if (pinnedCount > rank)
        return;

      /// Early exit: skip if current indices already equal the analyzed indices
      /// set
      if (arts::equalRange(acqOp.getIndices(), invariantIndices))
        return;

      /// Early exit: skip if no memory accesses to update
      const auto &memoryAccesses = acqNode->getMemoryAccesses();
      if (memoryAccesses.empty())
        return;

      OpBuilder builder(acqOp.getContext());
      builder.setInsertionPoint(acqOp);
      Location loc = acqOp.getLoc();

      /// Compute tail offsets/sizes after pinning invariant indices.
      SmallVector<Value, 4> tailOffsets;
      SmallVector<Value, 4> tailSizes;

      const uint64_t tailRank = rank - pinnedCount;

      tailOffsets.reserve(tailRank);
      tailSizes.reserve(tailRank);

      for (size_t d = 0; d < tailRank; ++d) {
        size_t srcDim = pinnedCount + d;
        Value off =
            (srcDim < info.offsets.size()) ? info.offsets[srcDim] : Value();
        tailOffsets.push_back(off);

        Value sz;
        if (tailRank == 1)
          sz = builder.create<arith::ConstantIndexOp>(loc, 1);
        else
          sz = (srcDim < info.sizes.size()) ? info.sizes[srcDim] : Value();
        tailSizes.push_back(sz);
      }

      /// Create shrunkDb acquire operation
      auto shrunkDb = builder.create<arts::DbAcquireOp>(
          loc, acqOp.getMode(), acqOp.getSourceGuid(), acqOp.getSourcePtr(),
          SmallVector<Value>(invariantIndices.begin(), invariantIndices.end()),
          SmallVector<Value>(tailOffsets.begin(), tailOffsets.end()),
          SmallVector<Value>(tailSizes.begin(), tailSizes.end()));

      /// Update memory accesses using the stored accesses from DbAcquireNode
      const bool isScalar = (tailRank == 0);

      /// Lambda to update memory access indices
      auto updateMemoryAccess = [&](auto memOp) {
        SmallVector<Value, 4> idxs;
        auto oldIdxs = memOp.getIndices();

        if (isScalar) {
          /// Scalar memref - no indices needed
          memOp.getIndicesMutable().assign(idxs);
          /// Update memref type to scalar
          auto elementType = memOp.getMemref()
                                 .getType()
                                 .template cast<MemRefType>()
                                 .getElementType();
          auto newType = MemRefType::get({}, elementType);
          memOp.getMemrefMutable().assign({memOp.getMemref()});
          memOp.getMemref().setType(newType);
          return;
        }

        /// Non-scalar memref - adjust indices
        idxs.reserve(tailRank);
        for (size_t d = pinnedCount; d < oldIdxs.size(); ++d) {
          Value idx = oldIdxs[d];
          size_t pos = d - pinnedCount;

          /// Subtract offset if non-zero to avoid useless arithmetic
          if (pos < tailRank && arts::isNonZeroIndex(tailOffsets[pos])) {
            idx = builder.create<arith::SubIOp>(memOp.getLoc(), idx,
                                                tailOffsets[pos]);
          }
          idxs.push_back(idx);
        }
        memOp.getIndicesMutable().assign(idxs);
      };

      /// Apply the update to all memory accesses
      for (Operation *acc : memoryAccesses) {
        if (auto load = dyn_cast<memref::LoadOp>(acc))
          updateMemoryAccess(load);
        else if (auto store = dyn_cast<memref::StoreOp>(acc))
          updateMemoryAccess(store);
      }

      /// Replace the old acquire operation with the new shrunkDb operation
      acqOp.getGuid().replaceAllUsesWith(shrunkDb.getGuid());
      acqOp.getPtr().replaceAllUsesWith(shrunkDb.getPtr());

      /// Remove the old acquire operation
      acqOp.erase();

      ARTS_DEBUG(" - Shrunk DB: " << shrunkDb);
      changed = true;
    });
  });

  return changed;
}

// Prefetch/overlap heuristic
// - Hoist acquires upward to start transfers earlier, but never above the
//   last defining op of their operands (preserves dominance and semantics).
// - Delay releases to just after the last in-block user to avoid premature
//   invalidation and reduce thrash while still keeping scope minimal.
// Scope: intra-block only (no CFG restructuring). Conservative and local.
// Example (before → after):
//   %x = compute
//   %a = arts.db_acquire %db offsets(%o0,%o1) sizes(%s0,%s1)
//   use(%a, %x)
//   arts.db_release %a
// becomes (if uses of %a occur later)
//   %x = compute
//   %a = arts.db_acquire %db offsets(%o0,%o1) sizes(%s0,%s1)
//   use(%a, %x)
//   ...
//   arts.db_release %a            /// moved after last use
bool DbPass::prefetchOverlap() {
  bool changed = false;
  /// Operate within blocks to preserve simple dominance and avoid CFG changes
  module.walk([&](func::FuncOp func) {
    for (Block &block : func.getBody().getBlocks()) {
      /// Build a position index for stable ordering comparisons
      llvm::DenseMap<Operation *, unsigned> pos;
      unsigned i = 0;
      for (Operation &op : block)
        pos[&op] = i++;

      /// 1) Hoist acquires
      /// Move each acquire as early as possible after the last operand def in
      /// this block. If operands are block arguments or defined above, the
      /// acquire goes to the block front.
      SmallVector<DbAcquireOp, 16> acquires;
      for (Operation &op : block)
        if (auto acq = dyn_cast<DbAcquireOp>(&op))
          acquires.push_back(acq);

      for (DbAcquireOp acq : acquires) {
        Operation *op = acq.getOperation();
        unsigned cur = pos[op];
        Operation *anchor = nullptr;
        for (Value v : op->getOperands()) {
          if (Operation *def = v.getDefiningOp()) {
            if (def->getBlock() == &block) {
              if (!anchor || pos[def] > pos[anchor])
                anchor = def;
            }
          }
        }
        unsigned targetPos = anchor ? (pos[anchor] + 1) : 0;
        if (targetPos < cur) {
          if (anchor)
            op->moveAfter(anchor);
          else
            op->moveBefore(&block.front());
          changed = true;
        }
      }

      /// Rebuild positions after moves
      pos.clear();
      i = 0;
      for (Operation &op : block)
        pos[&op] = i++;

      /// 2) Delay releases
      /// Move each release as late as possible within the same block: just
      /// after the last use of its operand(s). Skip if already in the best
      /// spot.
      SmallVector<DbReleaseOp, 16> releases;
      for (Operation &op : block)
        if (auto rel = dyn_cast<DbReleaseOp>(&op))
          releases.push_back(rel);

      for (DbReleaseOp rel : releases) {
        Operation *op = rel.getOperation();
        unsigned cur = pos[op];
        unsigned lastUsePos = cur;
        /// Consider all operands; typically the acquired view
        for (Value v : op->getOperands()) {
          for (OpOperand &use : v.getUses()) {
            Operation *user = use.getOwner();
            if (user->getBlock() != &block)
              continue;
            if (user == op)
              continue;
            lastUsePos = std::max(lastUsePos, pos[user]);
          }
        }
        if (lastUsePos > cur) {
          Operation *lastUser = nullptr;
          for (Operation &it : block)
            if (pos[&it] == lastUsePos) {
              lastUser = &it;
              break;
            }
          if (lastUser) {
            op->moveAfter(lastUser);
            changed = true;
          }
        }
      }
    }
  });
  return changed;
}

//===----------------------------------------------------------------------===//
// Reduce allocation dimensionality (conservative, all-children agreement)
//===----------------------------------------------------------------------===//
bool DbPass::reduceAllocDimensionality() {
  bool changed = false;

  module.walk([&](func::FuncOp func) {
    DbGraph &graph = AM->getDbGraph(func);

    graph.forEachAllocNode([&](DbAllocNode *allocNode) {
      DbAllocOp oldAlloc = allocNode->getDbAllocOp();
      if (!oldAlloc)
        return;

      /// Gather acquire nodes for this allocation
      SmallVector<DbAcquireNode *, 8> acquires;
      allocNode->forEachChildNode([&](NodeBase *child) {
        if (auto *acq = dyn_cast<DbAcquireNode>(child))
          acquires.push_back(acq);
      });
      if (acquires.empty())
        return;

      /// Determine the maximum K such that all child acquires agree on a common
      /// constant index for each of the first K dimensions (w.r.t. the BASE
      /// memref dimensions). For each dimension d, for each acquire we take the
      /// source-pinned index when present (from op.getIndices()), otherwise we
      /// consult the first invariant indices of the acquired view discovered by
      /// the analysis; all must be constant and equal.
      size_t K = 0;
      while (true) {
        bool canDropThisDim = true;
        int64_t commonVal = 0;
        bool commonValInit = false;
        for (DbAcquireNode *acqNode : acquires) {
          DbAcquireOp acqOp = acqNode->getDbAcquireOp();
          auto acqIdx = acqOp.getIndices();
          size_t numPinnedAtSource = (size_t)acqIdx.size();

          Value idxV;
          if (K < numPinnedAtSource) {
            idxV = acqIdx[K];
          } else {
            SmallVector<Value, 4> inv = acqNode->computeInvariantIndices();
            size_t pos = K - numPinnedAtSource;
            if (pos >= inv.size()) {
              canDropThisDim = false;
              break;
            }
            idxV = inv[pos];
          }

          int64_t cst = 0;
          if (!arts::getConstantIndex(idxV, cst)) {
            canDropThisDim = false;
            break;
          }
          if (!commonValInit) {
            commonVal = cst;
            commonValInit = true;
          } else if (commonVal != cst) {
            canDropThisDim = false;
            break;
          }
        }
        if (!canDropThisDim)
          break;
        ++K;
      }

      if (K == 0)
        return;

      /// We will only proceed if every acquire already pins at least K source
      /// dimensions explicitly in its `indices`. This guarantees we don't need
      /// to change intra-EDT memory accesses (we avoid removing dimensions of
      /// the acquired view). If any acquire has fewer than K explicit indices,
      /// bail out conservatively for this allocation.
      for (DbAcquireNode *acqNode : acquires) {
        if (acqNode->getDbAcquireOp().getIndices().size() < K)
          return; /// bail for this alloc
      }

      /// Build the replacement DbAllocOp with first K sizes dropped. We drop
      /// the address operand to decouple from original shape and rely on sizes.
      SmallVector<Value> oldSizes(oldAlloc.getSizes().begin(),
                                  oldAlloc.getSizes().end());
      if (K > oldSizes.size())
        return;
      SmallVector<Value> newSizes(oldSizes.begin() + (ptrdiff_t)K,
                                  oldSizes.end());
      SmallVector<Value> payload(oldAlloc.getPayloadSizes().begin(),
                                 oldAlloc.getPayloadSizes().end());

      OpBuilder builder(oldAlloc);
      Location loc = oldAlloc.getLoc();
      /// Route is not semantically used here; reuse a zero constant as in
      /// normalization.
      Value zeroRoute = builder.create<arith::ConstantIntOp>(loc, 0, 32);
      DbAllocOp newAlloc = builder.create<DbAllocOp>(
          loc, oldAlloc.getMode(), zeroRoute, oldAlloc.getAllocType(),
          oldAlloc.getDbMode(), oldAlloc.getElementType(), newSizes, payload);

      /// Rebuild all child acquires: remove the first K indices. Offsets/sizes
      /// are kept the same (acquired view rank remains unchanged) because we
      /// restricted to acquires that already pinned the dropped dims.
      for (DbAcquireNode *acqNode : acquires) {
        DbAcquireOp oldAcq = acqNode->getDbAcquireOp();
        OpBuilder b(oldAcq);
        Location aloc = oldAcq.getLoc();

        /// Replace the dependency operand in the parent EDT first so block
        /// argument mapping stays consistent after we swap the acquire op.
        Value oldPtr = oldAcq.getPtr();
        Operation *edtOp = nullptr;
        unsigned operandIdx = 0;
        for (auto &use : oldPtr.getUses()) {
          if (isa<EdtOp>(use.getOwner())) {
            edtOp = use.getOwner();
            operandIdx = use.getOperandNumber();
            break;
          }
        }

        /// Trim indices by K (safe due to the guard above)
        SmallVector<Value> idxTrim;
        auto oldIdx = oldAcq.getIndices();
        idxTrim.append(oldIdx.begin() + (ptrdiff_t)K, oldIdx.end());

        SmallVector<Value> offs(oldAcq.getOffsets().begin(),
                                oldAcq.getOffsets().end());
        SmallVector<Value> szs(oldAcq.getSizes().begin(),
                               oldAcq.getSizes().end());

        DbAcquireOp newAcq =
            b.create<DbAcquireOp>(aloc, oldAcq.getMode(), newAlloc.getGuid(),
                                  newAlloc.getPtr(), idxTrim, offs, szs);

        /// Swap the dependency operand in the EDT to the new acquire's ptr.
        if (edtOp)
          edtOp->setOperand(operandIdx, newAcq.getPtr());

        /// Redirect guid/ptr SSA uses and erase old acquire.
        oldAcq.getGuid().replaceAllUsesWith(newAcq.getGuid());
        oldAcq.getPtr().replaceAllUsesWith(newAcq.getPtr());
        oldAcq.erase();
      }

      /// Replace old alloc uses and erase if now dead.
      oldAlloc.getGuid().replaceAllUsesWith(newAlloc.getGuid());
      oldAlloc.getPtr().replaceAllUsesWith(newAlloc.getPtr());
      if (oldAlloc->use_empty())
        oldAlloc.erase();

      ARTS_INFO("Reduced alloc dimensionality by " << (int)K << " for alloc "
                                                   << newAlloc);
      changed = true;
    });
  });

  return changed;
}

//===----------------------------------------------------------------------===//
// Tighten acquire slices (offset tightening / splitting)
//===----------------------------------------------------------------------===//
bool DbPass::tightenAcquireSlices() {
  bool changed = false;

  module.walk([&](func::FuncOp func) {
    DbGraph &graph = AM->getDbGraph(func);

    /// Helper: consider values as invariant if defined outside the EDT region
    /// OR computed inside from only invariant operands via arithmetic/cast ops.
    auto isEdtInvariantDeep = [&](Region &region, Value v,
                                  auto &&isEdtInvariantDeepRef) -> bool {
      if (arts::isInvariantInEdt(region, v))
        return true;
      if (v.getDefiningOp<arith::ConstantIndexOp>())
        return true;
      Operation *def = v.getDefiningOp();
      if (!def)
        return false;
      /// Allow simple arithmetic/casts that don't introduce iteration-varying
      /// behavior when their inputs are invariant.
      if (isa<arith::AddIOp, arith::SubIOp, arith::MulIOp, arith::IndexCastOp,
              arith::ExtSIOp, arith::ExtUIOp, arith::TruncIOp>(def)) {
        for (Value opnd : def->getOperands()) {
          if (!isEdtInvariantDeepRef(region, opnd, isEdtInvariantDeepRef))
            return false;
        }
        return true;
      }
      return false;
    };

    graph.forEachAcquireNode([&](DbAcquireNode *acqNode) {
      DbAcquireOp acqOp = acqNode->getDbAcquireOp();
      /// Only handle rank-1 acquires for now
      if (acqNode->getInfo().getRank() != 1)
        return;

      /// Collect invariant indices used in the EDT for this acquired view.
      SmallVector<Operation *, 16> accesses = acqNode->getMemoryAccesses();
      if (accesses.empty())
        return;

      SmallVector<Value, 4> idxValues;
      DenseMap<Operation *, Value> opToIdx;
      bool anyNonInvariant = false;

      for (Operation *acc : accesses) {
        Value mem;
        ValueRange idxs;
        if (auto ld = dyn_cast<memref::LoadOp>(acc)) {
          mem = ld.getMemref();
          idxs = ld.getIndices();
        } else if (auto st = dyn_cast<memref::StoreOp>(acc)) {
          mem = st.getMemref();
          idxs = st.getIndices();
        } else {
          continue;
        }
        /// Ensure this access is inside the EDT and refers to the acquired arg
        if (!acqNode->getEdtUser().getBody().isAncestor(acc->getParentRegion()))
          continue;

        if (idxs.size() != 1) {
          anyNonInvariant = true;
          break;
        }
        Value iv = idxs[0];
        /// Accept any value that is invariant within the EDT (block arg or any
        /// expr derived solely from invariant values via simple arith/casts).
        if (!isEdtInvariantDeep(acqNode->getEdtUser().getBody(), iv,
                                isEdtInvariantDeep)) {
          anyNonInvariant = true;
          break;
        }
        opToIdx[acc] = iv;
        if (llvm::find(idxValues, iv) == idxValues.end())
          idxValues.push_back(iv);
      }

      if (anyNonInvariant)
        return;

      /// Case A: Single element used -> tighten to size 1 at that offset
      if (idxValues.size() == 1) {
        Value elem = idxValues.front();
        OpBuilder b(acqOp);
        Location loc = acqOp.getLoc();

        /// Compute new offset = baseOffset + elem
        Value newOff;
        SmallVector<Value, 4> oldOffs(acqOp.getOffsets().begin(),
                                      acqOp.getOffsets().end());
        if (!oldOffs.empty()) {
          Value base = oldOffs[0];
          if (arts::isNonZeroIndex(base))
            newOff = b.create<arith::AddIOp>(loc, base, elem);
          else
            newOff = elem;
        } else {
          newOff = elem;
        }

        SmallVector<Value> newOffs;
        newOffs.push_back(newOff);
        SmallVector<Value> newSizes;
        newSizes.push_back(b.create<arith::ConstantIndexOp>(loc, 1));

        DbAcquireOp newAcq = b.create<DbAcquireOp>(
            loc, acqOp.getMode(), acqOp.getSourceGuid(), acqOp.getSourcePtr(),
            SmallVector<Value>{}, newOffs, newSizes);

        /// Update the EDT dependency operand
        Value oldPtr = acqOp.getPtr();
        Operation *edtOp = nullptr;
        unsigned operandIdx = 0;
        for (auto &use : oldPtr.getUses()) {
          if (isa<EdtOp>(use.getOwner())) {
            edtOp = use.getOwner();
            operandIdx = use.getOperandNumber();
            break;
          }
        }
        if (edtOp)
          edtOp->setOperand(operandIdx, newAcq.getPtr());

        /// Retarget memory accesses: idx -> 0
        for (auto &kv : opToIdx) {
          Operation *acc = kv.first;
          if (auto ld = dyn_cast<memref::LoadOp>(acc)) {
            OpBuilder lb(ld);
            SmallVector<Value, 1> memrefV{acqNode->getUseInEdt()};
            ld.getMemrefMutable().assign(memrefV);
            SmallVector<Value, 1> idxV{
                lb.create<arith::ConstantIndexOp>(ld.getLoc(), 0)};
            ld.getIndicesMutable().assign(idxV);
          } else if (auto st = dyn_cast<memref::StoreOp>(acc)) {
            OpBuilder sb(st);
            SmallVector<Value, 1> memrefV{acqNode->getUseInEdt()};
            st.getMemrefMutable().assign(memrefV);
            SmallVector<Value, 1> idxV{
                sb.create<arith::ConstantIndexOp>(st.getLoc(), 0)};
            st.getIndicesMutable().assign(idxV);
          }
        }

        /// Swap SSA uses and erase old acquire
        acqOp.getGuid().replaceAllUsesWith(newAcq.getGuid());
        acqOp.getPtr().replaceAllUsesWith(newAcq.getPtr());
        acqOp.erase();
        changed = true;
        return;
      }

      /// Case B: Multiple distinct elements -> split acquire into N size-1
      if (idxValues.size() > 1) {
        /// Verify all accesses belong to the discovered set
        for (auto &kv : opToIdx) {
          if (llvm::find(idxValues, kv.second) == idxValues.end())
            return;
        }

        OpBuilder b(acqOp);
        Location loc = acqOp.getLoc();

        auto makeAcqFor = [&](Value idx) -> DbAcquireOp {
          Value baseOff = nullptr;
          SmallVector<Value, 4> oldOffs(acqOp.getOffsets().begin(),
                                        acqOp.getOffsets().end());
          if (!oldOffs.empty())
            baseOff = oldOffs[0];
          Value newOff;
          if (baseOff && arts::isNonZeroIndex(baseOff))
            newOff = b.create<arith::AddIOp>(loc, baseOff, idx);
          else
            newOff = idx;
          SmallVector<Value> offs;
          offs.push_back(newOff);
          SmallVector<Value> szs;
          szs.push_back(b.create<arith::ConstantIndexOp>(loc, 1));
          return b.create<DbAcquireOp>(
              loc, acqOp.getMode(), acqOp.getSourceGuid(), acqOp.getSourcePtr(),
              SmallVector<Value>{}, offs, szs);
        };

        SmallVector<DbAcquireOp, 8> newAcqs;
        newAcqs.reserve(idxValues.size());
        for (Value iv : idxValues)
          newAcqs.push_back(makeAcqFor(iv));

        /// Update EDT operands: replace old operand with pointers of all new
        /// acquires
        EdtOp edt = acqNode->getEdtUser();
        Operation *edtOp = edt.getOperation();
        Value oldPtr = acqOp.getPtr();
        unsigned operandIdx = 0;
        for (auto &use : oldPtr.getUses()) {
          if (use.getOwner() == edtOp) {
            operandIdx = use.getOperandNumber();
            break;
          }
        }
        SmallVector<Value, 8> newOperands;
        if (Value route = edt.getRoute())
          newOperands.push_back(route);
        auto deps = edt.getDependenciesAsVector();
        unsigned depsBegin = (edt.getRoute() ? 1u : 0u);
        unsigned depIdx = operandIdx - depsBegin;
        for (unsigned i = 0; i < deps.size(); ++i) {
          if (i == depIdx) {
            for (DbAcquireOp na : newAcqs)
              newOperands.push_back(na.getPtr());
          } else {
            newOperands.push_back(deps[i]);
          }
        }
        edtOp->setOperands(newOperands);

        /// Insert (N-1) new block arguments after the original one.
        Block &blk = edt.getBody().front();
        BlockArgument firstArg = acqNode->getUseInEdt().cast<BlockArgument>();
        SmallVector<Value, 8> newArgs;
        newArgs.push_back(firstArg);
        for (size_t i = 1; i < newAcqs.size(); ++i) {
          Value a = blk.insertArgument(firstArg.getArgNumber() + (unsigned)i,
                                       firstArg.getType(), loc);
          newArgs.push_back(a);
        }

        /// Retarget memory accesses to the corresponding arg and index 0
        for (auto &kv : opToIdx) {
          Operation *acc = kv.first;
          Value sel = kv.second;
          /// Find arg index for this sel in idxValues
          ptrdiff_t which = llvm::find(idxValues, sel) - idxValues.begin();
          if (which < 0 || (size_t)which >= newArgs.size())
            continue;
          Value targetArg = newArgs[(size_t)which];
          if (auto ld = dyn_cast<memref::LoadOp>(acc)) {
            OpBuilder lb(ld);
            SmallVector<Value, 1> memrefV{targetArg};
            ld.getMemrefMutable().assign(memrefV);
            SmallVector<Value, 1> idxV{
                lb.create<arith::ConstantIndexOp>(ld.getLoc(), 0)};
            ld.getIndicesMutable().assign(idxV);
          } else if (auto st = dyn_cast<memref::StoreOp>(acc)) {
            OpBuilder sb(st);
            SmallVector<Value, 1> memrefV{targetArg};
            st.getMemrefMutable().assign(memrefV);
            SmallVector<Value, 1> idxV{
                sb.create<arith::ConstantIndexOp>(st.getLoc(), 0)};
            st.getIndicesMutable().assign(idxV);
          }
        }

        /// Ensure single releases for all new args, remove any duplicates first
        SmallVector<Operation *, 8> toErase;
        for (Operation &op : blk) {
          if (auto rel = dyn_cast<DbReleaseOp>(&op)) {
            Value src = rel.getSource();
            if (llvm::find(newArgs, src) != newArgs.end())
              toErase.push_back(rel.getOperation());
          }
        }
        for (Operation *op : toErase)
          op->erase();
        OpBuilder rb(blk.getTerminator());
        for (Value a : newArgs)
          rb.create<DbReleaseOp>(loc, a);

        /// Remove old acquire
        acqOp.erase();
        changed = true;
      }
    });
  });

  return changed;
}

//===----------------------------------------------------------------------===//
// Convert trivial DBs to parameters
// When a DB is effectively read-only and 1D, turn it into a parameter
// to avoid runtime traffic.
// Example:
//   %db = arts.db_alloc %ptr sizes(%cN)
//   %a  = arts.db_acquire %db ...
//   load %a[...]
//   --> convert to a function/block argument replacing %db/%a
//===----------------------------------------------------------------------===//
bool DbPass::convertToParameters() {
  bool changed = false;
  /// module.walk([&](func::FuncOp func) {
  ///   auto graph = dbAnalysis.getOrCreateGraph(func);
  ///   graph.forEachAllocNode([&](DbAllocNode *allocNode) {
  ///     bool hasSingleSize = (allocNode->getSizes().size() == 1);
  ///     bool isOnlyReader = (allocNode->getAcquireNodesSize() > 0 &&
  ///     allocNode->getReleaseNodesSize() == 0);

  ///     if (!hasSingleSize || !isOnlyReader) return;

  ///     LLVM_DEBUG(dbgs() << "- Converting DB to parameter: " <<
  ///     allocNode->getDbAllocOp() << "\n");
  ///     allocNode->forEachChildNode([&](NodeBase *child) {
  ///       if (auto acquireNode = dyn_cast<DbAcquireNode>(child)) {
  ///         Operation *acquireOp = acquireNode->getOp();
  ///         acquireOp->getResult(0).replaceAllUsesWith(allocNode->getPtr());
  ///         acquireOp->erase();
  ///       }
  ///     });
  ///     if (allocNode->getDbAllocOp().use_empty()) {
  ///       allocNode->getDbAllocOp().erase();
  ///     }
  ///     changed = true;
  ///   });
  /// });
  return changed;
}

// Dead DB elimination
// Responsibility: remove DbAllocOps with no associated acquires/releases and
// no remaining uses. Uses DbGraph to count children; then checks use_empty.
// Example (before → after):
//   %db = arts.db_alloc %ptr sizes(%cN,%cM)
// no uses, no acquires/releases
// becomes: (alloc erased)
bool DbPass::deadDbElimination() {
  bool changed = false;

  module.walk([&](func::FuncOp func) {
    auto &graph = AM->getDbGraph(func);
    SmallVector<DbAllocNode *> deadAllocs;
    graph.forEachAllocNode([&](DbAllocNode *allocNode) {
      if (allocNode->getAcquireNodesSize() == 0 &&
          allocNode->getReleaseNodesSize() == 0) {
        deadAllocs.push_back(allocNode);
      }
    });
    for (auto *allocNode : deadAllocs) {
      auto dbOp = allocNode->getDbAllocOp();
      if (dbOp && dbOp->use_empty()) {
        dbOp.erase();
        changed = true;
      }
    }
  });
  return changed;
}

// Buffer reuse across non-overlapping lifetimes (disabled scaffolding)
// Intent: reuse storage of two DBs when lifetimes do not overlap and aliasing
// is safe. Requires alias and lifetime queries. Kept as future work.
bool DbPass::bufferReuse() {
  bool changed = false;
  /// module.walk([&](func::FuncOp func) {
  ///   auto graph = dbAnalysis.getOrCreateGraph(func);
  ///   SmallVector<std::pair<DbAllocNode *, DbAllocNode *>> reusePairs;
  ///   graph.forEachAllocNode([&](DbAllocNode *alloc1) {
  ///     graph.forEachAllocNode([&](DbAllocNode *alloc2) {
  ///       if (alloc1 != alloc2 &&
  ///       !graph.isAllocReachable(alloc1->getDbAllocOp(),
  ///       alloc2->getDbAllocOp()) &&
  ///           !graph.isAllocReachable(alloc2->getDbAllocOp(),
  ///           alloc1->getDbAllocOp()) &&
  ///           dbAnalysis.getAliasAnalysis()->mayAlias(alloc1->getDbAllocOp(),
  ///           alloc2->getDbAllocOp())) {
  ///         reusePairs.push_back({alloc1, alloc2});
  ///       }
  ///     });
  ///   });
  ///   for (auto &[alloc1, alloc2] : reusePairs) {
  ///     alloc2->forEachChildNode([&](NodeBase *child) {
  ///       if (auto acquire = dyn_cast<DbAcquireNode>(child)) {
  ///         acquire->getOp()->setOperand(0,
  ///         alloc1->getDbAllocOp().getResult(0));
  ///       } else if (auto release = dyn_cast<DbReleaseNode>(child)) {
  ///         release->getOp()->setOperand(0,
  ///         alloc1->getDbAllocOp().getResult(0));
  ///       }
  ///     });
  ///     alloc2->getDbAllocOp().erase();
  ///     changed = true;
  ///   }
  /// });
  return changed;
}

// Hoist loop-invariant allocations (disabled scaffolding)
// Intent: move DbAllocOps out of loops when invariant to reduce allocation
// overhead. Requires LoopAnalysis integration. Kept as future work.
bool DbPass::hoistAllocs() {
  bool changed = false;
  /// module.walk([&](func::FuncOp func) {
  ///   auto graph = dbAnalysis.getOrCreateGraph(func);
  ///   auto *loopAnalysis = dbAnalysis.getLoopAnalysis();
  ///   graph.forEachAllocNode([&](DbAllocNode *allocNode) {
  ///     Operation *allocOp = allocNode->getOp();
  ///     if (auto enclosingLoop = loopAnalysis->getEnclosingLoop(allocOp)) {
  ///       if (loopAnalysis->isLoopInvariant(allocOp, enclosingLoop)) {
  ///         allocOp->moveBefore(enclosingLoop);
  ///         changed = true;
  ///       }
  ///     }
  ///   });
  /// });
  return changed;
}

// Fuse adjacent identical accesses
// Responsibility: within a block, fuse back-to-back identical DbAcquireOps on
// the same parent alloc (replace later result with earlier), and similarly
// collapse consecutive identical DbReleaseOps on the same source.
// Scope: adjacency-only; does not reorder or cross block boundaries.
// Example (before → after):
//   %a0 = arts.db_acquire %db indices(%i,%j) sizes(%c4,%c4)
//   %a1 = arts.db_acquire %db indices(%i,%j) sizes(%c4,%c4)
//   use(%a1)
// becomes
//   %a0 = arts.db_acquire %db indices(%i,%j) sizes(%c4,%c4)
//   use(%a0)
bool DbPass::fuseAccesses() {
  bool changed = false;

  module.walk([&](func::FuncOp func) {
    auto &graph = AM->getDbGraph(func);
    if (graph.isEmpty())
      return;
    for (Block &block : func.getBody().getBlocks()) {
      DbAcquireOp prevAcq = nullptr;
      DbReleaseOp prevRel = nullptr;
      for (Operation &op : block) {
        if (auto acq = dyn_cast<DbAcquireOp>(&op)) {
          if (prevAcq && prevAcq->getNextNode() == &op) {
            auto parentPrev = graph.getParentAlloc(prevAcq.getOperation());
            auto parentNow = graph.getParentAlloc(acq.getOperation());
            if (parentPrev && parentNow && parentPrev == parentNow &&
                prevAcq.getMode() == acq.getMode() &&
                arts::equalRange(prevAcq.getIndices(), acq.getIndices()) &&
                arts::equalRange(prevAcq.getOffsets(), acq.getOffsets()) &&
                arts::equalRange(prevAcq.getSizes(), acq.getSizes())) {
              acq.getPtr().replaceAllUsesWith(prevAcq.getPtr());
              acq.erase();
              changed = true;
              /// Do not update prevAcq; continue chaining
              continue;
            }
          }
          prevAcq = acq;
          prevRel = nullptr;
          continue;
        }
        if (auto rel = dyn_cast<DbReleaseOp>(&op)) {
          if (prevRel && prevRel->getNextNode() == &op) {
            /// Fuse identical consecutive releases of the same source.
            if (prevRel.getSource() && rel.getSource() &&
                prevRel.getSource() == rel.getSource()) {
              rel.erase();
              changed = true;
              continue;
            }
          }
          prevRel = rel;
          prevAcq = nullptr;
          continue;
        }
        prevAcq = nullptr;
        prevRel = nullptr;
      }
    }
  });
  return changed;
}

void DbPass::exportToJson() {
  /// Create output directory if it doesn't exist
  std::string outputDir = "output";
  std::error_code ec = llvm::sys::fs::create_directories(outputDir);
  if (ec) {
    ARTS_ERROR("Failed to create output directory: " << ec.message());
    return;
  }

  module.walk([&](func::FuncOp func) {
    DbGraph &graph = AM->getDbGraph(func);
    if (graph.isEmpty())
      return;

    /// Create filename based on function name
    std::string filename = outputDir + "/" + func.getName().str() + "_db.json";

    /// Open file for writing
    std::error_code ec;
    llvm::raw_fd_ostream file(filename, ec);
    if (ec) {
      ARTS_ERROR("Failed to open file " << filename << ": " << ec.message());
      return;
    }

    /// Export the graph to JSON
    graph.exportToJson(file, true);
    file.close();

    ARTS_INFO("Exported DB analysis for function '" << func.getName() << "' to "
                                                    << filename);
  });
}

///===----------------------------------------------------------------------===///
/// Pass creation
///===----------------------------------------------------------------------===///
namespace mlir {
namespace arts {
std::unique_ptr<Pass> createDbPass(ArtsAnalysisManager *AM, bool exportJson) {
  return std::make_unique<DbPass>(AM, exportJson);
}
} // namespace arts
} // namespace mlir
