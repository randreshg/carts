///==========================================================================///
/// File: HeuristicsConfig.cpp
///
/// HeuristicsConfig infrastructure: decision recording, JSON export,
/// and delegation to partitioning heuristics.
///
/// H1.x evaluation logic has moved to PartitioningHeuristics.cpp.
///==========================================================================///

#include "arts/analysis/HeuristicsConfig.h"
#include "arts/analysis/graphs/db/DbNode.h"
#include "arts/analysis/HeuristicUtils.h"
#include "arts/ArtsDialect.h"
#include "arts/utils/Debug.h"
#include "arts/utils/DbUtils.h"
#include "arts/utils/metadata/IdRegistry.h"
#include "arts/utils/metadata/LocationMetadata.h"
#include "arts/utils/metadata/LoopMetadata.h"
#include "arts/utils/ValueUtils.h"
#include "mlir/IR/BuiltinAttributes.h"
#include <cstdlib>

ARTS_DEBUG_SETUP(heuristics)

using namespace mlir;
using namespace mlir::arts;

///===----------------------------------------------------------------------===///
/// Helper Functions
///===----------------------------------------------------------------------===///

namespace {

/// Extract heuristic ID from rationale string (e.g., "H1.2: ..." -> "H1.2")
std::string extractHeuristicId(llvm::StringRef rationale) {
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
/// HeuristicsConfig Implementation
///===----------------------------------------------------------------------===///

HeuristicsConfig::HeuristicsConfig(const AbstractMachine &machine,
                                   IdRegistry &registry,
                                   PartitionFallback fallback)
    : machine(machine), idRegistry(registry), partitionFallback(fallback) {
  ARTS_DEBUG("HeuristicsConfig initialized: singleNode="
             << isSingleNode() << ", valid=" << isValid());
}

bool HeuristicsConfig::isSingleNode() const { return machine.isSingleNode(); }

bool HeuristicsConfig::isValid() const { return machine.isValid(); }

///===----------------------------------------------------------------------===///
/// Decision Recording for Diagnostics
///===----------------------------------------------------------------------===///

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
              DbUtils::getUnderlyingDbAlloc(acquireOp.getSourcePtr())) {
        allocId = idRegistry.getOrCreate(allocOp);

        /// Compute affected DB IDs based on offsets and sizes
        ValueRange offsets = acquireOp.getOffsets();
        ValueRange sizes = acquireOp.getSizes();

        if (!offsets.empty() && !sizes.empty()) {
          int64_t offset = 0, count = 0;
          bool offsetKnown = ValueUtils::getConstantIndex(offsets[0], offset);
          bool sizeKnown = ValueUtils::getConstantIndex(sizes[0], count);

          if (offsetKnown && sizeKnown && allocId != 0) {
            for (int64_t i = 0; i < count; i++) {
              affectedDbIds.push_back(allocId + offset + i);
            }
            ARTS_DEBUG("  Computed affected_db_ids: ["
                       << allocId + offset << ", " << allocId + offset + count
                       << ")");
          } else {
            if (allocId != 0)
              affectedDbIds.push_back(allocId);
            ARTS_DEBUG("  Dynamic offset/size - partial correlation only");
          }
        } else if (allocId != 0) {
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

///===----------------------------------------------------------------------===///
/// H1: Partitioning Mode Evaluation
///===----------------------------------------------------------------------===///

PartitioningDecision
HeuristicsConfig::getPartitioningMode(const PartitioningContext &ctx) {
  ARTS_DEBUG("getPartitioningMode: canElementWise="
             << ctx.canElementWise << ", canBlock=" << ctx.canBlock
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

///===----------------------------------------------------------------------===///
/// H1.7: Per-Acquire Decisions
///===----------------------------------------------------------------------===///

SmallVector<AcquireDecision>
HeuristicsConfig::getAcquireDecisions(
    ArrayRef<const DbAcquirePartitionFacts *> acquireFacts) {
  SmallVector<AcquireDecision> decisions;
  decisions.reserve(acquireFacts.size());

  for (const DbAcquirePartitionFacts *facts : acquireFacts) {
    AcquireDecision decision;
    if (!facts) {
      decisions.push_back(decision);
      continue;
    }

    bool preserveDistributionContract =
        !facts->partitionDims.empty() &&
        llvm::any_of(facts->entries, [](const DbPartitionEntryFact &entry) {
          return entry.preservesDistributedContract;
        });
    bool needsFullRange =
        llvm::any_of(facts->entries, [](const DbPartitionEntryFact &entry) {
          return entry.needsFullRange;
        });

    if (preserveDistributionContract) {
      ARTS_DEBUG("H1.7: preserving distributed acquire without forcing "
                 "full-range");
    }

    if (needsFullRange && !preserveDistributionContract) {
      decision.needsFullRange = true;
      decision.canContributeBlockSize = false;
      decision.rationale = facts->hasIndirectAccess
                               ? "indirect access pattern"
                               : "partition offset not in access pattern";

      recordDecision("H1.7-AcquireFullRange", true,
                     "acquire needs full range: " + decision.rationale,
                     facts->acquire, {});

      ARTS_DEBUG("H1.7: acquire needs full-range (" << decision.rationale
                                                    << ")");
    }

    decisions.push_back(decision);
  }

  return decisions;
}

///===----------------------------------------------------------------------===///
/// JSON Export for ArtsMate
///===----------------------------------------------------------------------===///

void HeuristicsConfig::exportDecisionsToJson(llvm::json::OStream &J) const {
  J.attributeArray("heuristic_decisions", [&]() {
    for (const auto &d : decisions) {
      J.object([&]() {
        J.attribute("heuristic", d.heuristic);
        J.attribute("applied", d.applied);
        J.attribute("rationale", d.rationale);
        J.attribute("target_id", d.affectedArtsId);

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
