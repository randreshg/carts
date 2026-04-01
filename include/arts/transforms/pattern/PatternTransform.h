///==========================================================================///
/// File: PatternTransform.h
///
/// Shared pattern contract and transform interfaces for the pre-DB pattern
/// pipeline. Pattern discovery, refinement, and transform application should
/// all flow through these abstractions instead of being split between unrelated
/// pass-local helpers.
///==========================================================================///

#ifndef ARTS_TRANSFORMS_PATTERN_PATTERNTRANSFORM_H
#define ARTS_TRANSFORMS_PATTERN_PATTERNTRANSFORM_H

#include "arts/Dialect.h"
#include "mlir/IR/Operation.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"

namespace mlir {
namespace arts {

class PatternContract;

/// Common semantic contract shared by all pre-DB pattern families.
class PatternContract {
public:
  virtual ~PatternContract() = default;

  virtual ArtsDepPattern getFamily() const = 0;
  virtual int64_t getRevision() const { return 1; }
  virtual StringRef getName() const = 0;

  /// Stamp the shared semantic contract on the matched operation(s).
  virtual void stamp(Operation *op) const;
  virtual void stamp(ArrayRef<Operation *> ops) const;
};

class StencilNDPatternContract final : public PatternContract {
public:
  StencilNDPatternContract(ArtsDepPattern family, ArrayRef<int64_t> ownerDims,
                           ArrayRef<int64_t> minOffsets,
                           ArrayRef<int64_t> maxOffsets,
                           ArrayRef<int64_t> writeFootprint,
                           int64_t revision = 1,
                           ArrayRef<int64_t> blockShape = {})
      : family(family), ownerDims(ownerDims.begin(), ownerDims.end()),
        minOffsets(minOffsets.begin(), minOffsets.end()),
        maxOffsets(maxOffsets.begin(), maxOffsets.end()),
        writeFootprint(writeFootprint.begin(), writeFootprint.end()),
        blockShape(blockShape.begin(), blockShape.end()), revision(revision) {}

  ArtsDepPattern getFamily() const override { return family; }
  int64_t getRevision() const override { return revision; }
  StringRef getName() const override { return "stencil-nd"; }
  ArrayRef<int64_t> getOwnerDims() const { return ownerDims; }
  ArrayRef<int64_t> getMinOffsets() const { return minOffsets; }
  ArrayRef<int64_t> getMaxOffsets() const { return maxOffsets; }
  ArrayRef<int64_t> getWriteFootprint() const { return writeFootprint; }
  ArrayRef<int64_t> getBlockShape() const { return blockShape; }
  bool isMultiDimensional() const { return ownerDims.size() >= 2; }
  void stamp(Operation *op) const override;

private:
  ArtsDepPattern family = ArtsDepPattern::stencil_tiling_nd;
  SmallVector<int64_t, 4> ownerDims;
  SmallVector<int64_t, 4> minOffsets;
  SmallVector<int64_t, 4> maxOffsets;
  SmallVector<int64_t, 4> writeFootprint;
  SmallVector<int64_t, 4> blockShape;
  int64_t revision = 1;
};

/// Base interface for transforms that participate in the pre-DB pattern
/// pipeline.
class PatternTransform {
public:
  virtual ~PatternTransform() = default;

  virtual ArtsDepPattern getFamily() const { return ArtsDepPattern::unknown; }
  virtual int64_t getRevision() const { return 1; }
  virtual StringRef getName() const = 0;
};

/// Shared base for dependence/schedule transforms.
class DepPatternTransform : public PatternTransform {
public:
  ~DepPatternTransform() override = default;

  /// Apply the structural schedule rewrite across the module.
  virtual int run(ModuleOp module) = 0;
};

} // namespace arts
} // namespace mlir

#endif // ARTS_TRANSFORMS_PATTERN_PATTERNTRANSFORM_H
