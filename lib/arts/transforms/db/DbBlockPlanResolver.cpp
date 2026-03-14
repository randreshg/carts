///==========================================================================///
/// File: DbBlockPlanResolver.cpp
///
/// Block-plan resolution implementation for DbPartitioning.
///
/// Before:
///   %db = arts.db_alloc ... {partitioning = #arts.partitioning<block>}
///   %a  = arts.db_acquire %db ... {partition_sizes = []}
///
/// After:
///   %db = arts.db_alloc ... {partitioning = #arts.partitioning<block>,
///                            element_sizes = [%bx, %by]}
///   %a  = arts.db_acquire %db ... {partition_sizes = [%bx, %by]}
///==========================================================================///

#include "arts/transforms/db/DbBlockPlanResolver.h"
#include "arts/analysis/value/ValueAnalysis.h"
#include "arts/transforms/db/DbRewriter.h"
#include "arts/utils/DbUtils.h"
#include "arts/utils/Debug.h"
#include "arts/utils/OperationAttributes.h"
#include "arts/utils/StencilAttributes.h"
#include "arts/utils/Utils.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/IR/Dominance.h"
#include <limits>

using namespace mlir;
using namespace mlir::arts;

#define DEBUG_TYPE "db_block_plan"
ARTS_DEBUG_SETUP(db_block_plan);

namespace {

static bool shouldOverdecompose2DStencilFallback(
    DbAllocOp allocOp, ArrayRef<AcquirePartitionInfo> acquireInfos,
    ArrayRef<Value> resolvedBlockSizes) {
  if (!allocOp || allocOp.getElementSizes().size() != 2)
    return false;
  if (!resolvedBlockSizes.empty())
    return false;

  return llvm::any_of(acquireInfos, [](const AcquirePartitionInfo &info) {
    return requiresBlockSize(info.mode) && !info.needsFullRange &&
           info.accessPattern == AccessPattern::Stencil;
  });
}

static Value overdecompose2DStencilBlockSize(Value blockSize,
                                             OpBuilder &builder, Location loc) {
  if (!blockSize)
    return blockSize;
  Value one = arts::createOneIndex(builder, loc);
  Value adjusted = builder.create<arith::AddIOp>(loc, blockSize, one);
  Value halved = builder.create<arith::DivUIOp>(
      loc, adjusted, arts::createConstantIndex(builder, loc, 2));
  return builder.create<arith::MaxUIOp>(loc, halved, one);
}

static Value computeDefaultBlockSize(DbAllocOp allocOp, OpBuilder &builder,
                                     Location loc, bool useNodes) {
  if (allocOp.getElementSizes().empty())
    return nullptr;

  Value elemSize = allocOp.getElementSizes().front();
  if (!elemSize)
    return nullptr;

  Value one = arts::createOneIndex(builder, loc);
  Value parallelI32 =
      useNodes
          ? builder.create<RuntimeQueryOp>(loc, RuntimeQueryKind::totalNodes)
                .getResult()
          : builder.create<RuntimeQueryOp>(loc, RuntimeQueryKind::totalWorkers)
                .getResult();
  Value workers = builder.create<arith::IndexCastUIOp>(
      loc, builder.getIndexType(), parallelI32);
  Value workersClamped = builder.create<arith::MaxUIOp>(loc, workers, one);

  Value workersMinusOne =
      builder.create<arith::SubIOp>(loc, workersClamped, one);
  Value adjusted =
      builder.create<arith::AddIOp>(loc, elemSize, workersMinusOne);
  Value blockSize =
      builder.create<arith::DivUIOp>(loc, adjusted, workersClamped);
  return builder.create<arith::MaxUIOp>(loc, blockSize, one);
}

static SmallVector<Value>
resolveNDBlockHints(DbAllocOp allocOp, ArrayRef<Value> ndHints,
                    OpBuilder &builder, DominanceInfo &domInfo, Location loc) {
  SmallVector<Value> result;
  for (Value bs : ndHints) {
    if (!bs)
      continue;

    if (domInfo.properlyDominates(bs, allocOp)) {
      result.push_back(bs);
      continue;
    }

    Value traced = ValueAnalysis::traceValueToDominating(bs, allocOp, builder,
                                                         domInfo, loc);
    if (traced) {
      result.push_back(traced);
      continue;
    }

    if (auto constOp = bs.getDefiningOp<arith::ConstantIndexOp>()) {
      result.push_back(
          arts::createConstantIndex(builder, loc, constOp.value()));
      continue;
    }

    result.clear();
    break;
  }

  return result;
}

/// Normalize a block size candidate through the standard pipeline:
/// extract base -> ensure index type -> trace to dominating -> filter zero.
static Value normalizeBlockSizeCandidate(Value offset, Value size,
                                         OpBuilder &builder,
                                         DominanceInfo &domInfo,
                                         DbAllocOp allocOp, Location loc) {
  Value candidate = DbUtils::extractBaseBlockSizeCandidate(offset, size);
  if (!candidate)
    candidate = size;
  if (!candidate)
    return Value();

  candidate = ValueAnalysis::ensureIndexType(candidate, builder, loc);
  if (!candidate)
    return Value();

  if (!domInfo.properlyDominates(candidate, allocOp)) {
    candidate = ValueAnalysis::traceValueToDominating(candidate, allocOp,
                                                      builder, domInfo, loc);
  }

  if (!candidate)
    return Value();
  if (ValueAnalysis::isZeroConstant(
          ValueAnalysis::stripNumericCasts(candidate)))
    return Value();
  return candidate;
}

static SmallVector<Value> collectCanonicalBlockSizeCandidates(
    DbAllocOp allocOp, ArrayRef<AcquirePartitionInfo> acquireInfos,
    OpBuilder &builder, DominanceInfo &domInfo, Location loc) {
  SmallVector<Value> candidates;

  for (const auto &info : acquireInfos) {
    if (!requiresBlockSize(info.mode) || info.partitionSizes.empty())
      continue;
    if (info.needsFullRange)
      continue;

    DbAcquireOp acquire = info.acquire;
    if (!acquire)
      continue;

    unsigned offsetIdx = 0;
    Value blockIndex = DbUtils::pickRepresentativePartitionOffset(
        info.partitionOffsets, &offsetIdx);
    Value blockSizeVal = DbUtils::pickRepresentativePartitionSize(
        info.partitionSizes, offsetIdx);

    if (!blockIndex) {
      unsigned opIdx = 0;
      if (!acquire.getPartitionIndices().empty()) {
        SmallVector<Value> partIndices(acquire.getPartitionIndices().begin(),
                                       acquire.getPartitionIndices().end());
        blockIndex =
            DbUtils::pickRepresentativePartitionOffset(partIndices, &opIdx);
      } else if (!acquire.getPartitionOffsets().empty()) {
        SmallVector<Value> partOffsets(acquire.getPartitionOffsets().begin(),
                                       acquire.getPartitionOffsets().end());
        blockIndex =
            DbUtils::pickRepresentativePartitionOffset(partOffsets, &opIdx);
      }

      if (!acquire.getPartitionSizes().empty()) {
        SmallVector<Value> partSizes(acquire.getPartitionSizes().begin(),
                                     acquire.getPartitionSizes().end());
        blockSizeVal =
            DbUtils::pickRepresentativePartitionSize(partSizes, opIdx);
      }
    }

    Value domCandidate = normalizeBlockSizeCandidate(
        blockIndex, blockSizeVal, builder, domInfo, allocOp, loc);
    if (!domCandidate)
      continue;

    candidates.push_back(domCandidate);
  }

  return candidates;
}

static SmallVector<Value> collectCanonicalNDBlockSizeCandidates(
    DbAllocOp allocOp, ArrayRef<AcquirePartitionInfo> acquireInfos,
    OpBuilder &builder, DominanceInfo &domInfo, Location loc) {
  struct RankedCandidate {
    SmallVector<Value> blockSizes;
    SmallVector<unsigned> partitionDims;
    bool hasContract = false;
  };

  SmallVector<RankedCandidate> rankedCandidates;
  unsigned bestRank = 0;
  bool bestHasContract = false;

  for (const auto &info : acquireInfos) {
    if (!requiresBlockSize(info.mode) || info.needsFullRange)
      continue;

    unsigned rank = std::min<unsigned>(info.partitionOffsets.size(),
                                       info.partitionSizes.size());
    if (!info.partitionDims.empty())
      rank = std::min<unsigned>(rank, info.partitionDims.size());
    if (rank < 2)
      continue;

    SmallVector<Value> blockSizes;
    blockSizes.reserve(rank);
    bool failed = false;
    for (unsigned dim = 0; dim < rank; ++dim) {
      Value candidate = normalizeBlockSizeCandidate(
          info.partitionOffsets[dim], info.partitionSizes[dim], builder,
          domInfo, allocOp, loc);
      if (!candidate) {
        failed = true;
        break;
      }
      blockSizes.push_back(candidate);
    }
    if (failed || blockSizes.size() < 2)
      continue;

    bool hasContract = info.hasDistributionContract;
    if (blockSizes.size() > bestRank ||
        (blockSizes.size() == bestRank && hasContract && !bestHasContract)) {
      bestRank = static_cast<unsigned>(blockSizes.size());
      bestHasContract = hasContract;
      rankedCandidates.clear();
    }

    if (blockSizes.size() == bestRank && hasContract == bestHasContract) {
      SmallVector<unsigned> dims(info.partitionDims.begin(),
                                 info.partitionDims.end());
      rankedCandidates.push_back(
          {std::move(blockSizes), std::move(dims), hasContract});
    }
  }

  if (rankedCandidates.empty())
    return {};

  // --- Per-dimension independent merge (EXT-PART-4) ---
  //
  // Instead of taking global MIN across all candidates for every dimension,
  // we merge each dimension independently: only candidates that actually
  // partition a given dimension contribute to that dimension's MIN.
  //
  // Example: acquire1 suggests 64x128 (partitions dim 0 only),
  //          acquire2 suggests 128x64 (partitions dim 1 only).
  //   Global MIN  -> 64x64   (too conservative)
  //   Per-dim MIN -> 64x128  (correct: each dim uses its own partitioner)

  unsigned nDims =
      static_cast<unsigned>(rankedCandidates.front().blockSizes.size());

  // Check whether per-dim analysis is feasible: all candidates must have
  // partitionDims vectors of the expected rank so we can map positional
  // block sizes to actual DB dimensions.
  bool perDimFeasible = true;
  for (const auto &rc : rankedCandidates) {
    if (rc.partitionDims.size() != nDims) {
      perDimFeasible = false;
      break;
    }
  }

  // --- Global MIN (fallback / reference) ---
  SmallVector<Value> globalMerged = rankedCandidates.front().blockSizes;
  for (size_t i = 1; i < rankedCandidates.size(); ++i) {
    for (unsigned dim = 0; dim < nDims; ++dim) {
      globalMerged[dim] = builder.create<arith::MinUIOp>(
          loc, globalMerged[dim], rankedCandidates[i].blockSizes[dim]);
    }
  }

  if (!perDimFeasible || rankedCandidates.size() <= 1) {
    // Fall back to global MIN when per-dim analysis is not possible or
    // there is only a single candidate (both strategies are identical).
    return globalMerged;
  }

  // --- Per-dimension merge ---
  // For each positional dimension slot d, determine which candidates
  // actually partition that slot's DB dimension. A candidate "partitions
  // dimension d" if its partitionDims[d] matches the consensus DB
  // dimension for slot d (i.e., the most common mapping).
  //
  // We use the first candidate's partitionDims as the reference mapping
  // (all candidates passed the same bestRank filter, so positional
  // alignment is expected).
  ArrayRef<unsigned> refDims = rankedCandidates.front().partitionDims;

  SmallVector<Value> perDimMerged;
  perDimMerged.reserve(nDims);
  bool perDimDiffers = false;

  for (unsigned d = 0; d < nDims; ++d) {
    unsigned targetDbDim = refDims[d];
    Value mergedVal;
    unsigned contributorCount = 0;

    for (const auto &rc : rankedCandidates) {
      // A candidate contributes to this positional slot only if it maps
      // the same DB dimension at position d.
      if (rc.partitionDims[d] != targetDbDim)
        continue;
      if (!mergedVal) {
        mergedVal = rc.blockSizes[d];
      } else {
        mergedVal =
            builder.create<arith::MinUIOp>(loc, mergedVal, rc.blockSizes[d]);
      }
      ++contributorCount;
    }

    if (!mergedVal) {
      // No candidate contributed -- fall back to global MIN for the
      // entire result to stay safe.
      ARTS_DEBUG("per-dim merge: no contributors for dim "
                 << d << " (DB dim " << targetDbDim
                 << "), falling back to global MIN");
      return globalMerged;
    }

    perDimMerged.push_back(mergedVal);

    // Track whether per-dim result would differ from global MIN.
    if (contributorCount < rankedCandidates.size())
      perDimDiffers = true;
  }

  // Emit debug logging when per-dim merge produces a different result.
  if (perDimDiffers) {
    ARTS_DEBUG_REGION(
        ARTS_DBGS() << "[db_block_plan] per-dim merge differs from global MIN "
                    << "(nDims=" << nDims
                    << ", candidates=" << rankedCandidates.size() << ")\n";
        for (unsigned d = 0; d < nDims; ++d) {
          // Count how many candidates contributed to this dim.
          unsigned targetDbDim = refDims[d];
          unsigned count = 0;
          for (const auto &rc : rankedCandidates) {
            if (rc.partitionDims[d] == targetDbDim)
              ++count;
          }
          ARTS_DBGS() << "  dim " << d << " (DB dim " << targetDbDim
                      << "): " << count << "/" << rankedCandidates.size()
                      << " contributors\n";
        });
  }

  return perDimMerged;
}

static SmallVector<Value> collectContractNDBlockShapeCandidates(
    DbAllocOp allocOp, ArrayRef<AcquirePartitionInfo> acquireInfos,
    OpBuilder &builder, Location loc) {
  SmallVector<Value> contractShape;
  bool hasPreferredContract = false;

  for (const auto &info : acquireInfos) {
    if (info.needsFullRange || !info.acquire)
      continue;

    DbAcquireOp acquire = info.acquire;
    auto acquireMode = getPartitionMode(acquire.getOperation());
    bool blockCapableAcquire =
        requiresBlockSize(info.mode) ||
        (acquireMode && requiresBlockSize(*acquireMode)) ||
        info.hasDistributionContract ||
        hasSupportedBlockHalo(acquire.getOperation());
    if (!blockCapableAcquire)
      continue;

    auto shapeAttr = getStencilBlockShape(acquire.getOperation());
    if (!shapeAttr || shapeAttr->size() < 2)
      continue;

    bool prefersContract = info.hasDistributionContract ||
                           hasSupportedBlockHalo(acquire.getOperation());
    if (!prefersContract && hasPreferredContract)
      continue;
    if (prefersContract && !hasPreferredContract) {
      hasPreferredContract = true;
      contractShape.clear();
    }

    if (contractShape.empty()) {
      contractShape.reserve(shapeAttr->size());
      for (int64_t dimSize : *shapeAttr) {
        if (dimSize <= 0) {
          contractShape.clear();
          break;
        }
        contractShape.push_back(createConstantIndex(builder, loc, dimSize));
      }
      continue;
    }

    if (contractShape.size() != shapeAttr->size())
      continue;

    for (unsigned dim = 0; dim < contractShape.size(); ++dim) {
      Value dimConst = createConstantIndex(builder, loc, (*shapeAttr)[dim]);
      contractShape[dim] =
          builder.create<arith::MinUIOp>(loc, contractShape[dim], dimConst);
    }
  }

  if (!contractShape.empty() &&
      allocOp.getElementSizes().size() >= contractShape.size())
    return contractShape;
  return {};
}

static SmallVector<unsigned>
choosePartitionedDims(DbAllocOp allocOp, ArrayRef<AcquirePartitionInfo> infos,
                      unsigned nPartDims) {
  SmallVector<unsigned> chosen;
  if (nPartDims == 0)
    return chosen;

  int bestScore = std::numeric_limits<int>::max();
  bool bestFromFine = false;
  SmallVector<const AcquirePartitionInfo *, 8> preferredInfos;
  for (const auto &info : infos) {
    if (!info.hasDistributionContract)
      continue;
    if (info.partitionDims.size() != nPartDims)
      continue;
    preferredInfos.push_back(&info);
  }

  auto forEachVotingInfo =
      [&](llvm::function_ref<void(const AcquirePartitionInfo &)> fn) {
        if (!preferredInfos.empty()) {
          for (const AcquirePartitionInfo *info : preferredInfos)
            fn(*info);
          return;
        }
        for (const auto &info : infos)
          fn(info);
      };

  forEachVotingInfo([&](const AcquirePartitionInfo &candidate) {
    if (preferredInfos.empty() && candidate.needsFullRange)
      return;
    if (candidate.partitionDims.size() != nPartDims)
      return;

    int score = 0;
    forEachVotingInfo([&](const AcquirePartitionInfo &info) {
      if (preferredInfos.empty() && info.needsFullRange)
        return;
      if (info.partitionDims.empty() || info.partitionDims.size() != nPartDims)
        return;
      if (info.partitionDims != candidate.partitionDims) {
        score += (info.mode == PartitionMode::fine_grained) ? 2 : 1;
      }
    });

    bool fromFine = candidate.mode == PartitionMode::fine_grained;
    if (score < bestScore ||
        (score == bestScore && fromFine && !bestFromFine)) {
      bestScore = score;
      bestFromFine = fromFine;
      chosen.assign(candidate.partitionDims.begin(),
                    candidate.partitionDims.end());
    }
  });

  if (!chosen.empty()) {
    SmallVector<bool> seen(allocOp.getElementSizes().size(), false);
    bool valid = true;
    for (unsigned d : chosen) {
      if (d >= seen.size() || seen[d]) {
        valid = false;
        break;
      }
      seen[d] = true;
    }
    if (!valid)
      chosen.clear();
  }

  return chosen;
}

} // namespace

FailureOr<DbBlockPlanResult>
mlir::arts::resolveDbBlockPlan(const DbBlockPlanInput &input) {
  if (!input.allocOp || !input.builder)
    return failure();

  OpBuilder &builder = *input.builder;
  Location loc = input.loc;
  func::FuncOp func = input.allocOp->getParentOfType<func::FuncOp>();
  if (!func)
    return failure();

  DominanceInfo domInfo(func);

  DbBlockPlanResult result;

  if (!input.ndBlockSizeHints.empty()) {
    result.blockSizes = resolveNDBlockHints(
        input.allocOp, input.ndBlockSizeHints, builder, domInfo, loc);
  }

  SmallVector<Value> contractShape = collectContractNDBlockShapeCandidates(
      input.allocOp, input.acquireInfos, builder, loc);
  if (result.blockSizes.empty() ||
      contractShape.size() > result.blockSizes.size()) {
    result.blockSizes = std::move(contractShape);
  }

  if (result.blockSizes.empty()) {
    result.blockSizes = collectCanonicalNDBlockSizeCandidates(
        input.allocOp, input.acquireInfos, builder, domInfo, loc);
  }

  if (result.blockSizes.empty()) {
    Value blockSizeForPlan;

    if (input.staticBlockSizeHint && *input.staticBlockSizeHint > 0) {
      blockSizeForPlan =
          arts::createConstantIndex(builder, loc, *input.staticBlockSizeHint);
    }

    if (!blockSizeForPlan) {
      Value dynamicCandidate = input.dynamicBlockSizeHint;

      if (!dynamicCandidate) {
        SmallVector<Value> canonicalCandidates =
            collectCanonicalBlockSizeCandidates(
                input.allocOp, input.acquireInfos, builder, domInfo, loc);
        if (!canonicalCandidates.empty()) {
          dynamicCandidate = canonicalCandidates.front();
          for (size_t i = 1; i < canonicalCandidates.size(); ++i) {
            dynamicCandidate = builder.create<arith::MinUIOp>(
                loc, dynamicCandidate, canonicalCandidates[i]);
          }
        }
      }

      if (dynamicCandidate) {
        if (domInfo.properlyDominates(dynamicCandidate, input.allocOp)) {
          blockSizeForPlan = dynamicCandidate;
        } else {
          blockSizeForPlan = ValueAnalysis::traceValueToDominating(
              dynamicCandidate, input.allocOp, builder, domInfo, loc);
        }
      }
    }

    if (!blockSizeForPlan) {
      blockSizeForPlan = computeDefaultBlockSize(input.allocOp, builder, loc,
                                                 input.useNodesForFallback);
      if (shouldOverdecompose2DStencilFallback(
              input.allocOp, input.acquireInfos, result.blockSizes)) {
        blockSizeForPlan =
            overdecompose2DStencilBlockSize(blockSizeForPlan, builder, loc);
      }
    }

    if (!blockSizeForPlan)
      return failure();

    result.blockSizes.push_back(blockSizeForPlan);
  }

  Value one = arts::createOneIndex(builder, loc);
  for (unsigned i = 0; i < result.blockSizes.size(); ++i) {
    Value stripped = ValueAnalysis::stripNumericCasts(result.blockSizes[i]);
    if (ValueAnalysis::isZeroConstant(stripped))
      result.blockSizes[i] = one;
    result.blockSizes[i] =
        builder.create<arith::MaxUIOp>(loc, result.blockSizes[i], one);
  }

  result.partitionedDims =
      choosePartitionedDims(input.allocOp, input.acquireInfos,
                            static_cast<unsigned>(result.blockSizes.size()));

  return result;
}
