///==========================================================================///
/// File: LoopNormalizer.h
///
/// Abstract base class for loop normalization patterns.
/// Provides a pluggable framework for loop transformations that enable
/// downstream DbPartitioning.
///==========================================================================///

#ifndef ARTS_TRANSFORMS_LOOP_LOOPNORMALIZER_H
#define ARTS_TRANSFORMS_LOOP_LOOPNORMALIZER_H

#include "arts/analysis/metadata/MetadataManager.h"
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

/// Rewrite one loop entity into another while preserving pattern contracts.
bool rewriteNormalizedLoop(Operation *source, Operation *target,
                           MetadataManager &metadataManager);

/// Check if a lower bound value depends on an outer induction variable
/// via arith.addi(outerIV, constant). Returns the constant offset if found.
std::optional<int64_t> getTriangularOffset(Value lb, Value outerIV);

///===----------------------------------------------------------------------===///
/// Pattern factory functions
///===----------------------------------------------------------------------===///

/// Create SymmetricTriangularPattern for triangular→rectangular normalization.
std::unique_ptr<LoopPattern>
createSymmetricTriangularPattern(MetadataManager &metadataManager);

/// Create PerfectNestLinearizationPattern for absorbing directly nested
/// scf.for loops into a single linearized arts.for iteration space.
std::unique_ptr<LoopPattern>
createPerfectNestLinearizationPattern(MetadataManager &metadataManager);

} // namespace arts
} // namespace mlir

#endif // ARTS_TRANSFORMS_LOOP_LOOPNORMALIZER_H
