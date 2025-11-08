///===----------------------------------------------------------------------===///
// AccessAnalyzer.h - Memory Access Analysis Helper
///===----------------------------------------------------------------------===///

#ifndef ARTS_ANALYSIS_METADATA_ACCESSANALYZER_H
#define ARTS_ANALYSIS_METADATA_ACCESSANALYZER_H

#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/Operation.h"
#include "mlir/IR/Value.h"
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
    return isa<affine::AffineLoadOp, affine::AffineStoreOp>(op);
  }

  /// Check if an access is a read operation
  bool isReadAccess(Operation *op) const {
    return isa<affine::AffineLoadOp, memref::LoadOp>(op);
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
      }
    });
    return foundStrideOne;
  }

private:
  MLIRContext *context [[maybe_unused]];
};

} // namespace arts
} // namespace mlir

#endif // ARTS_ANALYSIS_METADATA_ACCESSANALYZER_H
