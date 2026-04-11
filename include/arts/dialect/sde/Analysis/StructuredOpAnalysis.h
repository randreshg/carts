///==========================================================================///
/// File: StructuredOpAnalysis.h
///
/// Reusable structural loop analysis for SDE scheduling-unit loops.
///
/// This analysis stays on the SDE side of the pipeline so semantic pattern
/// classification can be shared by RaiseToLinalg, tensor transforms, and later
/// SDE-owned distribution planning without depending on ARTS runtime IR.
///==========================================================================///

#ifndef ARTS_DIALECT_SDE_ANALYSIS_STRUCTUREDMETHODANALYSIS_H
#define ARTS_DIALECT_SDE_ANALYSIS_STRUCTUREDMETHODANALYSIS_H

#include "arts/dialect/sde/IR/SdeDialect.h"
#include "mlir/Dialect/Linalg/IR/Linalg.h"
#include "mlir/IR/AffineMap.h"
#include "llvm/ADT/SmallVector.h"
#include <optional>

namespace mlir::arts::sde {

struct LoopNestInfo {
  SmallVector<Value> ivs;
  Block *innermostBody = nullptr;
  SdeSuIterateOp rootIterOp;
};

struct MemrefAccessEntry {
  Value memref;
  AffineMap indexingMap;
  Operation *op = nullptr;
  bool isRead = false;
};

struct StructuredLoopSummary {
  LoopNestInfo nest;
  SmallVector<MemrefAccessEntry> reads;
  SmallVector<MemrefAccessEntry> writes;
  SmallVector<AffineMap> outputMaps;
  SmallVector<utils::IteratorType> iterTypes;
  SdeStructuredClassification classification =
      SdeStructuredClassification::elementwise;
  bool supportsReductionCarrier = false;

  bool supportsLinalgCarrier() const;
};

struct StructuredNeighborhoodInfo {
  SmallVector<int64_t, 4> ownerDims;
  SmallVector<int64_t, 4> spatialDims;
  SmallVector<int64_t, 4> minOffsets;
  SmallVector<int64_t, 4> maxOffsets;
  SmallVector<int64_t, 4> writeFootprint;
};

/// Analyze one `arts_sde.su_iterate` nest and recover the structural facts
/// needed by higher SDE passes. Returns nullopt when the loop is not a
/// supported perfectly nested memref-based structured loop.
std::optional<StructuredLoopSummary> analyzeStructuredLoop(SdeSuIterateOp op);

/// Recover a runtime-neutral structured neighborhood summary directly from SDE
/// loop-access analysis.
std::optional<StructuredNeighborhoodInfo>
extractNeighborhoodSummary(const StructuredLoopSummary &summary);

} // namespace mlir::arts::sde

#endif // ARTS_DIALECT_SDE_ANALYSIS_STRUCTUREDMETHODANALYSIS_H
