#ifndef CARTS_UTILS_PATTERNSEMANTICS_H
#define CARTS_UTILS_PATTERNSEMANTICS_H

#include "arts/Dialect.h"
#include <optional>

namespace mlir {
namespace arts {

/// Forward declaration to avoid circular dependency.
struct DistributionStrategy;

/// PatternSemantics provides semantic classification and behavior
/// for dependency patterns and distribution patterns.
///
/// This centralizes pattern-family logic that was previously scattered
/// across heuristics and partitioning code.
class PatternSemantics {
public:
  /// Pattern family classification for ArtsDepPattern.
  static bool isStencilFamily(ArtsDepPattern pattern);
  static bool isUniformFamily(ArtsDepPattern pattern);
  static bool isTriangularFamily(ArtsDepPattern pattern);
  static bool isMatmulFamily(ArtsDepPattern pattern);

  /// Stencil-specific subclassifications.
  static bool requiresHaloExchange(ArtsDepPattern pattern);

  /// Pattern-to-pattern mappings.
  static std::optional<EdtDistributionPattern>
  inferDistributionPattern(ArtsDepPattern depPattern);

  /// Distribution pattern overrides for specific dep patterns.
  /// Used by DistributionHeuristics::selectDistributionKind.
  static std::optional<EdtDistributionKind>
  getPreferredDistributionKind(ArtsDepPattern depPattern,
                                const DistributionStrategy &strategy);

  /// Stencil-specific coarsening policy.
  /// Returns true if the pattern benefits from specialized stencil coarsening.
  static bool prefersStencilCoarsening(ArtsDepPattern pattern);
  static bool prefersStencilCoarsening(EdtDistributionPattern pattern);
};

} // namespace arts
} // namespace mlir

#endif // CARTS_UTILS_PATTERNSEMANTICS_H
