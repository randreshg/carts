///==========================================================================///
/// File: LoopNormalizer.h
///
/// Abstract base class for loop normalization patterns.
/// Provides a pluggable framework for loop transformations that enable
/// downstream DbPartitioning.
///==========================================================================///

#ifndef ARTS_TRANSFORMS_LOOP_LOOPNORMALIZER_H
#define ARTS_TRANSFORMS_LOOP_LOOPNORMALIZER_H

#include "arts/Dialect.h"
#include "arts/transforms/pattern/PatternTransform.h"
#include "mlir/IR/Builders.h"
#include "llvm/ADT/StringRef.h"
#include <memory>

namespace mlir {
namespace arts {

///===----------------------------------------------------------------------===///
/// LoopPattern - Abstract base for loop normalization patterns
///===----------------------------------------------------------------------===///
class LoopPattern : public PatternTransform {
public:
  virtual ~LoopPattern() = default;

  /// Try to match this pattern in the given arts.for.
  /// Returns true if pattern is detected; populates internal match state.
  virtual bool match(ForOp artsFor) = 0;

  /// Apply the transformation using the last successful match.
  virtual LogicalResult apply(OpBuilder &builder) = 0;

  /// Name for logging/debugging.
  virtual StringRef getName() const override = 0;
};

///===----------------------------------------------------------------------===///
/// Shared utility functions for loop patterns
///===----------------------------------------------------------------------===///

/// Copy loop attributes (arts.id and arts.loop) from source to target op.
void copyLoopAttributes(Operation *source, Operation *target);

/// Update the tripCount field in a loop's arts.loop metadata attribute.
void updateTripCountMetadata(Operation *loop, int64_t newTripCount);

/// Check if a lower bound value depends on an outer induction variable
/// via arith.addi(outerIV, constant). Returns the constant offset if found.
std::optional<int64_t> getTriangularOffset(Value lb, Value outerIV);

///===----------------------------------------------------------------------===///
/// Pattern factory functions
///===----------------------------------------------------------------------===///

/// Create SymmetricTriangularPattern for triangular→rectangular normalization.
std::unique_ptr<LoopPattern> createSymmetricTriangularPattern();

/// Create PerfectNestLinearizationPattern for absorbing directly nested
/// scf.for loops into a single linearized arts.for iteration space.
std::unique_ptr<LoopPattern> createPerfectNestLinearizationPattern();

} // namespace arts
} // namespace mlir

#endif // ARTS_TRANSFORMS_LOOP_LOOPNORMALIZER_H
