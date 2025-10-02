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
#include <limits>
/// File operations
#include "mlir/Dialect/Utils/ReshapeOpsUtils.h"
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

  /// Adjust DB modes based on actual read/write accesses.
  bool adjustDbModes();

  /// DBs with single size, and "in" mode, can be converted into parameters.
  bool convertToParameters();

  /// Remove unused DB allocations.
  bool deadDbElimination();

  /// Export DB analysis to JSON files in output/ directory.
  void exportToJson();

  /// Reduce DB dimensionality at allocation level
  bool reduceAllocDimensionality();

  /// Tighten acquire slices inside EDTs.
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

  /// Graph construction and analysis
  module.walk([&](func::FuncOp func) {
    DbGraph &graph = AM->getDbGraph(func);
    graph.build();

    /// Print analysis results for verification
    ARTS_DEBUG_REGION({
      llvm::outs() << "\n";
      graph.print(llvm::outs());
      llvm::outs() << "\n";
    });
  });

  /// Adjust DB modes based on dependencies and accesses
  changed |= adjustDbModes();
  ;

  /// Global dimensionality reduction at allocation level
  // changed |= reduceAllocDimensionality();

  // /// Per-acquire slice tightening (offset narrowing and splitting)
  // changed |= tightenAcquireSlices();

  // /// Convert trivial DBs to parameters
  // changed |= convertToParameters();

  // /// Dead DB elimination
  // changed |= deadDbElimination();

  if (changed)
    module.walk([&](func::FuncOp func) { AM->invalidateFunction(func); });
  else
    ARTS_INFO("No changes made to the module");

  ARTS_DEBUG_FOOTER(DbPass);
  ARTS_DEBUG_REGION(module.dump(););
}

//===----------------------------------------------------------------------===//
/// Adjust DB modes based on accesses.
/// Based on the collected loads and stores, adjust the DB mode to in, out, or
/// inout.
//===----------------------------------------------------------------------===//
bool DbPass::adjustDbModes() {
  bool changed = false;

  module.walk([&](func::FuncOp func) {
    DbGraph &graph = AM->getDbGraph(func);

    // First, adjust per-acquire modes
    graph.forEachAcquireNode([&](DbAcquireNode *acqNode) {
      bool hasLoads = !acqNode->getLoads().empty();
      bool hasStores = !acqNode->getStores().empty();

      DbAcquireOp acqOp = acqNode->getDbAcquireOp();
      ArtsMode newMode = ArtsMode::inout;
      if (hasLoads && hasStores)
        newMode = ArtsMode::inout;
      else if (hasStores)
        newMode = ArtsMode::out;
      else if (hasLoads)
        newMode = ArtsMode::in;

      if (newMode == acqOp.getMode())
        return;

      acqOp.setModeAttr(ArtsModeAttr::get(acqOp.getContext(), newMode));
      changed = true;
    });

    /// Then, adjust alloc dbMode - if all child acquires are in, then the alloc
    /// dbMode should be in otherwise it should be out if some child acquires
    /// are inout, then the alloc dbMode should be inout
    graph.forEachAllocNode([&](DbAllocNode *allocNode) {
      ArtsMode maxMode = ArtsMode::in;
      allocNode->forEachChildNode([&](NodeBase *child) {
        if (auto *acqNode = dyn_cast<DbAcquireNode>(child)) {
          ArtsMode mode = acqNode->getDbAcquireOp().getMode();
          if (mode == ArtsMode::in)
            maxMode = ArtsMode::in;
          else if (mode == ArtsMode::out)
            maxMode = ArtsMode::out;
          else if (mode == ArtsMode::inout)
            maxMode = ArtsMode::inout;
        }
      });

      DbAllocOp allocOp = allocNode->getDbAllocOp();
      ArtsMode currentDbMode = allocOp.getMode();
      if (currentDbMode == maxMode)
        return;

      allocOp.setModeAttr(ArtsModeAttr::get(allocOp.getContext(), maxMode));
      changed = true;
    });
  });

  return changed;
}

//===----------------------------------------------------------------------===//
/// Reduce allocation dimensionality
///
/// If, for a given `arts.db_alloc` and for every child acquire of this
/// allocation, the first K leading dimensions are pinned to the same
/// index values across the entire EDT body, then those K dimensions
/// can be dropped from the allocation itself and moved to the payload.
/// Making the allocation and acquires more coarse-grained, and reducing
/// network traffic.
//===----------------------------------------------------------------------===//
bool DbPass::reduceAllocDimensionality() {
  bool changed = false;

  module.walk([&](func::FuncOp func) {
    DbGraph &graph = AM->getDbGraph(func);

    graph.forEachAllocNode([&](DbAllocNode *allocNode) {
      DbAllocOp oldAlloc = allocNode->getDbAllocOp();
      assert(oldAlloc && "Alloc node must have a DbAllocOp");

      /// We currently require the datablock to be described purely through its
      /// `sizes` so that we can reshape the pointer type when dimensions are
      /// dropped. Allocations tied to an explicit address are skipped for now.
      // if (oldAlloc.getAddress())
      //   return;

      /// Gather acquire nodes for this allocation.
      SmallVector<DbAcquireNode *, 8> acquires;
      allocNode->forEachChildNode([&](NodeBase *child) {
        if (auto *acq = dyn_cast<DbAcquireNode>(child))
          acquires.push_back(acq);
      });
      if (acquires.empty())
        return;

      /// The number of dimension we can drop is bounded by the minimum number
      /// of explicit indices supplied by every acquire.
      size_t minPinned = std::numeric_limits<size_t>::max();
      for (DbAcquireNode *acqNode : acquires)
        minPinned = std::min(
            minPinned, (size_t)acqNode->getDbAcquireOp().getIndices().size());
      if (minPinned == 0 || minPinned == std::numeric_limits<size_t>::max())
        return;

      auto valuesEquivalent = [&](Value a, Value b) {
        if (a == b)
          return true;
        int64_t ca = 0, cb = 0;
        if (arts::getConstantIndex(a, ca) && arts::getConstantIndex(b, cb))
          return ca == cb;
        return false;
      };

      size_t dropDims = 0;
      for (; dropDims < minPinned; ++dropDims) {
        Value reference =
            acquires.front()->getDbAcquireOp().getIndices()[dropDims];
        bool allMatch = true;
        for (DbAcquireNode *acqNode : acquires) {
          Value idx = acqNode->getDbAcquireOp().getIndices()[dropDims];
          if (!valuesEquivalent(reference, idx)) {
            allMatch = false;
            break;
          }
        }
        if (!allMatch)
          break;
      }

      if (dropDims == 0)
        return;

      SmallVector<Value> oldSizes(oldAlloc.getSizes().begin(),
                                  oldAlloc.getSizes().end());
      if (dropDims > oldSizes.size())
        return;

      SmallVector<Value> oldPayload(oldAlloc.getPayloadSizes().begin(),
                                    oldAlloc.getPayloadSizes().end());

      SmallVector<Value> newSizes(oldSizes.begin() + dropDims, oldSizes.end());
      SmallVector<Value> newPayload;
      newPayload.reserve(dropDims + oldPayload.size());
      newPayload.append(oldSizes.begin(), oldSizes.begin() + dropDims);
      newPayload.append(oldPayload.begin(), oldPayload.end());

      if (newSizes.empty())
        return;

      OpBuilder builder(oldAlloc);
      Location loc = oldAlloc.getLoc();
      Value newAddress;
      if (Value oldAddress = oldAlloc.getAddress()) {
        auto addrType = dyn_cast<MemRefType>(oldAddress.getType());
        if (!addrType || static_cast<size_t>(addrType.getRank()) <= dropDims)
          return;

        unsigned rank = addrType.getRank();
        SmallVector<OpFoldResult> offsets, sizes, strides;
        offsets.reserve(rank);
        sizes.reserve(rank);
        strides.reserve(rank);

        auto idxAttr = [&](int64_t v) { return builder.getIndexAttr(v); };

        for (unsigned dim = 0; dim < rank; ++dim) {
          if (dim < dropDims) {
            int64_t cst = 0;
            if (!arts::getConstantIndex(
                    acquires.front()->getDbAcquireOp().getIndices()[dim], cst))
              return;
            offsets.push_back(idxAttr(cst));
            sizes.push_back(idxAttr(1));
          } else {
            offsets.push_back(idxAttr(0));
            Value dimVal = builder.create<memref::DimOp>(loc, oldAddress, dim);
            sizes.push_back(dimVal);
          }
          strides.push_back(idxAttr(1));
        }

        auto subview = builder.create<memref::SubViewOp>(
            loc, oldAddress, offsets, sizes, strides);

        unsigned subRank = subview.getType().getRank();
        if (subRank <= dropDims)
          return;

        SmallVector<ReassociationIndices> reassoc;
        ReassociationIndices firstGroup;
        for (unsigned i = 0; i <= dropDims; ++i)
          firstGroup.push_back(i);
        reassoc.push_back(firstGroup);
        for (unsigned i = dropDims + 1; i < subRank; ++i)
          reassoc.push_back({static_cast<int64_t>(i)});

        auto collapsed =
            builder.create<memref::CollapseShapeOp>(loc, subview, reassoc);
        newAddress = collapsed.getResult();
      }

      DbAllocOp newAlloc;
      if (newAddress)
        newAlloc = builder.create<DbAllocOp>(
            loc, oldAlloc.getMode(), oldAlloc.getRoute(),
            oldAlloc.getAllocType(), oldAlloc.getDbMode(), newAddress, newSizes,
            newPayload);
      else
        newAlloc = builder.create<DbAllocOp>(
            loc, oldAlloc.getMode(), oldAlloc.getRoute(),
            oldAlloc.getAllocType(), oldAlloc.getDbMode(),
            oldAlloc.getElementType(), newSizes, newPayload);

      for (DbAcquireNode *acqNode : acquires) {
        DbAcquireOp oldAcq = acqNode->getDbAcquireOp();
        OpBuilder acqBuilder(oldAcq);
        Location aloc = oldAcq.getLoc();

        SmallVector<Value> newIdx(oldAcq.getIndices().begin() + dropDims,
                                  oldAcq.getIndices().end());
        SmallVector<Value> offs(oldAcq.getOffsets().begin(),
                                oldAcq.getOffsets().end());
        SmallVector<Value> szs(oldAcq.getSizes().begin(),
                               oldAcq.getSizes().end());

        DbAcquireOp newAcq = acqBuilder.create<DbAcquireOp>(
            aloc, oldAcq.getMode(), newAlloc.getGuid(), newAlloc.getPtr(),
            newIdx, offs, szs);

        oldAcq.getGuid().replaceAllUsesWith(newAcq.getGuid());
        oldAcq.getPtr().replaceAllUsesWith(newAcq.getPtr());
        oldAcq.erase();
      }

      oldAlloc.getGuid().replaceAllUsesWith(newAlloc.getGuid());
      oldAlloc.getPtr().replaceAllUsesWith(newAlloc.getPtr());
      oldAlloc.erase();

      ARTS_INFO("Reduced alloc dimensionality by "
                << (int)dropDims << " for alloc " << newAlloc);
      changed = true;
    });
  });

  return changed;
}

//===----------------------------------------------------------------------===//
/// Tighten acquire slices
///
/// Optimization A (offset tightening): For acquires, if all accesses within
/// the EDT target a single invariant index tuple, rewrite the acquire to
/// `sizes=[1,...]` with `offsets=[base+idx,...]` and retarget load/store
/// indices by subtracting the chosen offset (becomes [0,...]).
///
/// Optimization B (split by N elements): For acquires with exactly N distinct
/// invariant index tuples accessed in the EDT (and no dynamic indices), split
/// the single acquire into N size-1 acquires (one per tuple), update the EDT
/// dependency list and insert new block arguments for the additional acquires,
/// then retarget loads/stores to the corresponding block argument with index
/// [0,...]. The original releases are replaced by N releases.
//===----------------------------------------------------------------------===//
bool DbPass::tightenAcquireSlices() {
  bool changed = false;

  // module.walk([&](func::FuncOp func) {
  //   DbGraph &graph = AM->getDbGraph(func);

  //   /// Helper: consider values as invariant if defined outside the EDT
  //   region
  //   /// OR computed inside from only invariant operands via arithmetic/cast
  //   ops. auto isEdtInvariantDeep = [&](Region &region, Value v,
  //                                 auto &&isEdtInvariantDeepRef) -> bool {
  //     if (arts::isInvariantInEdt(region, v))
  //       return true;
  //     if (v.getDefiningOp<arith::ConstantIndexOp>())
  //       return true;
  //     Operation *def = v.getDefiningOp();
  //     if (!def)
  //       return false;
  //     /// Allow simple arithmetic/casts that don't introduce
  //     iteration-varying
  //     /// behavior when their inputs are invariant.
  //     if (isa<arith::AddIOp, arith::SubIOp, arith::MulIOp,
  //     arith::IndexCastOp,
  //             arith::ExtSIOp, arith::ExtUIOp, arith::TruncIOp>(def)) {
  //       for (Value opnd : def->getOperands()) {
  //         if (!isEdtInvariantDeepRef(region, opnd, isEdtInvariantDeepRef))
  //           return false;
  //       }
  //       return true;
  //     }
  //     return false;
  //   };

  //   graph.forEachAcquireNode([&](DbAcquireNode *acqNode) {
  //     DbAcquireOp acqOp = acqNode->getDbAcquireOp();
  //     size_t rank = acqNode->getInfo().getRank();
  //     if (rank == 0)
  //       return;

  //     /// Collect invariant index tuples used in the EDT for this acquired
  //     view. SmallVector<Operation *, 16> accesses =
  //     acqNode->getMemoryAccesses(); if (accesses.empty())
  //       return;

  //     DenseMap<Operation *, SmallVector<Value>> accToTuple;
  //     SmallVector<SmallVector<Value>> uniqueTuples;
  //     bool anyNonInvariant = false;

  //     Region &edtRegion = acqNode->getEdtUser().getBody();
  //     Value edtArg = acqNode->getUseInEdt();

  //     for (Operation *acc : accesses) {
  //       Value mem;
  //       ValueRange idxs;
  //       if (auto ld = dyn_cast<memref::LoadOp>(acc)) {
  //         mem = ld.getMemref();
  //         idxs = ld.getIndices();
  //       } else if (auto st = dyn_cast<memref::StoreOp>(acc)) {
  //         mem = st.getMemref();
  //         idxs = st.getIndices();
  //       } else {
  //         continue;
  //       }
  //       /// Ensure this access is inside the EDT and refers to the acquired
  //       arg if (!edtRegion.isAncestor(acc->getParentRegion()) || mem !=
  //       edtArg) {
  //         continue;
  //       }

  //       if (idxs.size() != rank) {
  //         anyNonInvariant = true;
  //         break;
  //       }

  //       SmallVector<Value> tuple(idxs.begin(), idxs.end());
  //       bool allInvariant = true;
  //       for (Value iv : tuple) {
  //         if (!isEdtInvariantDeep(edtRegion, iv, isEdtInvariantDeep)) {
  //           allInvariant = false;
  //           break;
  //         }
  //       }
  //       if (!allInvariant) {
  //         anyNonInvariant = true;
  //         break;
  //       }

  //       accToTuple[acc] = tuple;

  //       bool found = false;
  //       for (const auto &u : uniqueTuples) {
  //         if (u == tuple) {
  //           found = true;
  //           break;
  //         }
  //       }
  //       if (!found)
  //         uniqueTuples.push_back(tuple);
  //     }

  //     if (anyNonInvariant)
  //       return;

  //     /// Case A: Single tuple used -> tighten to sizes all 1 at that offset
  //     if (uniqueTuples.size() == 1) {
  //       const SmallVector<Value> &theTuple = uniqueTuples.front();
  //       OpBuilder b(acqOp);
  //       Location loc = acqOp.getLoc();

  //       /// Compute new offsets = baseOffsets + theTuple
  //       SmallVector<Value> newOffs;
  //       auto oldOffs = acqOp.getOffsets();
  //       if (oldOffs.empty()) {
  //         newOffs.append(theTuple.begin(), theTuple.end());
  //       } else {
  //         if (oldOffs.size() != rank)
  //           return;
  //         for (size_t d = 0; d < rank; ++d) {
  //           Value base = oldOffs[d];
  //           Value addend = theTuple[d];
  //           if (arts::isZeroIndex(addend)) {
  //             newOffs.push_back(base);
  //             continue;
  //           }
  //           if (arts::isZeroIndex(base)) {
  //             newOffs.push_back(addend);
  //             continue;
  //           }
  //           newOffs.push_back(b.create<arith::AddIOp>(loc, base, addend));
  //         }
  //       }

  //       Value one = b.create<arith::ConstantIndexOp>(loc, 1);
  //       SmallVector<Value> newSizes(rank, one);

  //       DbAcquireOp newAcq = b.create<DbAcquireOp>(
  //           loc, acqOp.getMode(), acqOp.getSourceGuid(),
  //           acqOp.getSourcePtr(), acqOp.getIndices(), newOffs, newSizes);

  //       /// Update the EDT dependency operand
  //       Value oldPtr = acqOp.getPtr();
  //       Operation *edtOp = nullptr;
  //       unsigned operandIdx = 0;
  //       for (auto &use : oldPtr.getUses()) {
  //         if (isa<EdtOp>(use.getOwner())) {
  //           edtOp = use.getOwner();
  //           operandIdx = use.getOperandNumber();
  //           break;
  //         }
  //       }
  //       if (edtOp)
  //         edtOp->setOperand(operandIdx, newAcq.getPtr());

  //       /// Retarget memory accesses: indices -> all 0
  //       Value zero = b.create<arith::ConstantIndexOp>(loc, 0);
  //       SmallVector<Value> zeroIdx(rank, zero);
  //       for (auto &kv : accToTuple) {
  //         Operation *acc = kv.first;
  //         if (auto ld = dyn_cast<memref::LoadOp>(acc)) {
  //           OpBuilder lb(ld);
  //           ld.getMemrefMutable().assign(edtArg);
  //           ld.getIndicesMutable().assign(zeroIdx);
  //         } else if (auto st = dyn_cast<memref::StoreOp>(acc)) {
  //           OpBuilder sb(st);
  //           st.getMemrefMutable().assign(edtArg);
  //           st.getIndicesMutable().assign(zeroIdx);
  //         }
  //       }

  //       /// Swap SSA uses and erase old acquire
  //       acqOp.getGuid().replaceAllUsesWith(newAcq.getGuid());
  //       acqOp.getPtr().replaceAllUsesWith(newAcq.getPtr());
  //       acqOp.erase();
  //       changed = true;
  //       return;
  //     }

  //     /// Case B: Multiple distinct tuples -> split acquire into N all-1
  //     if (uniqueTuples.size() > 1) {
  //       OpBuilder b(acqOp);
  //       Location loc = acqOp.getLoc();

  //       Value one = b.create<arith::ConstantIndexOp>(loc, 1);
  //       auto makeAcqFor = [&](const SmallVector<Value> &tuple) -> DbAcquireOp
  //       {
  //         SmallVector<Value> newOffs;
  //         auto oldOffs = acqOp.getOffsets();
  //         if (oldOffs.empty()) {
  //           newOffs.append(tuple.begin(), tuple.end());
  //         } else {
  //           if (oldOffs.size() != rank)
  //             return nullptr;
  //           for (size_t d = 0; d < rank; ++d) {
  //             Value base = oldOffs[d];
  //             Value addend = tuple[d];
  //             if (arts::isZeroIndex(addend)) {
  //               newOffs.push_back(base);
  //               continue;
  //             }
  //             if (arts::isZeroIndex(base)) {
  //               newOffs.push_back(addend);
  //               continue;
  //             }
  //             newOffs.push_back(b.create<arith::AddIOp>(loc, base, addend));
  //           }
  //         }
  //         SmallVector<Value> newSizes(rank, one);
  //         return b.create<DbAcquireOp>(
  //             loc, acqOp.getMode(), acqOp.getSourceGuid(),
  //             acqOp.getSourcePtr(), acqOp.getIndices(), newOffs, newSizes);
  //       };

  //       SmallVector<DbAcquireOp, 8> newAcqs;
  //       for (const auto &tuple : uniqueTuples) {
  //         DbAcquireOp na = makeAcqFor(tuple);
  //         if (!na)
  //           return;
  //         newAcqs.push_back(na);
  //       }

  //       /// Update EDT operands: replace old operand with pointers of all new
  //       /// acquires
  //       EdtOp edt = acqNode->getEdtUser();
  //       Operation *edtOp = edt.getOperation();
  //       Value oldPtr = acqOp.getPtr();
  //       unsigned operandIdx = 0;
  //       for (auto &use : oldPtr.getUses()) {
  //         if (use.getOwner() == edtOp) {
  //           operandIdx = use.getOperandNumber();
  //           break;
  //         }
  //       }
  //       SmallVector<Value, 8> newOperands;
  //       if (Value route = edt.getRoute())
  //         newOperands.push_back(route);
  //       auto deps = edt.getDependenciesAsVector();
  //       unsigned depsBegin = (edt.getRoute() ? 1u : 0u);
  //       unsigned depIdx = operandIdx - depsBegin;
  //       for (unsigned i = 0; i < deps.size(); ++i) {
  //         if (i == depIdx) {
  //           for (DbAcquireOp na : newAcqs)
  //             newOperands.push_back(na.getPtr());
  //         } else {
  //           newOperands.push_back(deps[i]);
  //         }
  //       }
  //       edtOp->setOperands(newOperands);

  //       /// Insert (N-1) new block arguments after the original one.
  //       Block &blk = edt.getBody().front();
  //       BlockArgument firstArg =
  //       acqNode->getUseInEdt().cast<BlockArgument>(); SmallVector<Value, 8>
  //       newArgs; newArgs.push_back(firstArg); for (size_t i = 1; i <
  //       newAcqs.size(); ++i) {
  //         Value a = blk.insertArgument(firstArg.getArgNumber() + (unsigned)i,
  //                                      firstArg.getType(), loc);
  //         newArgs.push_back(a);
  //       }

  //       /// Retarget memory accesses to the corresponding arg and indices all
  //       0 Value zero = b.create<arith::ConstantIndexOp>(loc, 0);
  //       SmallVector<Value> zeroIdx(rank, zero);
  //       for (auto &kv : accToTuple) {
  //         Operation *acc = kv.first;
  //         const SmallVector<Value> &sel = kv.second;
  //         auto it = llvm::find(uniqueTuples, sel);
  //         if (it == uniqueTuples.end())
  //           continue;
  //         size_t which = std::distance(uniqueTuples.begin(), it);
  //         Value targetArg = newArgs[which];
  //         if (auto ld = dyn_cast<memref::LoadOp>(acc)) {
  //           OpBuilder lb(ld);
  //           ld.getMemrefMutable().assign(targetArg);
  //           ld.getIndicesMutable().assign(zeroIdx);
  //         } else if (auto st = dyn_cast<memref::StoreOp>(acc)) {
  //           OpBuilder sb(st);
  //           st.getMemrefMutable().assign(targetArg);
  //           st.getIndicesMutable().assign(zeroIdx);
  //         }
  //       }

  //       /// Ensure single releases for all new args, remove any duplicates
  //       first SmallVector<Operation *, 8> toErase; for (Operation &op : blk)
  //       {
  //         if (auto rel = dyn_cast<DbReleaseOp>(&op)) {
  //           Value src = rel.getSource();
  //           if (llvm::find(newArgs, src) != newArgs.end())
  //             toErase.push_back(rel.getOperation());
  //         }
  //       }
  //       for (Operation *op : toErase)
  //         op->erase();
  //       OpBuilder rb(blk.getTerminator());
  //       for (Value a : newArgs)
  //         rb.create<DbReleaseOp>(loc, a);

  //       /// Remove old acquire
  //       acqOp.erase();
  //       changed = true;
  //     }
  //   });
  // });

  return changed;
}

//===----------------------------------------------------------------------===//
/// Convert trivial DBs to parameters
///
/// When a DB is effectively read-only and 1D, turn it into a parameter
/// to avoid runtime traffic.
/// Example:
///   %db = arts.db_alloc %ptr sizes(%cN)
///   %a  = arts.db_acquire %db ...
///   load %a[...]
///   --> convert to a function/block argument replacing %db/%a
//===----------------------------------------------------------------------===//
bool DbPass::convertToParameters() {
  bool changed = false;
  // module.walk([&](func::FuncOp func) {
  //   DbGraph &graph = AM->getDbGraph(func);
  //   graph.forEachAllocNode([&](DbAllocNode *allocNode) {
  //     bool hasSingleSize = (allocNode->getSizes().size() == 1);
  //     bool isOnlyReader = (allocNode->getAcquireNodesSize() > 0 &&
  //                          allocNode->getReleaseNodesSize() == 0);

  //     if (!hasSingleSize || !isOnlyReader)
  //       return;

  //     ARTS_DEBUG("- Converting DB to parameter: "
  //                       << allocNode->getDbAllocOp() << "\n");
  //     allocNode->forEachChildNode([&](NodeBase *child) {
  //       if (auto acquireNode = dyn_cast<DbAcquireNode>(child)) {
  //         DbAcquireOp acquireOp = acquireNode->getDbAcquireOp();
  //         acquireOp.getPtr().replaceAllUsesWith(allocNode->getPtr());
  //         acquireOp.erase();
  //       }
  //     });
  //     DbAllocOp allocOp = allocNode->getDbAllocOp();
  //     if (allocOp.getGuid().use_empty() && allocOp.getPtr().use_empty()) {
  //       allocOp.erase();
  //     }
  //     changed = true;
  //   });
  // });
  return changed;
}

//===----------------------------------------------------------------------===//
/// Dead DB elimination
///
/// Remove DbAllocOps with no associated acquires/releases and
/// no remaining uses. Uses DbGraph to count children; then checks use_empty.
/// Example (before → after):
///   %db = arts.db_alloc %ptr sizes(%cN,%cM)
/// no uses, no acquires/releases
/// becomes: (alloc erased)
//===----------------------------------------------------------------------===//
bool DbPass::deadDbElimination() {
  bool changed = false;

  module.walk([&](func::FuncOp func) {
    auto &graph = AM->getDbGraph(func);
    SmallVector<DbAllocNode *> deadAllocs;
    graph.forEachAllocNode([&](DbAllocNode *allocNode) {
      /// Check if allocation has no acquires (releases are connected via edges,
      /// not stored)
      if (allocNode->getAcquireNodesSize() == 0) {
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
