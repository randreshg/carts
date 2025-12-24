///==========================================================================///
/// DbAccessPattern.h - Unified access pattern types for datablock operations
///
/// This file defines common types for access pattern classification and
/// stencil bounds analysis used across DbAllocNode, DbAcquireNode, and
/// partitioning passes.
///==========================================================================///

#ifndef ARTS_ANALYSIS_GRAPHS_DB_DBACCESSPATTERN_H
#define ARTS_ANALYSIS_GRAPHS_DB_DBACCESSPATTERN_H

#include <cstdint>

namespace mlir {
namespace arts {

/// Per-acquire data access pattern classification.
/// Used to reason about mixed access patterns across a single allocation
/// and to select appropriate rewriter modes in DbPartitioning.
enum class AccessPattern {
  Unknown,  /// Pattern not yet analyzed or unrecognizable
  Uniform,  /// Regular access (offsets only, no stencil bounds detected)
  Stencil,  /// Neighbor-dependent access (halo detected, e.g., A[i-1], A[i+1])
  Indexed   /// Explicit indices or irregular access patterns
};

/// Stencil bounds information for halo-aware localization.
/// Computed during canBePartitioned() analysis and used by DbStencilRewriter.
struct StencilBounds {
  int64_t minOffset = 0;  /// Min constant offset from chunk_base (e.g., -1)
  int64_t maxOffset = 0;  /// Max constant offset from chunk_base (e.g., +1)
  bool isStencil = false; /// True if min != max (stencil pattern detected)
  bool valid = false;     /// True if analysis succeeded

  /// Compute left halo size (elements needed before chunk start)
  int64_t haloLeft() const { return minOffset < 0 ? -minOffset : 0; }

  /// Compute right halo size (elements needed after chunk end)
  int64_t haloRight() const { return maxOffset > 0 ? maxOffset : 0; }

  /// Check if any halo is needed
  bool hasHalo() const { return haloLeft() > 0 || haloRight() > 0; }
};

/// Summary of per-acquire access patterns at allocation level.
/// Used to determine partitioning strategy for an entire allocation.
struct AcquirePatternSummary {
  bool hasUniform = false;  /// At least one acquire uses uniform access
  bool hasStencil = false;  /// At least one acquire uses stencil access
  bool hasIndexed = false;  /// At least one acquire uses indexed access

  /// Returns true if multiple different patterns are present
  bool isMixed() const {
    int count = static_cast<int>(hasUniform) + static_cast<int>(hasStencil) +
                static_cast<int>(hasIndexed);
    return count > 1;
  }

  /// Get the dominant pattern (if not mixed)
  AccessPattern getDominant() const {
    if (isMixed())
      return AccessPattern::Unknown;
    if (hasStencil)
      return AccessPattern::Stencil;
    if (hasIndexed)
      return AccessPattern::Indexed;
    if (hasUniform)
      return AccessPattern::Uniform;
    return AccessPattern::Unknown;
  }
};

} // namespace arts
} // namespace mlir

#endif // ARTS_ANALYSIS_GRAPHS_DB_DBACCESSPATTERN_H
