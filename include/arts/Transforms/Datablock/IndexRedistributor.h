///==========================================================================///
/// File: IndexRedistributor.h
///
/// Consolidates index mapping logic for allocation promotion.
///==========================================================================///

#ifndef ARTS_TRANSFORMS_DATABLOCK_INDEXREDISTRIBUTOR_H
#define ARTS_TRANSFORMS_DATABLOCK_INDEXREDISTRIBUTOR_H

#include "mlir/IR/Builders.h"
#include "mlir/IR/Location.h"
#include "mlir/IR/Value.h"
#include "llvm/ADT/SmallVector.h"

namespace mlir {
namespace arts {

///===----------------------------------------------------------------------===///
/// IndexRedistributor - Consolidates index mapping logic for allocation
/// promotion
///
/// Handles redistribution of indices between outer (db_ref) and inner
/// (load/store) dimensions based on allocation mode:
///
/// CHUNKED mode: div/mod on first index
///   newOuter = [index / chunkSize]
///   newInner = [index % chunkSize, remaining...]
///
/// ELEMENT-WISE mode: redistribute indices
///   newOuter = [loadStoreIndices[0..outerRank]]
///   newInner = [loadStoreIndices[outerRank..]]
///===----------------------------------------------------------------------===///
struct IndexRedistributor {
  unsigned outerRank;
  unsigned innerRank;
  bool isChunked;
  Value chunkSize; // Only valid if isChunked

  /// Redistribute indices between outer and inner (for db_refs outside EDTs).
  std::pair<SmallVector<Value>, SmallVector<Value>>
  redistribute(ValueRange loadStoreIndices, OpBuilder &builder,
               Location loc) const;

  /// Redistribute indices with offset localization (for acquires inside EDTs).
  /// Element-wise only - localizes global indices to acquired slice.
  std::pair<SmallVector<Value>, SmallVector<Value>>
  redistributeWithOffset(ValueRange loadStoreIndices, Value elemOffset,
                         OpBuilder &builder, Location loc) const;
};

} // namespace arts
} // namespace mlir

#endif // ARTS_TRANSFORMS_DATABLOCK_INDEXREDISTRIBUTOR_H
