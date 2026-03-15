///==========================================================================///
/// File: DbAliasAnalysis.cpp
/// Defines alias analysis for DB operations and memory tracking.
///==========================================================================///

#include "arts/analysis/db/DbAliasAnalysis.h"
#include "arts/analysis/db/DbAnalysis.h"
#include "arts/analysis/graphs/db/DbGraph.h"
#include "arts/analysis/graphs/db/DbNode.h"
#include "arts/analysis/value/ValueAnalysis.h"
#include "arts/utils/Utils.h"

#include "arts/utils/Debug.h"
ARTS_DEBUG_SETUP(db_alias);

#include <algorithm>
#include <optional>
#include <utility>

using namespace mlir;
using namespace mlir::arts;

namespace {
std::pair<Value, Value> makeOrderedPair(Value a, Value b) {
  /// Use pointer comparison for ordering
  return a.getAsOpaquePointer() < b.getAsOpaquePointer() ? std::make_pair(a, b)
                                                         : std::make_pair(b, a);
}

struct ConstantSlice {
  SmallVector<int64_t, 4> offsets;
  SmallVector<int64_t, 4> sizes;
};

std::optional<ConstantSlice> extractConstantSlice(DbAcquireOp op) {
  ConstantSlice slice;
  auto extractRange = [&](ValueRange range,
                          SmallVectorImpl<int64_t> &storage) -> bool {
    storage.clear();
    storage.reserve(range.size());
    for (Value v : range) {
      int64_t cst = 0;
      if (!ValueAnalysis::getConstantIndex(v, cst))
        return false;
      storage.push_back(cst);
    }
    return true;
  };

  if (!extractRange(op.getOffsets(), slice.offsets))
    return std::nullopt;
  if (!extractRange(op.getSizes(), slice.sizes))
    return std::nullopt;
  return slice;
}

std::optional<DbAliasAnalysis::AliasResult>
refineAliasWithSlices(const DbAcquireNode *a, const DbAcquireNode *b) {
  if (!a || !b)
    return std::nullopt;
  auto sliceA = extractConstantSlice(a->getDbAcquireOp());
  auto sliceB = extractConstantSlice(b->getDbAcquireOp());
  if (!sliceA || !sliceB)
    return std::nullopt;

  /// Normalize dimensionality
  if (sliceA->offsets.size() != sliceB->offsets.size() ||
      sliceA->sizes.size() != sliceB->sizes.size())
    return std::nullopt;

  bool identical =
      sliceA->offsets == sliceB->offsets && sliceA->sizes == sliceB->sizes;
  if (identical)
    return DbAliasAnalysis::AliasResult::MustAlias;

  bool disjoint = false;
  for (size_t dim = 0; dim < sliceA->sizes.size(); ++dim) {
    int64_t beginA = sliceA->offsets[dim];
    int64_t endA = beginA + sliceA->sizes[dim];
    int64_t beginB = sliceB->offsets[dim];
    int64_t endB = beginB + sliceB->sizes[dim];
    if (endA <= beginB || endB <= beginA) {
      disjoint = true;
      break;
    }
  }

  if (disjoint)
    return DbAliasAnalysis::AliasResult::NoAlias;
  return DbAliasAnalysis::AliasResult::MayAlias;
}

/// Refine overlap using partition offsets/sizes from DbAcquireOp.
/// These are element-space partition hints (from depend clauses) and are more
/// reliable than beginIndex/endIndex ordering counters for proving
/// disjointness.
std::optional<DbAliasAnalysis::OverlapKind>
refineOverlapWithPartitionRanges(const DbAcquireNode *a,
                                 const DbAcquireNode *b) {
  DbAcquireOp opA = a->getDbAcquireOp();
  DbAcquireOp opB = b->getDbAcquireOp();
  if (!opA || !opB)
    return std::nullopt;

  ValueRange partOffsA = opA.getPartitionOffsets();
  ValueRange partSzA = opA.getPartitionSizes();
  ValueRange partOffsB = opB.getPartitionOffsets();
  ValueRange partSzB = opB.getPartitionSizes();

  if (partOffsA.empty() || partOffsB.empty())
    return std::nullopt;
  if (partOffsA.size() != partOffsB.size())
    return std::nullopt;

  /// If any dimension is provably disjoint, the regions are disjoint.
  for (size_t dim = 0; dim < partOffsA.size(); ++dim) {
    int64_t offA = 0, szA = 0, offB = 0, szB = 0;
    bool aConst = ValueAnalysis::getConstantIndex(partOffsA[dim], offA) &&
                  dim < partSzA.size() &&
                  ValueAnalysis::getConstantIndex(partSzA[dim], szA);
    bool bConst = ValueAnalysis::getConstantIndex(partOffsB[dim], offB) &&
                  dim < partSzB.size() &&
                  ValueAnalysis::getConstantIndex(partSzB[dim], szB);
    if (!aConst || !bConst)
      continue;

    /// Disjoint if [offA, offA+szA) and [offB, offB+szB) do not overlap.
    if (offA + szA <= offB || offB + szB <= offA)
      return DbAliasAnalysis::OverlapKind::Disjoint;
  }
  return std::nullopt;
}

} // namespace

DbAliasAnalysis::DbAliasAnalysis(DbAnalysis *analysis) {
  assert(analysis && "Analysis cannot be null");
  dbAnalysis = analysis;
}

static const char *aliasResultToString(DbAliasAnalysis::AliasResult res) {
  switch (res) {
  case DbAliasAnalysis::AliasResult::NoAlias:
    return "NO ALIAS";
  case DbAliasAnalysis::AliasResult::MustAlias:
    return "MUST ALIAS";
  case DbAliasAnalysis::AliasResult::MayAlias:
    return "MAY ALIAS";
  }
  ARTS_UNREACHABLE("Unhandled alias result");
}

///===----------------------------------------------------------------------===///
/// Alias Classification - Main Entry Point
///
/// This method implements a multi-stage alias analysis pipeline:
/// 1. Check cache for previous result
/// 2. Compare underlying allocation roots
/// 3. Analyze slice information (offsets, sizes)
/// 4. Apply metadata-based refinement
/// 5. Use access pattern analysis
/// 6. Perform detailed offset range comparison
/// 7. Fall back to conservative overlap estimation
///
/// Returns: NoAlias (definitely disjoint), MustAlias (identical), or
///          MayAlias (possibly overlapping, conservative)
///===----------------------------------------------------------------------===///
DbAliasAnalysis::AliasResult
DbAliasAnalysis::classifyAlias(const NodeBase &a, const NodeBase &b,
                               const std::string &indent) {
  Value ptrA = getUnderlyingValue(a);
  Value ptrB = getUnderlyingValue(b);

  ARTS_INFO("Analyzing alias between: " << a.getHierId() << " -- "
                                        << b.getHierId());

  /// Check cache
  auto key = makeOrderedPair(ptrA, ptrB);
  auto it = aliasCache.find(key);
  if (it != aliasCache.end()) {
    ARTS_INFO("  Result: " << aliasResultToString(it->second) << " (cache hit)"
                           << indent);
    return it->second;
  }

  /// Same value aliases with itself
  if (ptrA == ptrB) {
    aliasCache[key] = AliasResult::MustAlias;
    ARTS_INFO("  Result: MUST ALIAS (same pointer)" << indent);
    return AliasResult::MustAlias;
  }

  /// Start with conservative assumption (may alias)
  AliasResult result = AliasResult::MayAlias;
  ARTS_INFO("  Step 1: Checking slice information" << indent);

  /// Use metadata for quick alias refinement
  if (result == AliasResult::MayAlias) {
    /// Get root alloc nodes for metadata access
    auto getRootAlloc = [](const NodeBase &n) -> const DbAllocNode * {
      if (auto *acq = dyn_cast<DbAcquireNode>(&n))
        return acq->getRootAlloc();
      if (auto *alloc = dyn_cast<DbAllocNode>(&n))
        return alloc;
      return nullptr;
    };

    auto *allocA = getRootAlloc(a);
    auto *allocB = getRootAlloc(b);

    /// Use metadata to quickly rule out aliases
    if (allocA && allocB && allocA != allocB) {
      bool metadataRefinement = false;

      /// Different access patterns suggest different regions
      if (allocA->dominantAccessPattern && allocB->dominantAccessPattern) {
        auto patA = *allocA->dominantAccessPattern;
        auto patB = *allocB->dominantAccessPattern;

        /// Sequential + Random -> likely disjoint
        if ((patA == AccessPatternType::Sequential &&
             patB == AccessPatternType::Random) ||
            (patA == AccessPatternType::Random &&
             patB == AccessPatternType::Sequential)) {
          ARTS_INFO("  Metadata: Different access patterns (Sequential vs "
                    "Random) suggest NoAlias"
                    << indent);
          result = AliasResult::NoAlias;
          metadataRefinement = true;
        }
      }

      /// Very different spatial locality -> likely different regions
      if (!metadataRefinement && allocA->spatialLocality &&
          allocB->spatialLocality) {
        auto locA = *allocA->spatialLocality;
        auto locB = *allocB->spatialLocality;

        if ((locA == SpatialLocalityLevel::Excellent &&
             locB == SpatialLocalityLevel::Poor) ||
            (locA == SpatialLocalityLevel::Poor &&
             locB == SpatialLocalityLevel::Excellent)) {
          ARTS_INFO("  Metadata: Very different spatial locality suggests "
                    "different regions"
                    << indent);
          /// Don't set to NoAlias, but note the hint
        }
      }

      /// Very different temporal locality -> different access times
      if (!metadataRefinement && allocA->temporalLocality &&
          allocB->temporalLocality) {
        auto tempA = *allocA->temporalLocality;
        auto tempB = *allocB->temporalLocality;

        if ((tempA == TemporalLocalityLevel::Excellent &&
             tempB == TemporalLocalityLevel::Poor) ||
            (tempA == TemporalLocalityLevel::Poor &&
             tempB == TemporalLocalityLevel::Excellent)) {
          ARTS_INFO("  Metadata: Very different temporal locality noted"
                    << indent);
        }
      }

      if (metadataRefinement) {
        ARTS_INFO("  Step 1.5 Result: Refined using metadata" << indent);
      }
    }
  }

  /// If still unknown, use our overlap estimator for acquires
  if (result == AliasResult::MayAlias) {
    const auto *acqA = dyn_cast<DbAcquireNode>(&a);
    const auto *acqB = dyn_cast<DbAcquireNode>(&b);
    if (acqA && acqB) {
      if (auto sliceResult = refineAliasWithSlices(acqA, acqB)) {
        if (*sliceResult != AliasResult::MayAlias) {
          ARTS_INFO("  Step 2: Constant slice refinement -> "
                    << aliasResultToString(*sliceResult) << indent);
          result = *sliceResult;
        }
      }
    }

    if (acqA && acqB) {
      ARTS_INFO("  Step 2a: Using DbAliasAnalysis::estimateOverlap" << indent);
      auto k = estimateOverlap(acqA, acqB);
      const char *kindStr = k == OverlapKind::Disjoint  ? "DISJOINT"
                            : k == OverlapKind::Partial ? "PARTIAL"
                            : k == OverlapKind::Full    ? "FULL"
                                                        : "UNKNOWN";
      ARTS_INFO("  Step 2a Result: " << kindStr << indent);

      if (k == OverlapKind::Disjoint)
        result = AliasResult::NoAlias;
      else if (k == OverlapKind::Full)
        result = AliasResult::MustAlias;
    }

    /// Step 2b: Partition range disjointness from depend-clause partition
    /// offsets/sizes. These are element-space ranges and may prove disjointness
    /// even when regular offsets/sizes are dynamic or absent.
    if (result == AliasResult::MayAlias && acqA && acqB) {
      if (auto partResult = refineOverlapWithPartitionRanges(acqA, acqB)) {
        if (*partResult == OverlapKind::Disjoint) {
          ARTS_INFO("  Step 2b: Partition range disjointness -> NoAlias"
                    << indent);
          result = AliasResult::NoAlias;
        }
      }
    }

    /// Step 2c: Contract-based refinement via DbAnalysis.
    /// When both acquires are on the same allocation and have ownerDims from
    /// their lowering contracts, matching ownerDims with disjoint partition
    /// ranges proves non-overlap.
    if (result == AliasResult::MayAlias && acqA && acqB && dbAnalysis) {
      auto summaryA =
          dbAnalysis->getAcquireContractSummary(acqA->getDbAcquireOp());
      auto summaryB =
          dbAnalysis->getAcquireContractSummary(acqB->getDbAcquireOp());
      if (summaryA && summaryB && !summaryA->contract.ownerDims.empty() &&
          !summaryB->contract.ownerDims.empty()) {
        /// Same ownerDims; disjointness was not proved above. Log for
        /// diagnostics.
        if (summaryA->contract.ownerDims == summaryB->contract.ownerDims) {
          ARTS_DEBUG("  Step 2c: Both acquires share ownerDims (count="
                     << summaryA->contract.ownerDims.size()
                     << "), partition on same dims");
        }
      }
    }
  }

  /// Cache the result
  aliasCache[key] = result;
  ARTS_INFO("  => FINAL RESULT: " << aliasResultToString(result) << " (cached)"
                                  << indent);
  return result;
}

Value DbAliasAnalysis::getUnderlyingValue(const NodeBase &node) {
  Operation *op = node.getOp();
  if (isa<DbAllocOp>(op)) {
    return op->getResult(0);
  } else if (isa<DbAcquireOp>(op)) {
    Value sourcePtr = cast<DbAcquireOp>(op).getSourcePtr();
    return ValueAnalysis::getUnderlyingValue(sourcePtr);
  } else if (isa<DbReleaseOp>(op)) {
    Value source = cast<DbReleaseOp>(op).getSource();
    return ValueAnalysis::getUnderlyingValue(source);
  }
  ARTS_UNREACHABLE("Invalid DB node type");
}

DbAliasAnalysis::OverlapKind
DbAliasAnalysis::estimateOverlap(const DbAcquireNode *a,
                                 const DbAcquireNode *b) {
  if (!a || !b)
    return OverlapKind::Unknown;
  if (a == b)
    return OverlapKind::Full;

  DbAllocNode *allocA = a->getRootAlloc();
  DbAllocNode *allocB = b->getRootAlloc();
  if (allocA && allocB && allocA != allocB)
    return OverlapKind::Disjoint;

  DbAcquireOp opA = a->getDbAcquireOp();
  DbAcquireOp opB = b->getDbAcquireOp();
  if (opA && opB) {
    if (opA.getPtr() == opB.getPtr())
      return OverlapKind::Full;
    if (opA.getSourcePtr() && opB.getSourcePtr() &&
        opA.getSourcePtr() == opB.getSourcePtr())
      return OverlapKind::Full;
  }

  if (a->endIndex < b->beginIndex || b->endIndex < a->beginIndex)
    return OverlapKind::Disjoint;

  /// Partition range disjointness: when both acquires have constant partition
  /// offsets and sizes on the same allocation, check if their partition ranges
  /// are provably disjoint in any dimension.
  if (opA && opB && allocA && allocB && allocA == allocB) {
    if (auto sliceResult = refineAliasWithSlices(a, b)) {
      if (*sliceResult == AliasResult::NoAlias)
        return OverlapKind::Disjoint;
      if (*sliceResult == AliasResult::MustAlias)
        return OverlapKind::Full;
    }
  }

  /// Check partition offsets/sizes (element-space hints from depend clauses).
  /// These are distinct from regular offsets/sizes and may prove disjointness
  /// even when the regular slice check above was inconclusive.
  if (auto partResult = refineOverlapWithPartitionRanges(a, b)) {
    if (*partResult == OverlapKind::Disjoint)
      return OverlapKind::Disjoint;
  }

  return OverlapKind::Partial;
}

void DbAliasAnalysis::resetCache() { aliasCache.clear(); }
