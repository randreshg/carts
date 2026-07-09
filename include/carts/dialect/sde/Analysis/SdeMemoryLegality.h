///==========================================================================///
/// File: SdeMemoryLegality.h
///
/// Phase B substrate: block budget legality from device capacity facts.
///==========================================================================///

#ifndef CARTS_DIALECT_SDE_ANALYSIS_SDE_MEMORY_LEGALITY_H
#define CARTS_DIALECT_SDE_ANALYSIS_SDE_MEMORY_LEGALITY_H

#include "mlir/IR/Operation.h"
#include "mlir/IR/Value.h"
#include "mlir/Pass/AnalysisManager.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"

#include <cstdint>
#include <optional>

namespace mlir::carts::sde {

struct SdeMemoryLegality {
  static constexpr llvm::StringLiteral MaxBlockBytesAttr =
      "carts.sde.max-block-bytes";

  explicit SdeMemoryLegality(Operation *op);

  void compute();

  bool isInvalidated(const AnalysisManager::PreservedAnalyses &pa);

  std::optional<int64_t> getMaxBlockBytes() const { return maxBlockBytes; }

  SmallVector<int64_t, 4> capBlockShapeToBudget(
      Value root, ArrayRef<int64_t> logicalShape, ArrayRef<int64_t> ownerDims,
      ArrayRef<int64_t> currentBlockShape) const;

private:
  Operation *operation;
  std::optional<int64_t> maxBlockBytes;
};

} // namespace mlir::carts::sde

#endif // CARTS_DIALECT_SDE_ANALYSIS_SDE_MEMORY_LEGALITY_H
