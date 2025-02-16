
#include "arts/Analysis/DataBlockAnalysis.h"

/// Arts
#include "ArtsPassDetails.h"
#include "arts/ArtsDialect.h"
#include "arts/Passes/ArtsPasses.h"
/// Dialects
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/Builders.h"
#include "mlir/Support/LLVM.h"
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
// /// Keep this in mind to analyze dependencies
// static DataBlockOp createDatablockOp(PatternRewriter &rewriter, Region &region,
//                                      Location loc, StringRef mode,
//                                      Value inputMemRef) {
//   bool isLoad = false;
//   auto inputMemRefOp = inputMemRef.getDefiningOp();

//   /// If the defining operation is a load op obtain the base memref and
//   /// pinned indices.
//   Value baseMemRef;
//   SmallVector<Value> pinnedIndices;
//   AffineMap affineMap;
//   if (auto loadOp = dyn_cast<memref::LoadOp>(inputMemRefOp)) {
//     baseMemRef = loadOp.getMemref();
//     pinnedIndices.assign(loadOp.getIndices().begin(),
//                          loadOp.getIndices().end());
//     affineMap = AffineMap::getMultiDimIdentityMap(pinnedIndices.size(),
//                                                   loadOp.getContext());
//     isLoad = true;
//   } else if (auto affineLoadOp =
//                  dyn_cast<affine::AffineLoadOp>(inputMemRefOp)) {
//     baseMemRef = affineLoadOp.getMemref();
//     pinnedIndices.assign(affineLoadOp.getIndices().begin(),
//                          affineLoadOp.getIndices().end());
//     affineMap = affineLoadOp.getAffineMap();
//     isLoad = true;
//   } else {
//     baseMemRef = inputMemRef;
//   }

//   /// Ensure the input memref is of type MemRefType.
//   auto baseType = baseMemRef.getType().dyn_cast<MemRefType>();
//   assert(baseType && "Input must be a MemRefType.");

//   /// Compute the rank and verify it is positive.
//   int64_t rank = baseType.getRank();

//   if (!pinnedIndices.empty()) {
//     uint64_t pinnedIndicesSize = pinnedIndices.size();
//     /// Identify zeroIndices to check whether the dependency is on a subview of
//     /// the memref or the entire memref. We do so by iterating through the
//     /// pinned indices of each store and load operation in the region and
//     /// popping the last index if it is zero. The pinned indices are adjusted
//     /// accordingly.
//     SmallVector<int64_t> zeroIndices;
//     for (auto [i, index] : llvm::enumerate(pinnedIndices)) {
//       if (auto cstOp = index.getDefiningOp<arith::ConstantIndexOp>()) {
//         if (cstOp.value() == 0)
//           zeroIndices.push_back(i);
//       }
//     }

//     if (!zeroIndices.empty()) {
//       assert(zeroIndices.back() == rank - 1 &&
//              "Last index must be zero for now");

//       /// Lambda to find a matching load/store in the region
//       auto checkIndices = [&](ValueRange opIndices) {
//         if (opIndices.size() < pinnedIndices.size()) {
//           assert(false && "Unexpected number of indices");
//         }
//         if (opIndices[zeroIndices.back()] !=
//             pinnedIndices[zeroIndices.back()]) {
//           pinnedIndices.pop_back();
//           zeroIndices.pop_back();
//         }
//       };

//       /// Process all loads/stores in the region but first check if the base
//       /// memref matches
//       auto processOp = [&](Operation *op, Value memref, ValueRange indices) {
//         if (memref == baseMemRef)
//           checkIndices(indices);
//       };

//       region.walk([&](Operation *op) {
//         if (zeroIndices.empty())
//           return;
//         if (auto load = dyn_cast<memref::LoadOp>(op))
//           processOp(op, load.getMemref(), load.getIndices());
//         else if (auto store = dyn_cast<memref::StoreOp>(op))
//           processOp(op, store.getMemref(), store.getIndices());
//         else if (auto affineLoad = dyn_cast<affine::AffineLoadOp>(op))
//           processOp(op, affineLoad.getMemref(), affineLoad.getIndices());
//         else if (auto affineStore = dyn_cast<affine::AffineStoreOp>(op))
//           processOp(op, affineStore.getMemref(), affineStore.getIndices());
//       });
//     }

//     /// If a pinned index was removed, adjust the affine map accordingly
//     uint64_t newPinnedIndicesSize = pinnedIndices.size();
//     if (newPinnedIndicesSize != pinnedIndicesSize) {
//       SmallVector<AffineExpr> exprs;
//       for (uint64_t i = 0; i < newPinnedIndicesSize; ++i) {
//         if (i < newPinnedIndicesSize) {
//           exprs.push_back(rewriter.getAffineDimExpr(i));
//           continue;
//         }
//         exprs.push_back(rewriter.getAffineSymbolExpr(i));
//       }
//       LLVM_DEBUG(dbgs() << "Old affine map: " << affineMap << "\n");
//       affineMap =
//           AffineMap::get(newPinnedIndicesSize, 0, exprs, rewriter.getContext());
//       LLVM_DEBUG(dbgs() << "New affine map: " << affineMap << "\n");
//     }
//   }

//   /// Prepare arrays for subview
//   const auto pinnedCount = static_cast<int64_t>(pinnedIndices.size());
//   SmallVector<Value, 4> offsets(pinnedCount), sizes(rank);
//   SmallVector<int64_t, 4> subShape(rank);

//   OpBuilder::InsertionGuard g(rewriter);
//   if (inputMemRefOp)
//     rewriter.setInsertionPointAfter(inputMemRefOp);

//   /// Precompute constants and pinned count
//   // Value zero = rewriter.create<arith::ConstantIndexOp>(loc, 0);
//   Value oneSize = rewriter.create<arith::ConstantIndexOp>(loc, 1);

//   /// Compute dimension sizes, offsets, sizes, and subShape in one loop.
//   for (int64_t i = 0; i < rank; ++i) {
//     // Compute the dimension value.
//     Value dimVal =
//         baseType.isDynamicDim(i)
//             ? rewriter.create<memref::DimOp>(loc, baseMemRef, i).getResult()
//             : rewriter
//                   .create<arith::ConstantIndexOp>(loc, baseType.getDimSize(i))
//                   .getResult();

//     if (i < pinnedCount) {
//       offsets[i] = pinnedIndices[i];
//       sizes[i] = oneSize;
//       subShape[i] = 1;
//     } else {
//       // offsets[i] = zero;
//       sizes[i] = dimVal;
//       subShape[i] = baseType.isDynamicDim(i) ? ShapedType::kDynamic
//                                              : baseType.getDimSize(i);
//     }
//   }

//   /// Now build the subview memref type
//   auto elementType = baseType.getElementType();
//   auto subMemRefType = MemRefType::get(subShape, elementType,
//                                        baseType.getLayout().getAffineMap(),
//                                        baseType.getMemorySpace());

//   /// Insert polygeist.typeSizeOp to get the size of the element type
//   auto elementTypeSize = rewriter
//                              .create<polygeist::TypeSizeOp>(
//                                  loc, rewriter.getIndexType(), elementType)
//                              .getResult();

//   /// Create the final arts.datablock operation with the affine map attribute.
//   auto modeAttr = rewriter.getStringAttr(mode);
//   DataBlockOp depOp;
//   LLVM_DEBUG(dbgs() << "Creating datablock: " << subMemRefType << " - "
//                     << affineMap << "\n");
//   if (!affineMap) {
//     depOp = rewriter.create<arts::DataBlockOp>(loc, subMemRefType, modeAttr,
//                                                baseMemRef, elementType,
//                                                elementTypeSize, offsets, sizes);
//   } else {
//     auto affineMapAttr = AffineMapAttr::get(affineMap);
//     depOp = rewriter.create<arts::DataBlockOp>(
//         loc, subMemRefType, modeAttr, baseMemRef, elementType, elementTypeSize,
//         offsets, sizes, affineMapAttr);
//   }

//   if (isLoad)
//     depOp->setAttr("isLoad", rewriter.getUnitAttr());
//   return depOp;
// }

// for (Value dep : edtEnv.getDependencies()) {
//   auto depOp = cast<arts::DataBlockOp>(dep.getDefiningOp());
//   LLVM_DEBUG(dbgs() << "Processing datablock: " << depOp << "\n");
//   if (depOp.isLoad())
//     rewireDatablockUses(rewriter, region, depOp);
//   else
//     depOp.getBase().replaceAllUsesExcept(depOp.getResult(), depOp);
// }

// /// Rewrites any memref.load/memref.store in 'region' that references
// /// 'baseMemRef'. The new memref is 'newSubview'.
// static void rewireDatablockUses(PatternRewriter &rewriter, Region &region,
//                                 arts::DataBlockOp depOp) {
//   Value newSubview = depOp.getResult();
//   Value baseMemRef = depOp.getBase();

//   auto offsets = depOp.getOffsets();
//   auto sizes = depOp.getSizes();
//   auto subType = newSubview.getType().dyn_cast<MemRefType>();
//   assert(subType && "Expected MemRefType");
//   int64_t rank = subType.getRank();

//   /// Collect the dimensions that are pinned
//   SetVector<int64_t> pinnedDims;
//   for (int64_t i = 0; i < rank; ++i) {
//     if (auto cstOp = sizes[i].getDefiningOp<arith::ConstantIndexOp>()) {
//       if (cstOp.value() == 1)
//         pinnedDims.insert(i);
//     }
//   }

//   /// Walk the region and replace the loads/stores
//   auto isMatchingMemrefAndIndices = [&](auto op) {
//     // TODO: Alias analysis
//     if (op.getMemref() != baseMemRef)
//       return false;
//     auto oldIdx = op.getIndices();
//     if ((int64_t)oldIdx.size() != rank)
//       return false;
//     for (auto dim : pinnedDims) {
//       if (offsets[dim] != oldIdx[dim])
//         return false;
//     }
//     // If the op is affine, also compare the affine map.
//     if (llvm::isa<affine::AffineLoadOp>(op) ||
//         llvm::isa<affine::AffineStoreOp>(op)) {
//       auto expectedMap =
//           AffineMap::getMultiDimIdentityMap(rank, op.getContext());
//       if (llvm::isa<affine::AffineLoadOp>(op)) {
//         auto affineLoad = cast<affine::AffineLoadOp>(op);
//         if (affineLoad.getAffineMap() != expectedMap)
//           return false;
//       } else {
//         auto affineStore = cast<affine::AffineStoreOp>(op);
//         if (affineStore.getAffineMap() != expectedMap)
//           return false;
//       }
//     }
//     return true;
//   };

//   region.walk([&](Operation *op) {
//     auto updateOp = [&](auto specificOp) {
//       rewriter.updateRootInPlace(specificOp, [&]() {
//         SmallVector<Value> newIdx(rank);
//         Location loc = specificOp.getLoc();
//         Value c0 = rewriter.create<arith::ConstantIndexOp>(loc, 0);
//         for (int64_t i = 0; i < rank; ++i)
//           newIdx[i] = pinnedDims.contains(i) ? c0 : specificOp.getIndices()[i];
//         specificOp.getMemrefMutable().assign(newSubview);
//         specificOp.getIndicesMutable().assign(newIdx);
//       });
//     };

//     if (auto load = dyn_cast<memref::LoadOp>(op)) {
//       if (isMatchingMemrefAndIndices(load))
//         updateOp(load);
//     } else if (auto store = dyn_cast<memref::StoreOp>(op)) {
//       if (isMatchingMemrefAndIndices(store))
//         updateOp(store);
//     } else if (auto affineLoad = dyn_cast<affine::AffineLoadOp>(op)) {
//       if (isMatchingMemrefAndIndices(affineLoad))
//         updateOp(affineLoad);
//     } else if (auto affineStore = dyn_cast<affine::AffineStoreOp>(op)) {
//       if (isMatchingMemrefAndIndices(affineStore))
//         updateOp(affineStore);
//     }
//   });
// }

struct DataBlockPass
    : public arts::DataBlockBase<DataBlockPass> {

  void runOnOperation() override {
    ModuleOp module = getOperation();
    
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
