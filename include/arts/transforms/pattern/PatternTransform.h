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

/// Simple pattern contract for patterns without additional spatial metadata.
/// Stamps dep_pattern, distribution_pattern (derived from dep_pattern), and
/// pattern_revision.
class SimplePatternContract final : public PatternContract {
public:
  SimplePatternContract(ArtsDepPattern family, int64_t revision = 1)
      : family(family), revision(revision) {}

  ArtsDepPattern getFamily() const override { return family; }
  int64_t getRevision() const override { return revision; }
  StringRef getName() const override;
  void stamp(Operation *op) const override;

private:
  ArtsDepPattern family = ArtsDepPattern::unknown;
  int64_t revision = 1;
};

class StencilNDPatternContract final : public PatternContract {
public:
  StencilNDPatternContract(ArtsDepPattern family, ArrayRef<int64_t> ownerDims,
                           ArrayRef<int64_t> minOffsets,
                           ArrayRef<int64_t> maxOffsets,
                           ArrayRef<int64_t> writeFootprint,
                           int64_t revision = 1,
                           ArrayRef<int64_t> blockShape = {},
                           ArrayRef<int64_t> spatialDims = {})
      : family(family), ownerDims(ownerDims.begin(), ownerDims.end()),
        minOffsets(minOffsets.begin(), minOffsets.end()),
        maxOffsets(maxOffsets.begin(), maxOffsets.end()),
        writeFootprint(writeFootprint.begin(), writeFootprint.end()),
        blockShape(blockShape.begin(), blockShape.end()),
        spatialDims(spatialDims.begin(), spatialDims.end()),
        revision(revision) {}

  ArtsDepPattern getFamily() const override { return family; }
  int64_t getRevision() const override { return revision; }
  StringRef getName() const override { return "stencil-nd"; }
  ArrayRef<int64_t> getOwnerDims() const { return ownerDims; }
  ArrayRef<int64_t> getMinOffsets() const { return minOffsets; }
  ArrayRef<int64_t> getMaxOffsets() const { return maxOffsets; }
  ArrayRef<int64_t> getWriteFootprint() const { return writeFootprint; }
  ArrayRef<int64_t> getBlockShape() const { return blockShape; }
  ArrayRef<int64_t> getSpatialDims() const { return spatialDims; }
  bool isMultiDimensional() const { return ownerDims.size() >= 2; }
  void stamp(Operation *op) const override;

private:
  ArtsDepPattern family = ArtsDepPattern::stencil_tiling_nd;
  SmallVector<int64_t, 4> ownerDims;
  SmallVector<int64_t, 4> minOffsets;
  SmallVector<int64_t, 4> maxOffsets;
  SmallVector<int64_t, 4> writeFootprint;
  SmallVector<int64_t, 4> blockShape;
  SmallVector<int64_t, 4> spatialDims;
  int64_t revision = 1;
};

/// Lightweight marker-only contract for matmul patterns.
///
/// Unlike StencilNDPatternContract which carries rich payload (offsets, block
/// shapes, footprints) needed for DB partitioning heuristics, matmul patterns
/// serve primarily as semantic markers that trigger specific EDT distribution
/// strategies. The pattern is detected by MatmulReductionPattern and recorded
/// as ArtsDepPattern::matmul, which downstream passes (EdtDistribution,
/// LoweringContractUtils) use to select matmul-aware distribution without
/// requiring additional metadata.
///
/// Why marker-only:
/// - Matmul distribution strategy needs only the pattern family, not tiling
///   factors or reduction dimensions - those are kernel-transform details that
///   don't affect the distribution decision.
/// - Current consumers (getEffectiveDistributionPattern, DbAnalysis) only
///   check the enum value to set hasMatmulUpdate hint for coarse partitioning.
/// - Enriching with unused metadata (alpha/beta coefficients, matrix layout,
///   reduction dims) would create infrastructure without consumer value.
///
/// If future passes need matmul-specific metadata (e.g., tile size selection,
/// layout-aware block partitioning), the contract can be enriched at that time
/// with proven requirements rather than speculative payload.
class MatmulPatternContract final : public PatternContract {
public:
  explicit MatmulPatternContract(int64_t revision = 1) : revision(revision) {}

  ArtsDepPattern getFamily() const override { return ArtsDepPattern::matmul; }
  int64_t getRevision() const override { return revision; }
  StringRef getName() const override { return "matmul"; }

private:
  int64_t revision = 1;
};

/// Lightweight marker-only contract for triangular (LU/Cholesky) patterns.
///
/// Similar to MatmulPatternContract, this serves as a semantic marker for
/// triangular solver patterns (LU decomposition, Cholesky factorization, etc.)
/// without carrying pattern-specific payload. The pattern is recorded as
/// ArtsDepPattern::triangular and consumed by distribution strategy selection.
///
/// Why marker-only:
/// - No triangular-specific pattern transform currently exists (unlike matmul's
///   MatmulReductionPattern) - the enum is defined but unused in practice.
/// - When implemented, the triangular pattern would primarily need the marker
///   to select appropriate distribution (likely block-cyclic for load balance).
/// - Speculative metadata (pivot column, triangular type upper/lower, blocking
///   factor) would have no consumers until a triangular-aware transform exists.
///
/// If/when a triangular pattern transform is implemented, this contract can be
/// enriched with proven requirements (e.g., triangular type for dependency
/// computation, blocking factor for tile-size selection) rather than unused
/// infrastructure.
class TriangularPatternContract final : public PatternContract {
public:
  explicit TriangularPatternContract(int64_t revision = 1)
      : revision(revision) {}

  ArtsDepPattern getFamily() const override {
    return ArtsDepPattern::triangular;
  }
  int64_t getRevision() const override { return revision; }
  StringRef getName() const override { return "triangular"; }

private:
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
