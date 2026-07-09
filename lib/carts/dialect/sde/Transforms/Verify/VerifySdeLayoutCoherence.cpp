///==========================================================================///
/// File: VerifySdeLayoutCoherence.cpp
///
/// Verifier for committed `sde.array_layout` coherence with MU types and CU
/// access indices. Fails closed when typed layout facts, rank-expanded MU
/// storage, or CU index rewrites disagree.
///==========================================================================///

#include "carts/dialect/sde/Analysis/SdeAccessRelation.h"
#include "carts/dialect/sde/Analysis/SdeCommVolumeCost.h"
#include "carts/dialect/sde/Analysis/SdeDependence.h"
#include "carts/dialect/sde/Analysis/SdeMemoryLegality.h"
#include "carts/dialect/sde/Analysis/SdeAnalysisUtils.h"
#include "carts/dialect/sde/IR/SdeDialect.h"
#include "carts/dialect/sde/Transforms/Passes.h"
#include "carts/dialect/sde/Utils/MuLayout.h"

namespace mlir::carts::sde {
#define GEN_PASS_DEF_VERIFYSDELAYOUTCOHERENCE
#include "carts/dialect/sde/Transforms/Passes.h.inc"
} // namespace mlir::carts::sde

#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Pass/Pass.h"
#include "llvm/ADT/STLExtras.h"
#include <functional>
#include <numeric>

using namespace mlir;
using namespace mlir::carts;

namespace {

static bool sameI64Shape(ArrayRef<int64_t> lhs, ArrayRef<int64_t> rhs) {
  return lhs.size() == rhs.size() &&
         std::equal(lhs.begin(), lhs.end(), rhs.begin());
}

static bool ownerDimsMatch(ArrayRef<int64_t> layoutOwners,
                           ArrayRef<unsigned> recoveredOwners) {
  if (layoutOwners.size() != recoveredOwners.size())
    return false;
  SmallVector<int64_t, 4> recovered;
  for (unsigned dim : recoveredOwners)
    recovered.push_back(static_cast<int64_t>(dim));
  llvm::sort(recovered);
  SmallVector<int64_t, 4> layout(layoutOwners.begin(), layoutOwners.end());
  llvm::sort(layout);
  return layout == recovered;
}

static bool blockShapeMatchesOwnerLayout(ArrayRef<int64_t> layoutBlockShape,
                                         ArrayRef<int64_t> layoutOwnerDims,
                                         const sde::RecoveredMuPhysicalLayout &recovered) {
  for (auto [slot, ownerDim] : llvm::enumerate(recovered.ownerDims)) {
    if (slot >= recovered.physicalBlockShape.size())
      return false;
    int64_t expected = recovered.physicalBlockShape[slot];
    if (ownerDim >= layoutBlockShape.size())
      return false;
    if (layoutBlockShape[ownerDim] != expected)
      return false;
  }
  return ownerDimsMatch(layoutOwnerDims, recovered.ownerDims);
}

static bool memrefAccessUsesCommittedRank(Value memref, Operation *accessOp) {
  if (auto load = dyn_cast<memref::LoadOp>(accessOp))
    return load.getMemref() == memref;
  if (auto store = dyn_cast<memref::StoreOp>(accessOp))
    return store.getMemref() == memref;
  if (auto subview = dyn_cast<memref::SubViewOp>(accessOp))
    return subview.getSource() == memref;
  return false;
}

static LogicalResult verifyCuIndexRewrites(sde::SdeSuIterateOp su,
                                           sde::SdeArrayLayoutRootOp root,
                                           MemRefType committedType) {
  Block *computeBlock = sde::getSuIterateComputeBlock(su);
  if (!computeBlock)
    return success();

  const int64_t expectedRank = committedType.getRank();
  for (Operation &op : *computeBlock) {
    if (!memrefAccessUsesCommittedRank(root.getRoot(), &op))
      continue;
    unsigned indexCount = 0;
    if (auto load = dyn_cast<memref::LoadOp>(&op))
      indexCount = load.getIndices().size();
    else if (auto store = dyn_cast<memref::StoreOp>(&op))
      indexCount = store.getIndices().size();
    else if (auto subview = dyn_cast<memref::SubViewOp>(&op))
      indexCount = subview.getMixedOffsets().size();
    else
      continue;

    if (static_cast<int64_t>(indexCount) != expectedRank) {
      return op.emitOpError()
             << "index arity " << indexCount
             << " disagrees with committed MU rank " << expectedRank;
    }
  }
  return success();
}

struct VerifySdeLayoutCoherencePass
    : public sde::impl::VerifySdeLayoutCoherenceBase<
          VerifySdeLayoutCoherencePass> {
  void runOnOperation() override {
    (void)getAnalysis<sde::SdeAccessRelation>();
    (void)getAnalysis<sde::SdeCommVolumeCost>();
    (void)getAnalysis<sde::SdeDependence>();
    (void)getAnalysis<sde::SdeMemoryLegality>();

    ModuleOp module = getOperation();
    bool hasFailure = false;

    module.walk([&](sde::SdeSuIterateOp su) {
      if (su.getBody().empty())
        return;

      Block &entry = su.getBody().front();
      llvm::DenseMap<std::pair<int64_t, sde::SdeAccessMode>,
                     sde::SdeArrayLayoutRootOp>
          rootsByKey;
      for (sde::SdeArrayLayoutRootOp root :
           entry.getOps<sde::SdeArrayLayoutRootOp>()) {
        auto key = std::make_pair(root.getArrayId(), root.getMode());
        if (rootsByKey.contains(key)) {
          root.emitOpError()
              << "duplicate sde.array_layout_root for arrayId/mode";
          hasFailure = true;
          continue;
        }
        rootsByKey[key] = root;
      }

      for (sde::SdeArrayLayoutOp layout :
           entry.getOps<sde::SdeArrayLayoutOp>()) {
        auto key = std::make_pair(layout.getArrayId(), layout.getMode());
        auto rootIt = rootsByKey.find(key);
        if (rootIt == rootsByKey.end()) {
          layout.emitOpError()
              << "has no matching sde.array_layout_root provenance";
          hasFailure = true;
          continue;
        }

        sde::SdeArrayLayoutRootOp root = rootIt->second;
        auto muType = dyn_cast<MemRefType>(root.getRoot().getType());
        if (!muType)
          continue;

        ArrayRef<int64_t> logicalShape =
            layout.getLogicalShapeAttr().asArrayRef();
        ArrayRef<int64_t> blockShape = layout.getBlockShapeAttr().asArrayRef();
        ArrayRef<int64_t> ownerDims =
            layout.getOwnerDimsAttr()
                ? layout.getOwnerDimsAttr().asArrayRef()
                : ArrayRef<int64_t>{};

        if (std::optional<sde::RecoveredMuPhysicalLayout> recovered =
                sde::recoverMuPhysicalLayoutFromExpandedType(muType)) {
          if (!sameI64Shape(recovered->logicalShape, logicalShape)) {
            layout.emitOpError()
                << "logicalShape disagrees with rank-expanded MU logical shape";
            hasFailure = true;
          }
          if (!ownerDims.empty() &&
              !blockShapeMatchesOwnerLayout(blockShape, ownerDims, *recovered)) {
            layout.emitOpError()
                << "ownerDims/blockShape disagree with rank-expanded MU "
                   "physical layout";
            hasFailure = true;
          }
        } else if (muType.hasStaticShape()) {
          if (!sameI64Shape(muType.getShape(), logicalShape)) {
            layout.emitOpError()
                << "logicalShape disagrees with sde.mu_alloc static shape";
            hasFailure = true;
          }
          if (!ownerDims.empty()) {
            if (std::optional<sde::MuPhysicalLayout> resolved =
                    sde::resolveMuPhysicalLayout(muType, ownerDims,
                                                 blockShape)) {
              int64_t totalBlocks = std::accumulate(
                  resolved->blockCounts.begin(), resolved->blockCounts.end(),
                  int64_t{1}, std::multiplies<int64_t>());
              if (totalBlocks <= 1) {
                layout.emitOpError()
                    << "blockShape does not realize a non-trivial owner grid";
                hasFailure = true;
              }
            } else {
              layout.emitOpError()
                  << "ownerDims/blockShape are incompatible with MU type";
              hasFailure = true;
            }
          }
        }

        if (mlir::failed(verifyCuIndexRewrites(su, root, muType)))
          hasFailure = true;
      }
    });

    if (hasFailure)
      signalPassFailure();
  }
};

} // namespace

std::unique_ptr<Pass> mlir::carts::sde::createVerifySdeLayoutCoherencePass() {
  return std::make_unique<VerifySdeLayoutCoherencePass>();
}
