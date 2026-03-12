///==========================================================================///
/// File: AccessPatternAnalysis.cpp
///
/// Shared loop-relative memory access bounds analysis.
///==========================================================================///

#include "arts/Analysis/AccessPatternAnalysis.h"
#include "arts/Utils/ValueUtils.h"
#include <algorithm>
#include <limits>

using namespace mlir;
using namespace mlir::arts;

/// Normalize stencil bounds to center them around zero.
///
/// This is useful for uniformly shifted stencils. For example:
///   - Original bounds: [1, 3] (accesses A[i+1], A[i+2], A[i+3])
///   - Normalized:      [-1, 1] with centerOffset=2
///
/// This normalization simplifies halo computation by expressing the pattern
/// as symmetric offsets from a shifted base.
void arts::normalizeAccessBounds(AccessBoundsResult &bounds) {
  if (!bounds.valid || !bounds.isStencil)
    return;

  /// Already centered around zero (e.g., [-1, 1] or [-2, 2])
  if (bounds.minOffset < 0 && bounds.maxOffset > 0)
    return;

  /// Compute center offset: (min + max) / 2
  /// Only normalize if the sum is even (ensures integer center)
  int64_t sum = bounds.minOffset + bounds.maxOffset;
  if (sum % 2 != 0)
    return;

  int64_t center = sum / 2;
  if (center == 0)
    return;

  /// Shift bounds to center them around zero
  bounds.centerOffset = center;
  bounds.minOffset -= center;
  bounds.maxOffset -= center;
}

/// Analyze memory access bounds relative to loop induction variable and block
/// base.
///
/// This function extracts constant offsets from memory access index chains to
/// determine the access pattern (uniform vs. stencil) and compute halo bounds.
///
/// Algorithm:
///   1. For each access, find the index that depends on loopIV/blockBase
///   2. Try to extract constant offset from that index (e.g., iv+1, iv-2)
///   3. Track min/max offsets across all accesses
///   4. Classify as stencil if minOffset != maxOffset
///
/// Example:
///   Accesses: A[i-1], A[i], A[i+1]
///   Result: minOffset=-1, maxOffset=1, isStencil=true
///
/// Parameters:
///   - accesses: Collection of index chains from memory operations
///   - loopIV: Loop induction variable to analyze relative to
///   - blockBase: Block base value (may be same as loopIV)
///   - partitionDim: Optional dimension to focus on (for multi-dim analysis)
///
/// Returns: AccessBoundsResult with offset bounds and pattern classification
AccessBoundsResult
arts::analyzeAccessBoundsFromIndices(ArrayRef<AccessIndexInfo> accesses,
                                     Value loopIV, Value blockBase,
                                     std::optional<unsigned> partitionDim) {
  AccessBoundsResult bounds;
  bounds.minOffset = std::numeric_limits<int64_t>::max();
  bounds.maxOffset = std::numeric_limits<int64_t>::min();

  /// Validate inputs
  if (!loopIV || !blockBase)
    return bounds;

  /// Empty access list is valid (uniform access with offset 0)
  if (accesses.empty()) {
    bounds.minOffset = 0;
    bounds.maxOffset = 0;
    bounds.valid = true;
    return bounds;
  }

  bool foundAny = false;
  for (const AccessIndexInfo &access : accesses) {
    if (access.indexChain.empty())
      continue;

    /// Select which index to analyze for offset extraction.
    /// Priority:
    ///   1. Specific partition dimension (if provided)
    ///   2. First index that depends on loopIV/blockBase
    ///   3. First non-constant index (as a fallback)
    Value idxForBounds;

    /// Priority 1: Use specific dimension if analyzing multi-dim accesses
    if (partitionDim) {
      unsigned chainIdx = access.dbRefPrefix + *partitionDim;
      if (chainIdx < access.indexChain.size())
        idxForBounds = access.indexChain[chainIdx];
    }

    /// Priority 2: Find index that depends on our analysis base
    /// (loopIV/blockBase)
    if (!idxForBounds) {
      for (Value idx : access.indexChain) {
        if (ValueUtils::dependsOn(idx, loopIV) ||
            ValueUtils::dependsOn(idx, blockBase)) {
          idxForBounds = idx;
          break;
        }
      }
    }

    /// Priority 3: Use any non-constant index as fallback
    if (!idxForBounds) {
      for (Value idx : access.indexChain) {
        int64_t constVal = 0;
        if (!ValueUtils::getConstantIndex(idx, constVal)) {
          idxForBounds = idx;
          break;
        }
      }
    }

    /// Skip this access if we couldn't find a suitable index
    if (!idxForBounds)
      continue;

    /// Try to extract constant offset from the selected index
    /// Handles patterns like: iv, iv+c, iv-c, blockBase+c, etc.
    auto constOffset =
        ValueUtils::extractConstantOffset(idxForBounds, loopIV, blockBase);
    if (constOffset) {
      foundAny = true;
      bounds.minOffset = std::min(bounds.minOffset, *constOffset);
      bounds.maxOffset = std::max(bounds.maxOffset, *constOffset);
      continue;
    }

    /// If we couldn't extract a constant offset but the index depends on
    /// both loopIV and blockBase, it's a variable offset pattern.
    /// This indicates indirect or complex affine access that we can't
    /// statically analyze. Mark as variable and return early.
    if (ValueUtils::dependsOn(idxForBounds, loopIV) &&
        ValueUtils::dependsOn(idxForBounds, blockBase)) {
      bounds.hasVariableOffset = true;
      return bounds;
    }
  }

  /// Finalize bounds if we found any constant offsets
  if (foundAny) {
    /// Classify as stencil if we found multiple distinct offsets
    bounds.isStencil = (bounds.minOffset != bounds.maxOffset);
    bounds.valid = true;
    /// Normalize to center around zero for easier halo computation
    normalizeAccessBounds(bounds);
  }

  return bounds;
}
