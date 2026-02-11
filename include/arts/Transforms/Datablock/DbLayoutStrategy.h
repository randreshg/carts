///==========================================================================///
/// File: DbLayoutStrategy.h
///
/// Layout strategy contract for datablock pointer materialization.
/// Keeps layout-dependent addressing policy out of lowering pass bodies.
///==========================================================================///

#ifndef ARTS_TRANSFORMS_DATABLOCK_DBLAYOUTSTRATEGY_H
#define ARTS_TRANSFORMS_DATABLOCK_DBLAYOUTSTRATEGY_H

#include "arts/ArtsDialect.h"
#include "mlir/IR/Location.h"
#include "mlir/IR/Value.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/SmallVector.h"

namespace mlir {
namespace arts {

class ArtsCodegen;

struct LayoutInfo {
  DbAllocOp alloc;
  PartitionMode mode = PartitionMode::coarse;
  SmallVector<Value> sizes;
  SmallVector<Value> elementSizes;
  unsigned outerRank = 0;
  unsigned innerRank = 0;
};

/// Build layout information for a datablock handle or acquire pointer.
LayoutInfo buildLayoutInfo(Value source);

/// Base strategy for mode-aware datablock element pointer materialization.
class DbLayoutStrategy {
public:
  virtual ~DbLayoutStrategy() = default;

  virtual Value computeElementPointer(ArtsCodegen &AC, Location loc, Value base,
                                      ArrayRef<Value> indices,
                                      const LayoutInfo &layout) const = 0;
};

/// Return the concrete strategy for a partition mode.
const DbLayoutStrategy &getDbLayoutStrategy(PartitionMode mode);

} // namespace arts
} // namespace mlir

#endif /// ARTS_TRANSFORMS_DATABLOCK_DBLAYOUTSTRATEGY_H
