///==========================================================================
/// File: DataBlock.cpp
///==========================================================================

/// Arts
#include "ArtsPassDetails.h"
#include "arts/Analysis/DataBlockAnalysis.h"
#include "arts/ArtsDialect.h"
#include "arts/Passes/ArtsPasses.h"
/// Dialects
#include "arts/Utils/ArtsUtils.h"
#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/AffineMap.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/MLIRContext.h"
#include "mlir/Support/LLVM.h"
#include "polygeist/Ops.h"
/// Other
#include "mlir/IR/Dominance.h"
#include "mlir/IR/OpDefinition.h"
#include "mlir/IR/Operation.h"
#include "mlir/Pass/Pass.h"
/// Debug
#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include <cstdint>

#define DEBUG_TYPE "datablock"
#define line "-----------------------------------------\n"
#define dbgs() (llvm::dbgs())
#define DBGS() (dbgs() << "[" DEBUG_TYPE "] ")

using namespace mlir;
using namespace mlir::func;
using namespace mlir::arith;
using namespace mlir::polygeist;
using namespace mlir::arts;

namespace {

/// Returns true if any memory effect of dbOp may alias the given value.
static bool ptrMayAlias(DataBlockOp dbOp, Value val) {
  /// Get aliasing effects.
  if (auto memEff = dyn_cast<MemoryEffectOpInterface>(dbOp.getOperation())) {
    SmallVector<MemoryEffects::EffectInstance, 2> effects;
    memEff.getEffects(effects);
    return llvm::any_of(effects, [&](const MemoryEffects::EffectInstance &eA) {
      return mayAlias(eA, val);
    });
  }
  return false;
}

/// Returns the affine map associated with the operation, if any.
static std::optional<AffineMap> getAffineMap(Operation *op) {
  /// Check for affine load, store, or datablock.
  if (auto loadOp = dyn_cast<affine::AffineLoadOp>(op))
    return loadOp.getAffineMap();
  if (auto storeOp = dyn_cast<affine::AffineStoreOp>(op))
    return storeOp.getAffineMap();
  if (auto dbOp = dyn_cast<arts::DataBlockOp>(op))
    return dbOp.getAffineMap();
  return std::nullopt;
}

/// Sets the affine map attribute for the operation.
static void setAffineMap(Operation *op, AffineMap map) {
  auto mapAttr = AffineMapAttr::get(map);
  if (auto loadOp = dyn_cast<affine::AffineLoadOp>(op))
    loadOp->setAttr("map", mapAttr);
  else if (auto storeOp = dyn_cast<affine::AffineStoreOp>(op))
    storeOp->setAttr("map", mapAttr);
  else if (auto dbOp = dyn_cast<arts::DataBlockOp>(op))
    dbOp->setAttr("affineMap", mapAttr);
}

template <typename OpType> static bool isZeroIndex(OpType &op) {
  if (auto affineMap = getAffineMap(op.getOperation())) {
    auto affineResults = affineMap->getResults();
    if (!affineResults.empty() && affineResults.back() == 0)
      return true;
    else {
      SmallVector<Value, 4> pinnedIndices = op.getIndices();
      if (!pinnedIndices.empty() && pinnedIndices.back() != 0)
        return true;
    }
  }
  return false;
}

/// Returns true if the given operation uses the datablock.
template <typename OpTy>
static bool isDataBlockUsed(arts::DataBlockOp &dbOp, OpTy op) {
  /// Check valid operation.
  if (!op)
    return false;

  /// Check if the memref aliases the datablock.
  if (!ptrMayAlias(dbOp, op.getMemref()))
    return false;

  /// Get operation indices.
  const auto opIndices = op.getIndices();
  const auto dbIndices = dbOp.getIndices();

  /// The operation cannot have fewer indices than the datablock.
  const uint32_t dbIndicesSize = dbIndices.size();
  if (opIndices.size() < dbIndicesSize)
    return false;

  /// Get affine maps for both operations.
  std::optional<AffineMap> opAffineMap = getAffineMap(op.getOperation());
  std::optional<AffineMap> dbAffineMap = getAffineMap(dbOp.getOperation());

  /// If the datablock is affine, the op must be affine as well and their affine
  /// maps must match.
  if (dbAffineMap && opAffineMap) {
    /// Compare the results of the affine maps up to the number of results of
    /// the datablock.
    const uint32_t dbNumResults = isZeroIndex(dbOp)
                                      ? dbAffineMap->getNumResults() - 1
                                      : dbAffineMap->getNumResults();
    for (uint32_t i = 0; i < dbNumResults; ++i) {
      if (dbAffineMap->getResult(i) != opAffineMap->getResult(i))
        return false;
    }
  }
  /// If only one of the maps exists, they do not match.
  else if (dbAffineMap || opAffineMap) {
    return false;
  }

  /// Compare pinned indices.
  for (uint32_t i = 0; i < dbIndicesSize; ++i) {
    if (opIndices[i] != dbIndices[i])
      return false;
  }

  LLVM_DEBUG(dbgs() << "    - " << op << "\n");
  return true;
}

/// Adjusts the datablock to determine the appropriate pinned indices.
/// When a pinned index is removed, the affine map is adjusted accordingly.
/// For example, if a datablock is loaded with a pinned index of 0, we need
/// to check if the dependency is on the entire memref or a subview of it.
// static void adjustDataBlock(DataBlockOp &dbOp, Region &region) {
//   /// Lambda that returns true if the last index is zero.
//   auto isZeroIndex = [&](auto &op, bool drop = false) {
//     if (auto affineMap = getAffineMap(op.getOperation())) {
//       auto affineResults = affineMap->getResults();
//       if (!affineResults.empty() && affineResults.back() == 0) {
//         if (drop) {
//           auto newMap =
//               AffineMap::get(affineResults.size() - 1, 0,
//                              affineResults.drop_back(), op.getContext());
//           setAffineMap(op.getOperation(), newMap);
//         }
//         return true;
//       } else {
//         SmallVector<Value, 4> pinnedIndices = op.getIndices();
//         if (!pinnedIndices.empty() && pinnedIndices.back() != 0) {
//           if (drop) {
//             pinnedIndices.pop_back();
//             op.getIndicesMutable().assign(pinnedIndices);
//           }
//           return true;
//         }
//       }
//     }
//     return false;
//   };

//   /// We only care about datablocks whose last index is zero.
//   if (!isZeroIndex(dbOp))
//     return;

//   LLVM_DEBUG(dbgs() << line << "Adjusting datablock:\n  " << dbOp << "\n");

//   MLIRContext *ctx = dbOp.getContext();
//   OpBuilder builder(ctx);

//   /// Process memref load/store and affine load/store ops.
//   auto processOp = [&](auto op) -> void {
//     LLVM_DEBUG(dbgs() << "    - Processing: " << op << "\n");
//     /// If it is equal, ignore
//     if (isDataBlockUsed(dbOp, op))
//       return;

//     /// If the last index is not zero, do nothing.
//     if (!isZeroIndex(op))
//       return;

//     /// Check if the operation uses the datablock, but ignores the last
//     index. if (!isDataBlockUsed(dbOp, op, true))
//       return;

//     LLVM_DEBUG(dbgs() << "     - It is used\n");

//     /// Drop the last index.
//     /// Remove the last (pinned) index from the datablock.
//     isZeroIndex(dbOp, true);
//     LLVM_DEBUG(dbgs() << "     - Dropped last index\n"
//                       << "       - " << dbOp << "\n");
//   };

//   region.walk([&](Operation *op) {
//     if (auto load = dyn_cast<memref::LoadOp>(op))
//       processOp(load);
//     else if (auto store = dyn_cast<memref::StoreOp>(op))
//       processOp(store);
//     else if (auto affineLoad = dyn_cast<affine::AffineLoadOp>(op))
//       processOp(affineLoad);
//     else if (auto affineStore = dyn_cast<affine::AffineStoreOp>(op))
//       processOp(affineStore);
//   });
// }

/// Rewrites any memref.load/memref.store in the region that reference the
/// original memref. The new memref is taken from 'dbOp'.
static void rewireDatablockUses(arts::DataBlockOp &dbOp, Region &region,
                                DominanceInfo &domInfo) {
  MLIRContext *ctx = dbOp.getContext();
  auto builder = OpBuilder(ctx);
  LLVM_DEBUG(dbgs() << line << "Rewiring uses of:\n  " << dbOp << "\n");

  if (!dbOp.isLoad())
    dbOp.getPtr().replaceAllUsesExcept(dbOp.getResult(), dbOp);

  /// If the value was loaded, rewire the operations with the same pinned
  /// indices.
  const unsigned drop = dbOp.getIndices().size();

  auto updateOp = [&](auto opToUpdate) {
    OpBuilder::InsertionGuard IG(builder);
    builder.setInsertionPoint(opToUpdate);
    auto loc = opToUpdate->getLoc();
    auto origIndices = opToUpdate.getIndices();

    /// If the op supports an affine map attribute (as in affine load/store),
    /// update it accordingly.
    if (auto opAffine = getAffineMap(opToUpdate)) {
      LLVM_DEBUG(dbgs() << "  Original affine map: " << *opAffine << "\n");
      AffineMap newMap;
      {
        SmallVector<AffineExpr, 4> newExprs;
        /// Insert drop leading zero constants.
        for (unsigned i = 0; i < drop; ++i)
          newExprs.push_back(builder.getAffineConstantExpr(0));
        /// Append the remaining indices from the original op.
        for (unsigned i = drop; i < origIndices.size(); ++i)
          newExprs.push_back(builder.getAffineDimExpr(i));
        newMap = AffineMap::get(origIndices.size(), 0, newExprs, ctx);
      }
      LLVM_DEBUG(dbgs() << "  New affine map: " << newMap << "\n");
      opToUpdate->setAttr("map", AffineMapAttr::get(newMap));
    }

    /// Create a new indices vector with 'drop' leading zeros, followed by the
    /// remaining original indices.
    SmallVector<Value, 4> newIndices;
    {
      /// Insert drop leading zero constants.
      for (unsigned i = 0; i < drop; ++i)
        newIndices.push_back(builder.create<arith::ConstantIndexOp>(loc, 0));
      /// Append the remaining indices from the original op.
      for (unsigned i = drop; i < origIndices.size(); ++i)
        newIndices.push_back(origIndices[i]);
    }

    /// Update the memref operand and indices.
    opToUpdate.getMemrefMutable().assign(dbOp.getResult());
    opToUpdate.getIndicesMutable().assign(newIndices);

    LLVM_DEBUG(dbgs() << "  Drop: " << drop << "\n");
    LLVM_DEBUG(dbgs() << "  Updated: " << opToUpdate << "\n");
  };

  /// Recursively walk the region, but do not enter child regions of arts
  /// operations.
  std::function<void(Region &)> walkRegion = [&](Region &currRegion) {
    for (Block &block : currRegion.getBlocks()) {
      for (Operation &op : block) {
        /// Simply ignore not dominated operations.
        if (!domInfo.dominates(dbOp.getOperation(), &op))
          continue;

        /// If the operation is an arts operation, skip processing its nested
        /// regions.
        if (isa<arts::DataBlockOp>(op) || isa<arts::EdtOp>(op))
          continue;

        /// Process load and store operations.
        if (auto load = dyn_cast<memref::LoadOp>(op)) {
          if (isDataBlockUsed(dbOp, load))
            updateOp(load);
        } else if (auto store = dyn_cast<memref::StoreOp>(op)) {
          if (isDataBlockUsed(dbOp, store))
            updateOp(store);
        } else if (auto affineLoad = dyn_cast<affine::AffineLoadOp>(op)) {
          if (isDataBlockUsed(dbOp, affineLoad))
            updateOp(affineLoad);
        } else if (auto affineStore = dyn_cast<affine::AffineStoreOp>(op)) {
          if (isDataBlockUsed(dbOp, affineStore))
            updateOp(affineStore);
        }

        /// Iterate into child regions if available.
        for (Region &childRegion : op.getRegions())
          walkRegion(childRegion);
      }
    }
  };

  walkRegion(region);
}

struct DataBlockPass : public arts::DataBlockBase<DataBlockPass> {

  void runOnOperation() override {
    ModuleOp module = getOperation();
    LLVM_DEBUG(dbgs() << line << "DataBlockPass STARTED\n" << line);
    OpBuilder builder(module);

    // module.walk([&](EdtOp edt) {
    //   auto &region = edt.getRegion();
    //   for (auto dep : edt.getDependencies()) {
    //     auto dbOp = cast<DataBlockOp>(dep.getDefiningOp());
    //     adjustDataBlock(dbOp, region);
    //   }
    // });

    /// Rewire uses of datablock operations.
    module.walk([&](func::FuncOp func) {
      DominanceInfo domInfo(func);
      func.walk<mlir::WalkOrder::PostOrder>([&](arts::EdtOp edt) {
        // auto *region = dbOp->getParentRegion();
        auto &region = edt.getRegion();
        for (auto dep : edt.getDependencies()) {
          auto dbOp = cast<arts::DataBlockOp>(dep.getDefiningOp());
          rewireDatablockUses(dbOp, region, domInfo);
          // LLVM_DEBUG(dbgs() << func << "\n");
        }
      });
    });
    module.dump();
  }
};

} // end anonymous namespace

namespace mlir {
namespace arts {
std::unique_ptr<Pass> createDataBlockPass() {
  return std::make_unique<DataBlockPass>();
}
} // namespace arts
} // namespace mlir
