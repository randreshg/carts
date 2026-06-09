///==========================================================================///
/// File: MuLayoutRewriter.cpp
///
/// Implementation of the SDE MU-level rank-expansion rewriter (see
/// MuLayoutRewriter.h).
///==========================================================================///

#include "carts/dialect/sde/Utils/MuLayoutRewriter.h"
#include "carts/utils/ArrayAttrUtils.h"
#include "carts/utils/ValueAnalysis.h"

#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/Operation.h"
#include "polygeist/Ops.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallVector.h"

#include <memory>

using namespace mlir;
using namespace mlir::carts;

namespace mlir::carts::sde {

std::unique_ptr<MuAccessIndexer>
makeMuAccessIndexer(SdeStructuredClassification cls,
                    const MuPhysicalLayout &plan) {
  switch (cls) {
  case SdeStructuredClassification::stencil:
    return std::make_unique<MuStencilIndexer>(plan);
  case SdeStructuredClassification::elementwise:
  case SdeStructuredClassification::elementwise_pipeline:
    return std::make_unique<MuElementWiseIndexer>(plan);
  default:
    return std::make_unique<MuBlockIndexer>(plan);
  }
}

bool isBlockGridRealizable(SdeSuIterateOp si, MemRefType logicalType,
                           MuPhysicalLayout &out) {
  if (!si)
    return false;
  std::optional<SdeStructuredClassification> cls =
      si.getStructuredClassification();
  if (!cls)
    return false;
  switch (*cls) {
  case SdeStructuredClassification::elementwise:
  case SdeStructuredClassification::elementwise_pipeline:
  case SdeStructuredClassification::stencil:
    break;
  default:
    return false; // matmul / reduction -> conservative, no attr promise
  }

  std::optional<MuPhysicalLayout> plan =
      resolveMuPhysicalLayout(logicalType, si.getPhysicalOwnerDimsAttr(),
                              si.getPhysicalBlockShapeAttr());
  if (!plan)
    return false;
  out = *plan;
  return true;
}

llvm::SmallVector<Value, 6> MuBlockIndexer::localize(ValueRange logicalIndices,
                                                     OpBuilder &builder,
                                                     Location loc) const {
  // Owner dim -> committed block extent (constant index value, built lazily).
  llvm::SmallVector<std::optional<int64_t>, 4> ownerBlockForDim(
      plan.logicalRank());
  for (auto [slot, dim] : llvm::enumerate(plan.ownerDims))
    ownerBlockForDim[dim] = plan.blockExtents[slot];

  auto blockConst = [&](int64_t extent) -> Value {
    return arith::ConstantIndexOp::create(builder, loc, extent);
  };

  llvm::SmallVector<Value, 6> result;
  result.reserve(plan.expandedRank());

  // Prefix grid coordinates: gridCoord = idx / B, in committed owner order.
  for (auto [slot, dim] : llvm::enumerate(plan.ownerDims)) {
    Value g = logicalIndices[dim];
    Value b = blockConst(plan.blockExtents[slot]);
    result.push_back(arith::DivUIOp::create(builder, loc, g, b));
  }

  // Tile coordinates: owner dim -> idx % B, non-owner dim -> passthrough.
  for (unsigned d = 0; d < plan.logicalRank(); ++d) {
    if (ownerBlockForDim[d]) {
      Value g = logicalIndices[d];
      Value b = blockConst(*ownerBlockForDim[d]);
      result.push_back(arith::RemUIOp::create(builder, loc, g, b));
    } else {
      result.push_back(logicalIndices[d]);
    }
  }

  return result;
}

SdeSuIterateOp findCommittedBlockPlanWriter(SdeMuAllocOp muAlloc) {
  SdeSuIterateOp result;
  ArrayAttr ownerA, blockA;
  for (Operation *user : muAlloc.getMemref().getUsers()) {
    if (!isa<memref::LoadOp, memref::StoreOp>(user))
      continue;
    SdeSuIterateOp si = user->getParentOfType<SdeSuIterateOp>();
    while (si &&
           !(si.getPhysicalOwnerDimsAttr() && si.getPhysicalBlockShapeAttr()))
      si = si->getParentOfType<SdeSuIterateOp>();
    if (!si)
      continue;
    if (!result) {
      result = si;
      ownerA = si.getPhysicalOwnerDimsAttr();
      blockA = si.getPhysicalBlockShapeAttr();
    } else if (si.getPhysicalOwnerDimsAttr() != ownerA ||
               si.getPhysicalBlockShapeAttr() != blockA) {
      return SdeSuIterateOp(); // conflicting committed plans -> conservative
    }
  }
  return result;
}

// Rank expansion preserves the allocation base pointer. Pointer comparisons can
// be repointed; dereferencing pointer uses keep the MU conservative.
static bool isLayoutInvariantBasePointer(polygeist::Memref2PointerOp m2p) {
  for (Operation *user : m2p.getResult().getUsers())
    if (!isa<LLVM::ICmpOp>(user))
      return false;
  return true;
}

bool muRootHasUnsupportedUse(Value root) {
  for (Operation *user : root.getUsers()) {
    if (isa<memref::LoadOp, memref::StoreOp, memref::DeallocOp,
            SdeMuAccessWindowOp, SdeRedistOp>(user))
      continue;
    if (auto m2p = dyn_cast<polygeist::Memref2PointerOp>(user))
      if (isLayoutInvariantBasePointer(m2p))
        continue;
    return true;
  }
  return false;
}

std::optional<ExpandedBlockGridMu>
recognizeExpandedBlockGridMu(SdeSuIterateOp si, MemRefType muType) {
  if (!si || !muType)
    return std::nullopt;
  std::optional<llvm::SmallVector<int64_t, 4>> ownerVals =
      readI64ArrayAttr(si.getPhysicalOwnerDimsAttr());
  std::optional<llvm::SmallVector<int64_t, 4>> blockVals =
      readI64ArrayAttr(si.getPhysicalBlockShapeAttr());
  if (!ownerVals || blockVals == std::nullopt || ownerVals->empty())
    return std::nullopt;

  // Expanded form: K leading grid dims + L tile dims, with the rank-length
  // block-shape carrying the logical rank L.
  const unsigned numOwner = ownerVals->size();
  const unsigned logicalRank = blockVals->size();
  const unsigned muRank = muType.getRank();
  if (logicalRank + numOwner != muRank)
    return std::nullopt; // flat / owner-length / rank mismatch -> out of scope

  // Owner dims: unique, in [0, L), sorted ascending (the canonical order the
  // ND geometry emits its grid prefix in).
  llvm::SmallVector<unsigned, 4> ownerDims;
  ownerDims.reserve(numOwner);
  for (unsigned i = 0; i < numOwner; ++i) {
    int64_t od = (*ownerVals)[i];
    if (od < 0 || static_cast<unsigned>(od) >= logicalRank)
      return std::nullopt;
    if (i > 0 && static_cast<unsigned>(od) <= ownerDims.back())
      return std::nullopt; // not strictly ascending (catches dups + disorder)
    ownerDims.push_back(static_cast<unsigned>(od));
  }

  ExpandedBlockGridMu out;
  out.ownerDims = std::move(ownerDims);
  out.logicalRank = logicalRank;
  out.blockExtents.reserve(numOwner);
  out.gridCounts.reserve(numOwner);
  ArrayRef<int64_t> shape = muType.getShape();
  for (unsigned i = 0; i < numOwner; ++i) {
    out.blockExtents.push_back((*blockVals)[out.ownerDims[i]]);
    out.gridCounts.push_back(shape[i]); // leading K grid dims, owner order
  }
  return out;
}

std::optional<llvm::SmallVector<int64_t, 4>>
findOwnerIterationExtents(SdeSuIterateOp si, llvm::ArrayRef<int64_t> blockExtents,
                          llvm::ArrayRef<int64_t> gridCounts) {
  if (!si || blockExtents.size() != gridCounts.size() || blockExtents.empty())
    return std::nullopt;
  std::optional<llvm::SmallVector<int64_t, 4>> halo =
      readI64ArrayAttr(si.getPhysicalHaloShapeAttr());

  // Fold the committed iteration extents once (independent of the expanded
  // type) so each per-owner-dim proof is non-tautological.
  llvm::SmallVector<std::optional<int64_t>, 6> rawExtents;
  for (auto it :
       llvm::enumerate(llvm::zip(si.getLowerBounds(), si.getUpperBounds()))) {
    auto [lb, ub] = it.value();
    std::optional<int64_t> lbc = ValueAnalysis::tryFoldConstantIndex(lb);
    std::optional<int64_t> ubc = ValueAnalysis::tryFoldConstantIndex(ub);
    if (lbc && ubc)
      rawExtents.push_back(*ubc - *lbc);
    else
      rawExtents.push_back(std::nullopt);
  }

  // Match each owner dim's (block, grid) to a DISTINCT committed loop dim whose
  // extent (optionally halo-widened) ceilDivs to the grid count.
  const unsigned numOwner = blockExtents.size();
  llvm::SmallVector<int64_t, 4> result(numOwner, 0);
  llvm::SmallVector<bool, 6> used(rawExtents.size(), false);
  for (unsigned i = 0; i < numOwner; ++i) {
    int64_t blockExtent = blockExtents[i];
    int64_t gridExtent = gridCounts[i];
    if (blockExtent <= 0)
      return std::nullopt;
    bool matched = false;
    for (unsigned dim = 0; dim < rawExtents.size(); ++dim) {
      if (used[dim] || !rawExtents[dim])
        continue;
      int64_t extent = *rawExtents[dim];
      if (extent <= 0)
        continue;
      if ((extent + blockExtent - 1) / blockExtent == gridExtent) {
        result[i] = extent;
        used[dim] = true;
        matched = true;
        break;
      }
      if (halo && dim < halo->size()) {
        int64_t widened = extent + 2 * (*halo)[dim];
        if (widened > 0 &&
            (widened + blockExtent - 1) / blockExtent == gridExtent) {
          result[i] = widened;
          used[dim] = true;
          matched = true;
          break;
        }
      }
    }
    if (!matched)
      return std::nullopt; // no distinct committed extent yields this grid count
  }
  return result;
}

LogicalResult MuLayoutRewriter::apply(SdeMuAllocOp muAlloc) {
  Value oldMemref = muAlloc.getMemref();
  auto logicalType = dyn_cast<MemRefType>(oldMemref.getType());
  if (!logicalType ||
      logicalType.getRank() != static_cast<int64_t>(plan.logicalRank()))
    return failure();

  // Pre-scan: every use must be a direct memref.load / memref.store on the MU
  // root (logical-rank indexed), or a dealloc. Anything else -> fail closed,
  // no mutation.
  llvm::SmallVector<memref::LoadOp, 8> loads;
  llvm::SmallVector<memref::StoreOp, 8> stores;
  llvm::SmallVector<memref::DeallocOp, 2> deallocs;
  llvm::SmallVector<polygeist::Memref2PointerOp, 2> basePointers;
  for (OpOperand &use : oldMemref.getUses()) {
    Operation *user = use.getOwner();
    if (auto dealloc = dyn_cast<memref::DeallocOp>(user)) {
      deallocs.push_back(dealloc);
      continue;
    }
    if (auto load = dyn_cast<memref::LoadOp>(user)) {
      if (load.getMemRef() != oldMemref ||
          load.getIndices().size() != plan.logicalRank())
        return failure();
      loads.push_back(load);
      continue;
    }
    if (auto store = dyn_cast<memref::StoreOp>(user)) {
      if (store.getMemRef() != oldMemref ||
          store.getIndices().size() != plan.logicalRank())
        return failure();
      stores.push_back(store);
      continue;
    }
    // Only layout-invariant base-pointer checks can follow the expanded memref.
    if (auto m2p = dyn_cast<polygeist::Memref2PointerOp>(user)) {
      if (isLayoutInvariantBasePointer(m2p)) {
        basePointers.push_back(m2p);
        continue;
      }
    }
    return failure();
  }

  MemRefType expandedType = buildExpandedMuType(logicalType, plan);

  // Build the new expanded mu_alloc in place of the old one (fully static ->
  // no dynamic sizes).
  OpBuilder builder(muAlloc);
  auto newAlloc = SdeMuAllocOp::create(builder, muAlloc.getLoc(), expandedType,
                                       ValueRange{});
  Value newMemref = newAlloc.getMemref();

  // Rewrite reads.
  for (memref::LoadOp load : loads) {
    OpBuilder b(load);
    llvm::SmallVector<Value, 6> idx =
        indexer.localize(load.getIndices(), b, load.getLoc());
    auto newLoad =
        memref::LoadOp::create(b, load.getLoc(), newMemref, ValueRange(idx));
    load.getResult().replaceAllUsesWith(newLoad.getResult());
    load.erase();
  }

  // Rewrite writes.
  for (memref::StoreOp store : stores) {
    OpBuilder b(store);
    llvm::SmallVector<Value, 6> idx =
        indexer.localize(store.getIndices(), b, store.getLoc());
    memref::StoreOp::create(b, store.getLoc(), store.getValueToStore(),
                            newMemref, ValueRange(idx));
    store.erase();
  }

  // Preserve allocation identity checks after replacing the MU root.
  for (polygeist::Memref2PointerOp m2p : basePointers) {
    OpBuilder b(m2p);
    auto repl = polygeist::Memref2PointerOp::create(b, m2p.getLoc(),
                                                    m2p.getType(), newMemref);
    m2p.getResult().replaceAllUsesWith(repl.getResult());
    m2p.erase();
  }

  for (memref::DeallocOp dealloc : deallocs)
    dealloc.erase();

  muAlloc.erase();
  return success();
}

} // namespace mlir::carts::sde
