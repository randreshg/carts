///==========================================================================///
/// File: HeuristicsConfig.cpp
///
/// Centralized heuristics configuration and evaluation for the CARTS compiler.
///
/// This file implements:
/// - H1.x partitioning heuristics (evaluatePartitioningHeuristics)
/// - H2 twin-diff heuristics (shouldUseTwinDiff)
/// - Decision recording for ArtsMate diagnostics
///==========================================================================///

#include "arts/Analysis/HeuristicsConfig.h"
#include "arts/ArtsDialect.h"
#include "arts/Transforms/Datablock/DbRewriter.h"
#include "arts/Utils/ArtsDebug.h"
#include "arts/Utils/DatablockUtils.h"
#include "arts/Utils/Metadata/IdRegistry.h"
#include "arts/Utils/Metadata/LocationMetadata.h"
#include "arts/Utils/ValueUtils.h"

ARTS_DEBUG_SETUP(heuristics)

using namespace mlir;
using namespace mlir::arts;

///===----------------------------------------------------------------------===///
/// Helper Functions
///===----------------------------------------------------------------------===///

namespace {

/// Extract heuristic ID from rationale string (e.g., "H1.2: ..." -> "H1.2")
static std::string extractHeuristicId(llvm::StringRef rationale) {
  /// Look for "H1.X:" pattern
  if (rationale.starts_with("H1.")) {
    size_t colonPos = rationale.find(':');
    if (colonPos != std::string::npos)
      return rationale.substr(0, colonPos).str();
  }
  return "Fallback";
}

} // namespace

///===----------------------------------------------------------------------===///
/// H1: Partitioning Heuristics Evaluation
///===----------------------------------------------------------------------===///

PartitioningDecision
mlir::arts::evaluatePartitioningHeuristics(const PartitioningContext &ctx,
                                           const ArtsAbstractMachine *machine,
                                           PartitionFallback fallback) {
  ARTS_DEBUG("evaluatePartitioningHeuristics: canElementWise="
             << ctx.canElementWise << ", canChunked=" << ctx.canChunked
             << ", pinnedDimCount=" << ctx.pinnedDimCount
             << ", accessMode=" << static_cast<int>(ctx.accessMode));

  /// H1.1: Read-Only Single-Node -> Coarse
  /// On a single node with read-only access, there is no write contention to
  /// relieve via fine-graining. Coarse allocation reduces datablock count.
  if (machine && machine->getNodeCount() == 1) {
    bool isReadOnly = !ctx.acquires.empty() ? ctx.allReadOnly()
                                            : (ctx.accessMode == ArtsMode::in);
    if (isReadOnly && !ctx.canChunked && !ctx.canElementWise) {
      ARTS_DEBUG("H1.1 applied: Read-only single-node prefers coarse");
      return PartitioningDecision::coarse(
          ctx, "H1.1: All acquires read-only on single-node prefers coarse");
    }
  }

  /// H1.2: Mixed Access (Chunked Writes + Indirect Reads) -> Chunked
  /// Mixed access patterns use chunked partitioning with full-range acquires
  /// for indirect reads. This avoids DbCopy/DbSync overhead.
  if (ctx.canChunked && ctx.hasIndirectRead && !ctx.hasIndirectWrite &&
      ctx.hasDirectAccess) {
    ARTS_DEBUG("H1.2 applied: Mixed access pattern");
    return PartitioningDecision::chunked(
        ctx, "H1.2: Mixed access (chunked writes + full-range indirect reads)");
  }

  /// H1.3: Stencil/Indexed Patterns
  /// - Indexed access (data-dependent) requires element-wise partitioning
  /// - Stencil patterns use Stencil mode (chunked + ESD) for halo handling
  const auto &patterns = ctx.accessPatterns;

  /// Case 1: Indexed access -> Element-wise (unpredictable access pattern)
  if (patterns.hasIndexed) {
    ARTS_DEBUG("H1.3 applied: Indexed access requires element-wise");
    return PartitioningDecision::elementWise(
        ctx, 1, "H1.3: Indexed access requires element-wise");
  }

  /// Case 2: Stencil (with or without uniform) -> Stencil mode (chunked + ESD)
  if (patterns.hasStencil) {
    if (!ctx.canChunked) {
      ARTS_DEBUG("H1.3 applied: Stencil but chunked unsupported; fallback");
      return PartitioningDecision::elementWise(
          ctx, 1,
          "H1.3: Stencil detected but chunked unsupported; fallback to "
          "element-wise");
    }

    ARTS_DEBUG("H1.3 applied: Stencil uses ESD mode");
    return PartitioningDecision::stencil(
        ctx, patterns.hasUniform
                 ? "H1.3: Mixed (uniform+stencil) uses Stencil mode - chunked "
                   "handles both"
                 : "H1.3: Pure stencil uses ESD mode");
  }

  /// H1.4: Multi-Node -> Fine-Grained for Network Efficiency
  /// On multi-node systems, fine-grained partitioning reduces data transfer
  /// by allowing nodes to acquire only the data they need.
  if (machine && machine->getNodeCount() > 1) {
    bool canDoChunked = ctx.canChunked || ctx.anyCanChunked();
    bool canDoElementWise = ctx.canElementWise || ctx.anyCanElementWise();

    if (canDoChunked) {
      ARTS_DEBUG("H1.4 applied: Multi-node prefers chunked");
      return PartitioningDecision::chunked(
          ctx, "H1.4: Multi-node prefers chunked for network efficiency");
    }

    if (canDoElementWise) {
      ARTS_DEBUG("H1.4 applied: Multi-node prefers fine-grained");
      unsigned outerRank =
          ctx.pinnedDimCount > 0 ? ctx.pinnedDimCount : ctx.maxPinnedDimCount();
      outerRank = outerRank > 0 ? outerRank : 1;
      return PartitioningDecision::elementWise(
          ctx, outerRank, "H1.4: Multi-node prefers fine-grained");
    }
  }

  /// H1.5: Non-Uniform Access -> Coarse
  /// When access patterns are non-uniform and no fine-grained option is
  /// available, default to coarse allocation to avoid complexity.
  if (!ctx.isUniformAccess && !ctx.canElementWise && !ctx.canChunked) {
    ARTS_DEBUG("H1.5 applied: Non-uniform access prefers coarse");
    return PartitioningDecision::coarse(
        ctx, "H1.5: Non-uniform access prefers coarse");
  }

  /// Fallback: Respect user partition-fallback preference
  if (fallback == PartitionFallback::FineGrained) {
    ARTS_DEBUG("Fallback applied: No heuristic matched, using fine-grained "
               "(user preference)");
    return PartitioningDecision::elementWise(
        ctx, 1, "Fallback: User preference for fine-grained partitioning");
  }

  ARTS_DEBUG("Fallback applied: No heuristic matched, using coarse (user "
             "preference)");
  return PartitioningDecision::coarse(
      ctx, "Fallback: User preference for coarse partitioning");
}

//===----------------------------------------------------------------------===//
// HeuristicsConfig Implementation
//===----------------------------------------------------------------------===//

HeuristicsConfig::HeuristicsConfig(const ArtsAbstractMachine &machine,
                                   IdRegistry &registry,
                                   PartitionFallback fallback)
    : machine(machine), idRegistry(registry), partitionFallback(fallback) {
  ARTS_DEBUG("HeuristicsConfig initialized: singleNode="
             << isSingleNode() << ", valid=" << isValid());
}

bool HeuristicsConfig::isSingleNode() const {
  return machine.getNodeCount() == 1;
}

bool HeuristicsConfig::isValid() const { return machine.isValid(); }

//===----------------------------------------------------------------------===//
// H2: Twin-Diff Heuristic
//===----------------------------------------------------------------------===//

bool HeuristicsConfig::shouldUseTwinDiff(const TwinDiffContext &context) {
  Operation *op = context.op;

  /// Temporary global disable: twin-diff is not yet fully supported.
  /// When re-enabled, remove this early return and the #if 0 block below.
  recordDecision("TwinDiff-Disabled", true,
                 "twin-diff disabled globally (temporary)", op, {});
  return false;

  /// Twin-diff heuristics (disabled - to be re-enabled when runtime supports
  /// it)
#if 0
  /// H2: Single-node disables twin-diff unconditionally
  /// - No remote owner to send diffs to
  /// - Twin allocation + memcpy + diff computation are wasted
  if (isSingleNode()) {
    recordDecision("H2-TwinDiff", true, "single-node disables twin-diff", op,
                   {});
    return false;
  }

  /// Proof-based decisions (multi-node only)
  switch (context.proof) {
  case TwinDiffProof::IndexedControl:
    recordDecision("TwinDiff-IndexedControl", true,
                   "DbControlOps guarantee indexed access (proven isolation)",
                   op, {});
    return false;

  case TwinDiffProof::PartitionSuccess:
    recordDecision("TwinDiff-Partition", true,
                   "successful partitioning proves non-overlapping chunks", op,
                   {});
    return false;

  case TwinDiffProof::AliasAnalysis:
    recordDecision("TwinDiff-Alias", true,
                   "alias analysis proves disjoint acquires", op, {});
    return false;

  case TwinDiffProof::None:
  default:
    recordDecision(
        "TwinDiff-SafeDefault", true,
        "no proof of non-overlap, using safe default (twin_diff=true)", op, {});
    return true;
  }
#endif
}

//===----------------------------------------------------------------------===//
// Decision Recording for Diagnostics
//===----------------------------------------------------------------------===//

void HeuristicsConfig::recordDecision(llvm::StringRef heuristic, bool applied,
                                      llvm::StringRef rationale, Operation *op,
                                      const llvm::StringMap<int64_t> &inputs) {
  /// Get artsId from operation via IdRegistry
  int64_t artsId = op ? idRegistry.getOrCreate(op) : 0;

  /// Compute affected alloc and runtime DB IDs (PGO-style mapping)
  int64_t allocId = 0;
  SmallVector<int64_t> affectedDbIds;
  std::string sourceLocation;

  if (op) {
    /// Extract source location for ArtsMate correlation
    LocationMetadata locMeta = LocationMetadata::fromLocation(op->getLoc());
    if (locMeta.isValid())
      sourceLocation = locMeta.file + ":" + std::to_string(locMeta.line);

    /// For DbAcquireOp: compute affected runtime DB IDs
    if (auto acquireOp = dyn_cast<DbAcquireOp>(op)) {
      /// Trace back to parent DbAllocOp
      if (Operation *allocOp =
              DatablockUtils::getUnderlyingDbAlloc(acquireOp.getSourcePtr())) {
        /// Use getOrCreate to ensure the alloc has an ID (it may not be
        /// assigned yet if we're in the middle of CreateDbs pass)
        allocId = idRegistry.getOrCreate(allocOp);

        /// Compute affected DB IDs based on offsets and sizes
        /// Runtime DB ID = allocId + offset (for 1D case)
        ValueRange offsets = acquireOp.getOffsets();
        ValueRange sizes = acquireOp.getSizes();

        if (!offsets.empty() && !sizes.empty()) {
          int64_t offset = 0, count = 0;
          bool offsetKnown = ValueUtils::getConstantIndex(offsets[0], offset);
          bool sizeKnown = ValueUtils::getConstantIndex(sizes[0], count);

          if (offsetKnown && sizeKnown && allocId != 0) {
            /// Compute exact affected DB IDs: [allocId + offset, allocId +
            /// offset + count)
            for (int64_t i = 0; i < count; i++) {
              affectedDbIds.push_back(allocId + offset + i);
            }
            ARTS_DEBUG("  Computed affected_db_ids: ["
                       << allocId + offset << ", " << allocId + offset + count
                       << ")");
          } else {
            /// Dynamic offset/size - can't compute exact IDs
            /// Record just the base for partial correlation
            if (allocId != 0)
              affectedDbIds.push_back(allocId); /// At minimum, base is affected
            ARTS_DEBUG("  Dynamic offset/size - partial correlation only");
          }
        } else if (allocId != 0) {
          /// No offsets = acquires entire grid (coarse or all DBs)
          /// Get sizes from the parent DbAllocOp to compute all affected IDs
          if (auto dbAlloc = dyn_cast<DbAllocOp>(allocOp)) {
            ValueRange allocSizes = dbAlloc.getSizes();
            if (!allocSizes.empty()) {
              int64_t totalDBs = 1;
              bool allStatic = true;
              for (Value sz : allocSizes) {
                int64_t dim;
                if (ValueUtils::getConstantIndex(sz, dim))
                  totalDBs *= dim;
                else
                  allStatic = false;
              }
              if (allStatic) {
                for (int64_t i = 0; i < totalDBs; i++)
                  affectedDbIds.push_back(allocId + i);
                ARTS_DEBUG("  Acquires all DBs: ["
                           << allocId << ", " << allocId + totalDBs << ")");
              }
            }
          }
        }
      }
    }
  }

  decisions.push_back({heuristic.str(), applied, rationale.str(), artsId,
                       allocId, std::move(affectedDbIds), sourceLocation,
                       inputs});

  ARTS_DEBUG("Recorded heuristic decision: "
             << heuristic << " applied=" << applied << " for ARTS ID " << artsId
             << " (allocId=" << allocId << ")");
}

llvm::ArrayRef<HeuristicDecision> HeuristicsConfig::getDecisions() const {
  return decisions;
}

//===----------------------------------------------------------------------===//
// H1: Partitioning Mode Evaluation
//===----------------------------------------------------------------------===//

PartitioningDecision
HeuristicsConfig::getPartitioningMode(const PartitioningContext &ctx) {
  ARTS_DEBUG("getPartitioningMode: canElementWise="
             << ctx.canElementWise << ", canChunked=" << ctx.canChunked
             << ", pinnedDimCount=" << ctx.pinnedDimCount
             << ", accessMode=" << static_cast<int>(ctx.accessMode));

  /// Call the unified heuristics evaluation function
  auto decision =
      evaluatePartitioningHeuristics(ctx, &machine, partitionFallback);

  /// Record the decision for diagnostics (extract heuristic ID from rationale)
  std::string heuristicId = extractHeuristicId(decision.rationale);
  recordDecision(heuristicId, true, decision.rationale, nullptr, {});

  ARTS_DEBUG("Heuristic " << heuristicId << " applied: " << decision.rationale);
  return decision;
}

//===----------------------------------------------------------------------===//
// JSON Export for ArtsMate
//===----------------------------------------------------------------------===//

void HeuristicsConfig::exportDecisionsToJson(llvm::json::OStream &J) const {
  J.attributeArray("heuristic_decisions", [&]() {
    for (const auto &d : decisions) {
      J.object([&]() {
        J.attribute("heuristic", d.heuristic);
        J.attribute("applied", d.applied);
        J.attribute("rationale", d.rationale);
        J.attribute("target_id", d.affectedArtsId);

        /// PGO-style mapping: compile-time -> runtime correlation
        if (d.affectedAllocId != 0)
          J.attribute("alloc_id", d.affectedAllocId);

        if (!d.affectedDbIds.empty()) {
          J.attributeArray("affected_db_ids", [&]() {
            for (int64_t id : d.affectedDbIds)
              J.value(id);
          });
        }

        if (!d.sourceLocation.empty())
          J.attribute("source_location", d.sourceLocation);

        if (!d.costModelInputs.empty()) {
          J.attributeObject("parameters", [&]() {
            for (const auto &input : d.costModelInputs)
              J.attribute(input.first(), input.second);
          });
        }
      });
    }
  });
}
