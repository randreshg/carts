///==========================================================================///
/// File: DbHeuristics.cpp
///
/// DB heuristic policy: decision recording, JSON export, and delegation to
/// partitioning heuristics.
///==========================================================================///

#include "arts/analysis/heuristics/DbHeuristics.h"
#include "arts/Dialect.h"
#include "arts/analysis/db/DbAnalysis.h"
#include "arts/analysis/graphs/db/DbNode.h"
#include "arts/analysis/value/ValueAnalysis.h"
#include "arts/utils/DbUtils.h"
#include "arts/utils/Debug.h"
#include "arts/utils/metadata/IdRegistry.h"
#include "arts/utils/metadata/LocationMetadata.h"
#include "arts/utils/metadata/LoopMetadata.h"
#include "mlir/IR/BuiltinAttributes.h"

ARTS_DEBUG_SETUP(db_heuristics)

using namespace mlir;
using namespace mlir::arts;

namespace {

static std::string extractHeuristicId(llvm::StringRef rationale) {
  if (rationale.starts_with("H1.")) {
    size_t colonPos = rationale.find(':');
    if (colonPos != std::string::npos)
      return rationale.substr(0, colonPos).str();
  }
  return "Fallback";
}

} // namespace

DbHeuristics::DbHeuristics(const AbstractMachine &machine, IdRegistry &registry,
                           PartitionFallback fallback)
    : machine(machine), idRegistry(registry), partitionFallback(fallback) {}

bool DbHeuristics::isSingleNode() const { return machine.isSingleNode(); }

bool DbHeuristics::isValid() const { return machine.isValid(); }

void DbHeuristics::recordDecision(llvm::StringRef heuristic, bool applied,
                                  llvm::StringRef rationale, Operation *op,
                                  const llvm::StringMap<int64_t> &inputs) {
  int64_t artsId = op ? idRegistry.getOrCreate(op) : 0;
  int64_t allocId = 0;
  SmallVector<int64_t> affectedDbIds;
  std::string sourceLocation;

  if (op) {
    LocationMetadata locMeta = LocationMetadata::fromLocation(op->getLoc());
    if (locMeta.isValid())
      sourceLocation = locMeta.file + ":" + std::to_string(locMeta.line);

    if (auto acquireOp = dyn_cast<DbAcquireOp>(op)) {
      if (Operation *allocOp =
              DbUtils::getUnderlyingDbAlloc(acquireOp.getSourcePtr())) {
        allocId = idRegistry.getOrCreate(allocOp);
        ValueRange offsets = acquireOp.getOffsets();
        ValueRange sizes = acquireOp.getSizes();

        if (!offsets.empty() && !sizes.empty()) {
          int64_t offset = 0;
          int64_t count = 0;
          bool offsetKnown =
              ValueAnalysis::getConstantIndex(offsets[0], offset);
          bool sizeKnown = ValueAnalysis::getConstantIndex(sizes[0], count);
          if (offsetKnown && sizeKnown && allocId != 0) {
            if (count > kMaxOuterDBs) {
              // Cap: store only the range start for very large DB counts.
              affectedDbIds.push_back(allocId + offset);
            } else {
              for (int64_t i = 0; i < count; ++i)
                affectedDbIds.push_back(allocId + offset + i);
            }
          } else if (allocId != 0) {
            affectedDbIds.push_back(allocId);
          }
        } else if (allocId != 0) {
          if (auto dbAlloc = dyn_cast<DbAllocOp>(allocOp)) {
            ValueRange allocSizes = dbAlloc.getSizes();
            if (!allocSizes.empty()) {
              int64_t totalDbs = 1;
              bool allStatic = true;
              for (Value sz : allocSizes) {
                int64_t dim = 0;
                if (ValueAnalysis::getConstantIndex(sz, dim))
                  totalDbs *= dim;
                else
                  allStatic = false;
              }
              if (allStatic) {
                if (totalDbs > kMaxOuterDBs) {
                  // Cap: store only the first element for very large DB counts.
                  affectedDbIds.push_back(allocId);
                } else {
                  for (int64_t i = 0; i < totalDbs; ++i)
                    affectedDbIds.push_back(allocId + i);
                }
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
}

llvm::ArrayRef<HeuristicDecision> DbHeuristics::getDecisions() const {
  return decisions;
}

PartitioningDecision
DbHeuristics::choosePartitioning(const PartitioningContext &ctx) {
  if (ctx.preferBlockND && ctx.preferredOuterRank > 0) {
    std::string rationale =
        "H1.B0: owner-compute tiling keeps N-D block partitioning";
    recordDecision("H1.B0", true, rationale, nullptr, {});
    return PartitioningDecision::blockND(ctx, ctx.preferredOuterRank,
                                         rationale);
  }

  auto decision =
      evaluatePartitioningHeuristics(ctx, &machine, partitionFallback);
  std::string heuristicId = extractHeuristicId(decision.rationale);
  recordDecision(heuristicId, true, decision.rationale, nullptr, {});
  return decision;
}

SmallVector<AcquireDecision> DbHeuristics::chooseAcquirePolicies(
    ArrayRef<const DbAcquirePartitionFacts *> acquireFacts,
    ArrayRef<const DbAnalysis::AcquireContractSummary *> summaries) {
  SmallVector<AcquireDecision> result;
  result.reserve(acquireFacts.size());

  for (size_t i = 0; i < acquireFacts.size(); ++i) {
    const DbAcquirePartitionFacts *facts = acquireFacts[i];
    const DbAnalysis::AcquireContractSummary *summary =
        (i < summaries.size()) ? summaries[i] : nullptr;

    AcquireDecision decision;
    if (!facts) {
      result.push_back(decision);
      continue;
    }

    bool preserveDistributionContract =
        !facts->partitionDims.empty() &&
        llvm::any_of(facts->entries, [](const DbPartitionEntryFact &entry) {
          return entry.preservesDistributedContract;
        });

    /// When a contract summary preserves a distributed contract entry,
    /// treat it as evidence to keep the distribution layout intact.
    if (!preserveDistributionContract && summary &&
        summary->preservesDistributedContractEntry()) {
      preserveDistributionContract = true;
      ARTS_DEBUG("  chooseAcquirePolicies["
                 << i << "]: contract summary preserves distributed contract");
    }

    bool needsFullRange =
        llvm::any_of(facts->entries, [](const DbPartitionEntryFact &entry) {
          return entry.needsFullRange;
        });

    /// Use contract summary to refine the full-range decision.
    /// If the summary has ownerDims information, the acquire has a known
    /// owner-compute relationship that can restrict access to owned blocks,
    /// so full-range may not actually be needed.
    if (needsFullRange && summary && !summary->contract.ownerDims.empty() &&
        !facts->hasIndirectAccess) {
      ARTS_DEBUG("  chooseAcquirePolicies["
                 << i
                 << "]: contract ownerDims override full-range "
                    "(owner-compute locality known)");
      needsFullRange = false;
    }

    /// If the summary indicates stencil semantics with block hints,
    /// the acquire can use halo-aware block access instead of full-range.
    if (needsFullRange && summary &&
        summary->contract.hasExplicitStencilContract() &&
        summary->hasBlockHints() && !facts->hasIndirectAccess) {
      ARTS_DEBUG("  chooseAcquirePolicies["
                 << i
                 << "]: stencil contract with block hints overrides "
                    "full-range");
      needsFullRange = false;
    }

    /// When the contract summary has a refined depPattern, prefer it over
    /// the raw facts' pattern for rationale reporting.
    std::optional<ArtsDepPattern> effectiveDepPattern;
    if (summary && summary->contract.depPattern)
      effectiveDepPattern = summary->contract.depPattern;
    else
      effectiveDepPattern = facts->depPattern;

    /// If the summary indicates distribution contract information, factor
    /// it into the block-size contribution decision. Acquires with a known
    /// distribution pattern are better candidates for block-size contribution.
    if (summary && summary->hasDistributionContract() &&
        !decision.canContributeBlockSize) {
      decision.canContributeBlockSize = true;
      ARTS_DEBUG("  chooseAcquirePolicies["
                 << i
                 << "]: distribution contract enables block-size contribution");
    }

    if (needsFullRange && !preserveDistributionContract) {
      decision.needsFullRange = true;
      decision.canContributeBlockSize = false;

      if (facts->hasIndirectAccess) {
        decision.rationale = "indirect access pattern";
      } else if (effectiveDepPattern &&
                 *effectiveDepPattern != ArtsDepPattern::unknown) {
        decision.rationale =
            "partition offset not in access pattern (depPattern=" +
            std::string(stringifyArtsDepPattern(*effectiveDepPattern)) + ")";
      } else {
        decision.rationale = "partition offset not in access pattern";
      }

      recordDecision("H1.7-AcquireFullRange", true,
                     "acquire needs full range: " + decision.rationale,
                     facts->acquire, {});
    }

    result.push_back(decision);
  }

  return result;
}

void DbHeuristics::exportDecisionsToJson(llvm::json::OStream &J) const {
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
          J.attributeObject("cost_model_inputs", [&]() {
            for (const auto &kv : d.costModelInputs)
              J.attribute(kv.first(), kv.second);
          });
        }
      });
    }
  });
}
