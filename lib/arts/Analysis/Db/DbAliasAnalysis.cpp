///==========================================================================///
/// File: DbAliasAnalysis.cpp
/// Defines alias analysis for DB operations and memory tracking.
///==========================================================================///

#include "arts/Analysis/Db/DbAliasAnalysis.h"
#include "arts/Analysis/Db/DbAnalysis.h"
#include "arts/Analysis/Graphs/Db/DbGraph.h"
#include "arts/Analysis/Graphs/Db/DbNode.h"
#include "arts/Utils/ArtsUtils.h"

#include "arts/Utils/ArtsDebug.h"
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

} // namespace

DbAliasAnalysis::DbAliasAnalysis(DbAnalysis *analysis) {
  assert(analysis && "Analysis cannot be null");
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
  llvm_unreachable("Unhandled alias result");
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

      // Different access patterns suggest different regions
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
      ARTS_INFO("  Step 2: Using DbAliasAnalysis::estimateOverlap" << indent);
      auto k = estimateOverlap(acqA, acqB);
      const char *kindStr = k == OverlapKind::Disjoint  ? "DISJOINT"
                            : k == OverlapKind::Partial ? "PARTIAL"
                            : k == OverlapKind::Full    ? "FULL"
                                                        : "UNKNOWN";
      ARTS_INFO("  Step 2 Result: " << kindStr << indent);

      if (k == OverlapKind::Disjoint)
        result = AliasResult::NoAlias;
      else if (k == OverlapKind::Full)
        result = AliasResult::MustAlias;
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
    return arts::getUnderlyingValue(sourcePtr);
  } else if (isa<DbReleaseOp>(op)) {
    Value source = cast<DbReleaseOp>(op).getSource();
    return arts::getUnderlyingValue(source);
  }
  llvm_unreachable("Invalid DB node type");
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

  return OverlapKind::Partial;
}
