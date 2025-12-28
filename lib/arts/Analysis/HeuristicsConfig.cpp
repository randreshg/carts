///==========================================================================///
/// File: HeuristicsConfig.cpp
///
/// This file implements the HeuristicsConfig class which centralizes all
/// compile-time heuristic decision logic.
///==========================================================================///

#include "arts/Analysis/HeuristicsConfig.h"
#include "arts/Analysis/Heuristics/PartitioningHeuristics.h"
#include "arts/Transforms/Datablock/DbRewriter.h"
#include "arts/Utils/ArtsDebug.h"
#include "arts/Utils/DatablockUtils.h"
#include "arts/Utils/Metadata/IdRegistry.h"
#include "arts/Utils/Metadata/LocationMetadata.h"
#include "arts/Utils/ValueUtils.h"
#include "llvm/ADT/STLExtras.h"

ARTS_DEBUG_SETUP(heuristics)

using namespace mlir;
using namespace mlir::arts;

namespace {
/// Compute minimum inner bytes based on execution mode
static int64_t computeMinInnerBytes(const ArtsAbstractMachine &machine) {
  auto mode = machine.getExecutionMode();
  switch (mode) {
  case ExecutionMode::SingleThreaded:
    return 64; /// Cache-line aligned
  case ExecutionMode::IntraNode:
    return 1024; /// Modest lower bound
  case ExecutionMode::InterNode:
    return 4096; /// Avoid tiny chunks for distributed
  }
  return 64; /// Fallback
}
} // namespace

HeuristicsConfig::HeuristicsConfig(const ArtsAbstractMachine &machine,
                                   IdRegistry &idRegistry)
    : machine(machine), idRegistry_(idRegistry) {
  initializeThresholds();
  initializeDefaultHeuristics();
  ARTS_DEBUG("HeuristicsConfig initialized: singleNode="
             << isSingleNode() << ", valid=" << isValid()
             << ", minInnerBytes=" << thresholds_.minInnerBytes
             << ", partitioningHeuristics=" << partitioningHeuristics_.size());
}

void HeuristicsConfig::initializeThresholds() {
  /// Compute thresholds based on execution mode and memory tier
  auto mode = machine.getExecutionMode();
  /// Set DB limits based on execution mode
  switch (mode) {
  case ExecutionMode::SingleThreaded:
    thresholds_.maxOuterDBs = 256;
    thresholds_.maxTotalDBsPerEDT = 64;
    thresholds_.minElementsPerChunk = 4;
    break;
  case ExecutionMode::IntraNode:
    thresholds_.maxOuterDBs = 512;
    thresholds_.maxTotalDBsPerEDT = 256;
    thresholds_.minElementsPerChunk = 16;
    break;
  case ExecutionMode::InterNode:
    thresholds_.maxOuterDBs = 2048;
    thresholds_.maxTotalDBsPerEDT = 1024;
    thresholds_.minElementsPerChunk = 64;
    break;
  }

  /// Compute memory-aware thresholds
  thresholds_.minInnerBytes = computeMinInnerBytes(machine);

  ARTS_DEBUG("Thresholds initialized: maxOuterDBs="
             << thresholds_.maxOuterDBs
             << ", maxTotalDBsPerEDT=" << thresholds_.maxTotalDBsPerEDT
             << ", minInnerBytes=" << thresholds_.minInnerBytes);
}

bool HeuristicsConfig::isSingleNode() const {
  return machine.getNodeCount() == 1;
}

bool HeuristicsConfig::isValid() const { return machine.isValid(); }

//===----------------------------------------------------------------------===//
// H4: Twin-Diff Heuristic
//===----------------------------------------------------------------------===//

bool HeuristicsConfig::shouldUseTwinDiff(const TwinDiffContext &context) {
  Operation *op = context.op;

  /// H4: Single-node disables twin-diff unconditionally
  /// - No remote owner to send diffs to
  /// - Twin allocation + memcpy + diff computation are wasted
  if (isSingleNode()) {
    recordDecision("H4-TwinDiff", true, "single-node disables twin-diff", op,
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
    return true; // Safe default - handles potential overlap at runtime
  }
}

//===----------------------------------------------------------------------===//
// Decision Recording for Diagnostics
//===----------------------------------------------------------------------===//

void HeuristicsConfig::recordDecision(llvm::StringRef heuristic, bool applied,
                                      llvm::StringRef rationale, Operation *op,
                                      const llvm::StringMap<int64_t> &inputs) {
  /// Get artsId from operation via IdRegistry
  int64_t artsId = op ? idRegistry_.getOrCreate(op) : 0;

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
        allocId = idRegistry_.getOrCreate(allocOp);

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

  decisions_.push_back({heuristic.str(), applied, rationale.str(), artsId,
                        allocId, std::move(affectedDbIds), sourceLocation,
                        inputs});

  ARTS_DEBUG("Recorded heuristic decision: "
             << heuristic << " applied=" << applied << " for ARTS ID " << artsId
             << " (allocId=" << allocId << ")");
}

llvm::ArrayRef<HeuristicDecision> HeuristicsConfig::getDecisions() const {
  return decisions_;
}

//===----------------------------------------------------------------------===//
// H1: Unified Partitioning Heuristics Registry
//===----------------------------------------------------------------------===//

void HeuristicsConfig::initializeDefaultHeuristics() {
  // Get all default partitioning heuristics from factory
  auto defaults = createDefaultPartitioningHeuristics();
  for (auto &h : defaults) {
    registerHeuristic(std::move(h));
  }

  // Sort by priority (highest first)
  llvm::sort(partitioningHeuristics_, [](const auto &a, const auto &b) {
    return a->getPriority() > b->getPriority();
  });

  ARTS_DEBUG("Registered " << partitioningHeuristics_.size()
                           << " partitioning heuristics");
  for (const auto &h : partitioningHeuristics_) {
    ARTS_DEBUG("  - " << h->getName() << " (priority=" << h->getPriority()
                      << "): " << h->getDescription());
  }
}

void HeuristicsConfig::registerHeuristic(
    std::unique_ptr<PartitioningHeuristic> h) {
  ARTS_DEBUG("Registering heuristic: " << h->getName());
  partitioningHeuristics_.push_back(std::move(h));
}

PartitioningDecision
HeuristicsConfig::getPartitioningMode(const PartitioningContext &ctx) {
  ARTS_DEBUG("getPartitioningMode: canElementWise="
             << ctx.canElementWise << ", canChunked=" << ctx.canChunked
             << ", pinnedDimCount=" << ctx.pinnedDimCount
             << ", accessMode=" << static_cast<int>(ctx.accessMode));

  // Evaluate heuristics in priority order
  for (const auto &heuristic : partitioningHeuristics_) {
    auto decision = heuristic->evaluate(ctx);
    if (decision.has_value()) {
      // Record the decision for diagnostics
      recordDecision(heuristic->getName(), true, decision->rationale, nullptr,
                     {});
      ARTS_DEBUG("Heuristic " << heuristic->getName()
                              << " applied: " << decision->rationale);
      return *decision;
    }
  }

  // Fallback: coarse allocation
  PartitioningDecision fallback;
  fallback.mode = RewriterMode::ElementWise;
  fallback.outerRank = 0;
  fallback.innerRank = ctx.memrefRank; // All dims are inner for coarse
  fallback.rationale = "No heuristic applied, using coarse fallback";
  recordDecision("Fallback", true, fallback.rationale, nullptr, {});
  ARTS_DEBUG("Fallback applied: " << fallback.rationale);
  return fallback;
}

void HeuristicsConfig::exportDecisionsToJson(llvm::json::OStream &J) const {
  J.attributeArray("heuristic_decisions", [&]() {
    for (const auto &decision : decisions_) {
      J.object([&]() {
        J.attribute("heuristic", decision.heuristic);
        J.attribute("applied", decision.applied);
        J.attribute("rationale", decision.rationale);
        J.attribute("target_id", decision.affectedArtsId);

        /// PGO-style mapping: compile-time → runtime correlation
        if (decision.affectedAllocId != 0)
          J.attribute("alloc_id", decision.affectedAllocId);

        if (!decision.affectedDbIds.empty()) {
          J.attributeArray("affected_db_ids", [&]() {
            for (int64_t id : decision.affectedDbIds)
              J.value(id);
          });
        }

        if (!decision.sourceLocation.empty())
          J.attribute("source_location", decision.sourceLocation);

        if (!decision.costModelInputs.empty()) {
          J.attributeObject("parameters", [&]() {
            for (const auto &input : decision.costModelInputs) {
              J.attribute(input.first(), input.second);
            }
          });
        }
      });
    }
  });
}
