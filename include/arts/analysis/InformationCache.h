///==========================================================================///
/// File: InformationCache.h
///
/// Pre-scanned module-level fact cache to eliminate redundant walks.
///==========================================================================///

#ifndef ARTS_ANALYSIS_INFORMATIONCACHE_H
#define ARTS_ANALYSIS_INFORMATIONCACHE_H

#include "arts/Dialect.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/IR/BuiltinOps.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallVector.h"

namespace mlir {
namespace arts {

/// Cache of pre-scanned module-level facts. Built once by scanning the module,
/// invalidated when IR structure changes. Eliminates redundant module.walk()
/// calls that multiple passes repeat independently.
class InformationCache {
public:
  /// Build the cache by scanning the module.
  void build(ModuleOp module);

  /// Invalidate all cached data.
  void invalidate();

  /// Check if the cache has been built.
  bool isBuilt() const { return built; }

  /// Get all EdtOps within a function.
  ArrayRef<EdtOp> getEdtsInFunction(func::FuncOp func) const;

  /// Get all DbAllocOps within a function.
  ArrayRef<DbAllocOp> getAllocsInFunction(func::FuncOp func) const;

  /// Get all ForOps within a function.
  ArrayRef<ForOp> getLoopsInFunction(func::FuncOp func) const;

  /// Get all DbAcquireOps within a function.
  ArrayRef<DbAcquireOp> getAcquiresInFunction(func::FuncOp func) const;

  /// Get all functions in the module.
  ArrayRef<func::FuncOp> getFunctions() const { return functions; }

  /// Get total EDT count across all functions.
  size_t getTotalEdtCount() const;

  /// Get total DB alloc count across all functions.
  size_t getTotalAllocCount() const;

private:
  bool built = false;
  SmallVector<func::FuncOp> functions;
  DenseMap<func::FuncOp, SmallVector<EdtOp>> edtsPerFunction;
  DenseMap<func::FuncOp, SmallVector<DbAllocOp>> allocsPerFunction;
  DenseMap<func::FuncOp, SmallVector<ForOp>> loopsPerFunction;
  DenseMap<func::FuncOp, SmallVector<DbAcquireOp>> acquiresPerFunction;

  /// Empty vectors returned when function not found.
  static const SmallVector<EdtOp> emptyEdts;
  static const SmallVector<DbAllocOp> emptyAllocs;
  static const SmallVector<ForOp> emptyLoops;
  static const SmallVector<DbAcquireOp> emptyAcquires;
};

} // namespace arts
} // namespace mlir

#endif // ARTS_ANALYSIS_INFORMATIONCACHE_H
