///==========================================================================///
/// File: MuLayoutRewriter.cpp
///
/// Implementation of the SDE MU-level rank-expansion rewriter (see
/// MuLayoutRewriter.h).
///==========================================================================///

#include "carts/dialect/sde/Utils/MuLayoutRewriter.h"
#include "carts/utils/ArrayAttrUtils.h"

#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/Utils/StaticValueUtils.h"
#include "mlir/IR/Operation.h"
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

bool isSingleOwnerBlockGridRealizable(SdeSuIterateOp si, MemRefType logicalType,
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
  if (!plan || !plan->isSingleContiguousOwner())
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

bool muRootHasUnsupportedUse(Value root) {
  for (Operation *user : root.getUsers()) {
    if (isa<memref::LoadOp, memref::StoreOp, memref::DeallocOp,
            SdeMuAccessWindowOp, SdeRedistOp>(user))
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
  if (!ownerVals || !blockVals || ownerVals->size() != 1)
    return std::nullopt;

  // Single-owner expanded form: muRank == originalRank + 1, with the
  // rank-length block-shape carrying the original rank.
  const unsigned muRank = muType.getRank();
  if (blockVals->size() + 1 != muRank)
    return std::nullopt; // flat / owner-length / multi-owner -> out of scope
  int64_t ownerDim = (*ownerVals)[0];
  if (ownerDim < 0 || static_cast<unsigned>(ownerDim) >= blockVals->size())
    return std::nullopt;

  ExpandedBlockGridMu out;
  out.ownerDim = static_cast<unsigned>(ownerDim);
  out.logicalRank = blockVals->size();
  out.blockExtent = (*blockVals)[ownerDim];
  out.gridCount = muType.getShape().front();
  return out;
}

std::optional<int64_t> findOwnerIterationExtent(SdeSuIterateOp si,
                                                int64_t blockExtent,
                                                int64_t gridExtent) {
  if (!si || blockExtent <= 0)
    return std::nullopt;
  std::optional<llvm::SmallVector<int64_t, 4>> halo =
      readI64ArrayAttr(si.getPhysicalHaloShapeAttr());
  for (auto it :
       llvm::enumerate(llvm::zip(si.getLowerBounds(), si.getUpperBounds()))) {
    unsigned dim = it.index();
    auto [lb, ub] = it.value();
    std::optional<int64_t> lbc = getConstantIntValue(lb);
    std::optional<int64_t> ubc = getConstantIntValue(ub);
    if (!lbc || !ubc)
      continue;
    int64_t extent = *ubc - *lbc;
    if (extent <= 0)
      continue;
    if ((extent + blockExtent - 1) / blockExtent == gridExtent)
      return extent;
    if (halo && dim < halo->size()) {
      int64_t widened = extent + 2 * (*halo)[dim];
      if (widened > 0 &&
          (widened + blockExtent - 1) / blockExtent == gridExtent)
        return widened;
    }
  }
  return std::nullopt;
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
    // Subview / cast / capture / escape: not yet supported by rank expansion.
    // Fail closed.
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

  for (memref::DeallocOp dealloc : deallocs)
    dealloc.erase();

  muAlloc.erase();
  return success();
}

} // namespace mlir::carts::sde
