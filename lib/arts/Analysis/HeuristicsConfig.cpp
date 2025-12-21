///==========================================================================///
/// File: HeuristicsConfig.cpp
///
/// This file implements the HeuristicsConfig class which centralizes all
/// compile-time heuristic decision logic for single-rank and other
/// optimizations in the CARTS compiler framework.
///==========================================================================///

#include "arts/Analysis/HeuristicsConfig.h"
#include "arts/Utils/ArtsDebug.h"
#include "arts/Utils/DatablockUtils.h"
#include "arts/Utils/Metadata/IdRegistry.h"
#include "arts/Utils/Metadata/LocationMetadata.h"
#include "arts/Utils/ValueUtils.h"

ARTS_DEBUG_SETUP(heuristics)

using namespace mlir;
using namespace mlir::arts;

HeuristicsConfig::HeuristicsConfig(const ArtsAbstractMachine &machine,
                                   IdRegistry &idRegistry)
    : machine(machine), idRegistry_(idRegistry) {
  initializeThresholds();
  ARTS_DEBUG("HeuristicsConfig initialized: singleNode="
             << isSingleNode() << ", valid=" << isValid()
             << ", minInnerBytes=" << thresholds_.minInnerBytes);
}

void HeuristicsConfig::initializeThresholds() {
  /// Compute thresholds based on execution mode and memory tier
  auto mode = machine.getExecutionMode();
  const auto &mem = machine.getMemoryConfig();

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
  thresholds_.minInnerBytes = computeMinInnerBytes();
  thresholds_.optimalInnerBytes = computeOptimalInnerBytes();

  ARTS_DEBUG("Thresholds initialized: maxOuterDBs="
             << thresholds_.maxOuterDBs
             << ", maxTotalDBsPerEDT=" << thresholds_.maxTotalDBsPerEDT
             << ", minInnerBytes=" << thresholds_.minInnerBytes
             << ", optimalInnerBytes=" << thresholds_.optimalInnerBytes);
}

int64_t HeuristicsConfig::computeMinInnerBytes() const {
  auto mode = machine.getExecutionMode();
  const auto &mem = machine.getMemoryConfig();

  switch (mode) {
  case ExecutionMode::SingleThreaded:
    /// Cache-line aligned (64 bytes)
    return 64;
  case ExecutionMode::IntraNode:
    /// L2 per thread: fit chunk in per-core cache
    return (mem.l2OrderKB * 1024) / std::max(1, machine.getThreads());
  case ExecutionMode::InterNode:
    /// Amortize network: latency × bandwidth
    /// latencyUs * (Mbps / 8) = bytes to transfer during one RTT
    return (mem.latencyOrderUs * mem.networkOrderMbps) / 8;
  }
  return 64; /// Fallback
}

int64_t HeuristicsConfig::computeOptimalInnerBytes() const {
  auto mode = machine.getExecutionMode();
  const auto &mem = machine.getMemoryConfig();

  switch (mode) {
  case ExecutionMode::SingleThreaded:
    return 256; /// 4 cache lines
  case ExecutionMode::IntraNode:
    /// Half of L2 per thread for optimal cache usage
    return (mem.l2OrderKB * 1024) / (2 * std::max(1, machine.getThreads()));
  case ExecutionMode::InterNode:
    /// L3 shared across nodes
    return (mem.l3OrderMB * 1024 * 1024) / std::max(1, machine.getNodeCount());
  }
  return 256; /// Fallback
}

bool HeuristicsConfig::isSingleNode() const {
  return machine.getNodeCount() == 1;
}

bool HeuristicsConfig::isValid() const { return machine.isValid(); }

bool HeuristicsConfig::shouldDisableTwinDiff() const {
  /// H4: On single-node, twin_diff is pure overhead
  /// - No remote owner to send diffs to
  /// - Twin allocation + memcpy + diff computation are wasted
  return isSingleNode();
}

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

bool HeuristicsConfig::shouldPreferCoarseForReadOnly(
    ArtsMode accessMode) const {
  /// H1: Read-only memrefs on single-node should prefer coarse allocation
  /// - No write contention to relieve via fine-graining
  /// - Coarse reduces DB count and per-EDT dependency overhead
  return isSingleNode() && accessMode == ArtsMode::in;
}

std::optional<bool> HeuristicsConfig::shouldUseFineGrained(
    std::optional<int64_t> totalDBs, std::optional<int64_t> totalDBsPerEDT,
    std::optional<int64_t> innerBytes, PartitioningSource source) const {
  /// H2: Cost model for fine-grained allocation with partial eval
  /// Uses reject-early semantics: if any known value fails, return false.
  /// If all known values pass but some unknown, return nullopt.

  /// For ParallelFor source: be more permissive (prioritize chunking)
  /// For UserProvided source: this shouldn't be called (handled at call site)
  bool prioritizeChunking = (source == PartitioningSource::ParallelFor);

  ARTS_DEBUG("  H2: source=" << static_cast<int>(source)
                             << ", prioritizeChunking=" << prioritizeChunking);

  /// Multi-node: always fine-grained (chunking for network efficiency)
  if (!isSingleNode()) {
    ARTS_DEBUG("  H2: multi-node -> fine-grained (chunking)");
    return true;
  }

  /// Get mode-dependent thresholds
  int64_t maxDBs = thresholds_.maxOuterDBs;
  int64_t maxDBsPerEDT = thresholds_.maxTotalDBsPerEDT;
  int64_t minBytes = thresholds_.minInnerBytes;

  /// REJECT EARLY: If any known value fails its check, return false immediately
  if (totalDBs.has_value() && *totalDBs > maxDBs) {
    ARTS_DEBUG("  H2 REJECT: totalDBs=" << *totalDBs << " > " << maxDBs);
    return false;
  }
  if (totalDBsPerEDT.has_value() && *totalDBsPerEDT > maxDBsPerEDT) {
    ARTS_DEBUG("  H2 REJECT: totalDBsPerEDT=" << *totalDBsPerEDT << " > "
                                              << maxDBsPerEDT);
    return false;
  }
  if (innerBytes.has_value() && *innerBytes < minBytes) {
    /// For ParallelFor, only reject if MUCH smaller than threshold
    if (!prioritizeChunking || *innerBytes < (minBytes / 4)) {
      ARTS_DEBUG("  H2 REJECT: innerBytes=" << *innerBytes << " < "
                                            << minBytes);
      return false;
    }
    ARTS_DEBUG("  H2 WARNING: innerBytes="
               << *innerBytes << " < " << minBytes
               << " but allowing due to ParallelFor priority");
  }

  /// If ALL values known and ALL passed, return true
  if (totalDBs && totalDBsPerEDT && innerBytes) {
    ARTS_DEBUG("  H2 APPROVE: totalDBs=" << *totalDBs << ", totalDBsPerEDT="
                                         << *totalDBsPerEDT
                                         << ", innerBytes=" << *innerBytes);
    return true;
  }

  /// Some values unknown but no known failures
  /// For ParallelFor: be optimistic and approve chunking
  if (prioritizeChunking) {
    ARTS_DEBUG("  H2 OPTIMISTIC: ParallelFor -> approve chunking despite "
               "unknown values");
    return true;
  }

  /// Default: can't fully decide
  ARTS_DEBUG("  H2 PARTIAL: known values pass, but some unknown"
             << " (totalDBs=" << (totalDBs ? std::to_string(*totalDBs) : "?")
             << ", totalDBsPerEDT="
             << (totalDBsPerEDT ? std::to_string(*totalDBsPerEDT) : "?")
             << ", innerBytes="
             << (innerBytes ? std::to_string(*innerBytes) : "?") << ")");
  return std::nullopt;
}

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
