///===----------------------------------------------------------------------===///
// AccessAnalyzer.h - Memory Access Analysis Helper
///===----------------------------------------------------------------------===///

#ifndef ARTS_ANALYSIS_METADATA_ACCESSANALYZER_H
#define ARTS_ANALYSIS_METADATA_ACCESSANALYZER_H

#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/Operation.h"
#include "mlir/IR/Value.h"
#include "llvm/ADT/STLExtras.h"
#include <optional>

namespace mlir {
namespace arts {

///===----------------------------------------------------------------------===///
// AccessAnalyzer - Helper for analyzing memory access operations
///===----------------------------------------------------------------------===///
class AccessAnalyzer {
public:
  explicit AccessAnalyzer(MLIRContext *context) : context(context) {}

  /// Check if an operation is a memory access (load or store)
  bool isMemoryAccess(Operation *op) const {
    return isa<affine::AffineLoadOp, affine::AffineStoreOp, memref::LoadOp,
               memref::StoreOp>(op);
  }

  /// Check if an access is affine
  bool isAffineAccess(Operation *op) const {
    if (isa<affine::AffineLoadOp, affine::AffineStoreOp>(op))
      return true;
    if (auto load = dyn_cast<memref::LoadOp>(op))
      return llvm::all_of(load.getIndices(),
                          [&](Value idx) { return isAffineIndexExpr(idx); });
    if (auto store = dyn_cast<memref::StoreOp>(op))
      return llvm::all_of(store.getIndices(),
                          [&](Value idx) { return isAffineIndexExpr(idx); });
    return false;
  }

  /// Check if an access is a read operation
  bool isReadAccess(Operation *op) const {
    return isa<affine::AffineLoadOp, memref::LoadOp>(op);
  }

  /// Check if an access is a write operation
  bool isWriteAccess(Operation *op) const {
    return isa<affine::AffineStoreOp, memref::StoreOp>(op);
  }

  /// Inspect whether an index expression is affine.
  bool isAffineIndex(Value value) const { return isAffineIndexExpr(value); }

  /// Inspect whether an index expression represents unit stride.
  bool isUnitStrideLike(Value value) const { return isUnitStrideIndex(value); }

  /// Inspect if an index expression is a constant.
  bool isConstantIndexValue(Value value) const {
    return isConstantIndex(value);
  }

  /// Get the memref being accessed
  Value getAccessedMemref(Operation *op) const {
    if (auto load = dyn_cast<affine::AffineLoadOp>(op))
      return load.getMemRef();
    if (auto store = dyn_cast<affine::AffineStoreOp>(op))
      return store.getMemRef();
    if (auto load = dyn_cast<memref::LoadOp>(op))
      return load.getMemRef();
    if (auto store = dyn_cast<memref::StoreOp>(op))
      return store.getMemRef();
    return nullptr;
  }

  /// Extract affine map from an affine access
  std::optional<AffineMap> extractAffineMap(Operation *op) const {
    if (auto load = dyn_cast<affine::AffineLoadOp>(op))
      return load.getAffineMap();
    if (auto store = dyn_cast<affine::AffineStoreOp>(op))
      return store.getAffineMap();
    return std::nullopt;
  }

  /// Check if all accesses to a memref have stride-one pattern
  bool hasStrideOneAccess(Value memref, Operation *scopeOp) const {
    bool foundStrideOne = false;
    scopeOp->walk([&](Operation *op) {
      if (!isMemoryAccess(op))
        return;
      if (getAccessedMemref(op) != memref)
        return;

      // For affine accesses, check if innermost dimension has stride 1
      if (auto map = extractAffineMap(op)) {
        if (map->getNumResults() > 0) {
          auto lastResult = map->getResult(map->getNumResults() - 1);
          if (auto dimExpr = lastResult.dyn_cast<AffineDimExpr>()) {
            foundStrideOne = true;
          }
        }
        return;
      }

      if (auto load = dyn_cast<memref::LoadOp>(op)) {
        if (!load.getIndices().empty() &&
            isUnitStrideIndex(load.getIndices().back())) {
          foundStrideOne = true;
        }
        return;
      }
      if (auto store = dyn_cast<memref::StoreOp>(op)) {
        if (!store.getIndices().empty() &&
            isUnitStrideIndex(store.getIndices().back())) {
          foundStrideOne = true;
        }
        return;
      }
    });
    return foundStrideOne;
  }

private:
  MLIRContext *context [[maybe_unused]];

  /// Return true if value describes a constant integer/index.
  bool isConstantIndex(Value value) const {
    if (auto constOp = value.getDefiningOp<arith::ConstantOp>())
      return constOp.getValue().isa<IntegerAttr>();
    return false;
  }

  /// Strip benign casts when looking at indices.
  Value stripIndexCasts(Value value) const {
    if (auto cast = value.getDefiningOp<arith::IndexCastOp>())
      return stripIndexCasts(cast.getIn());
    if (auto ext = value.getDefiningOp<arith::ExtSIOp>())
      return stripIndexCasts(ext.getIn());
    if (auto trunc = value.getDefiningOp<arith::TruncIOp>())
      return stripIndexCasts(trunc.getIn());
    return value;
  }

  bool isLoopInductionVar(Value value) const {
    if (auto arg = value.dyn_cast<BlockArgument>()) {
      Operation *parent = arg.getOwner()->getParentOp();
      return parent && isa<affine::AffineForOp, scf::ForOp, scf::ParallelOp,
                           scf::ForallOp>(parent);
    }
    return false;
  }

  bool isAffineIndexExpr(Value value, int depth = 0) const {
    if (depth > 8)
      return false;
    value = stripIndexCasts(value);
    if (isLoopInductionVar(value) || isConstantIndex(value))
      return true;

    Operation *def = value.getDefiningOp();
    if (!def)
      return false;

    if (auto add = dyn_cast<arith::AddIOp>(def))
      return isAffineIndexExpr(add.getLhs(), depth + 1) &&
             isAffineIndexExpr(add.getRhs(), depth + 1);
    if (auto sub = dyn_cast<arith::SubIOp>(def))
      return isAffineIndexExpr(sub.getLhs(), depth + 1) &&
             isAffineIndexExpr(sub.getRhs(), depth + 1);
    if (auto mul = dyn_cast<arith::MulIOp>(def)) {
      if (isConstantIndex(mul.getLhs()))
        return isAffineIndexExpr(mul.getRhs(), depth + 1);
      if (isConstantIndex(mul.getRhs()))
        return isAffineIndexExpr(mul.getLhs(), depth + 1);
      return false;
    }
    return false;
  }

  bool isUnitStrideIndex(Value value, int depth = 0) const {
    if (depth > 4)
      return false;
    value = stripIndexCasts(value);
    if (isLoopInductionVar(value))
      return true;

    Operation *def = value.getDefiningOp();
    if (!def)
      return false;

    if (auto add = dyn_cast<arith::AddIOp>(def)) {
      if (isConstantIndex(add.getLhs()))
        return isUnitStrideIndex(add.getRhs(), depth + 1);
      if (isConstantIndex(add.getRhs()))
        return isUnitStrideIndex(add.getLhs(), depth + 1);
    } else if (auto sub = dyn_cast<arith::SubIOp>(def)) {
      if (isConstantIndex(sub.getRhs()))
        return isUnitStrideIndex(sub.getLhs(), depth + 1);
    }
    return false;
  }
};

} // namespace arts
} // namespace mlir

#endif // ARTS_ANALYSIS_METADATA_ACCESSANALYZER_H
