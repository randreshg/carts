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
#include "arts/utils/LoweringContractUtils.h"
#include "arts/utils/OperationAttributes.h"
#include "arts/utils/Utils.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/IR/Dominance.h"
#include <limits>

using namespace mlir;
using namespace mlir::arts;

#define DEBUG_TYPE "db_block_plan"
ARTS_DEBUG_SETUP(db_block_plan_resolver);

namespace {

static SmallVector<unsigned>
choosePartitionedDims(DbAllocOp allocOp, ArrayRef<AcquirePartitionInfo> infos,
                      unsigned nPartDims);

static Value
computeDefaultBlockSize(DbAllocOp allocOp,
                        ArrayRef<AcquirePartitionInfo> acquireInfos,
                        OpBuilder &builder, Location loc, bool useNodes,
                        bool clampStencilFallbackWorkers) {
  if (allocOp.getElementSizes().empty())
    return nullptr;

  unsigned preferredDim = 0;
  if (SmallVector<unsigned> partitionedDims =
          choosePartitionedDims(allocOp, acquireInfos, /*nPartDims=*/1);
      !partitionedDims.empty() &&
      partitionedDims.front() < allocOp.getElementSizes().size()) {
    preferredDim = partitionedDims.front();
  }

  Value elemSize = allocOp.getElementSizes()[preferredDim];
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

  /// Keep fallback stencil block sizing aligned with the same active-worker
  /// space that ForOpt uses for intranode stencil coarsening. Without this
  /// cap, >32-thread single-node builds can keep 8-row task chunks while
  /// shrinking DB blocks to 3 rows, which widens dependency windows and breaks
  /// Jacobi-style halo schedules.
  if (clampStencilFallbackWorkers && !useNodes) {
    Value minOwnedOuterIters = arts::createConstantIndex(builder, loc, 8);
    Value maxWorkersByOwnedSpan =
        builder.create<arith::DivUIOp>(loc, elemSize, minOwnedOuterIters);
    maxWorkersByOwnedSpan =
        builder.create<arith::MaxUIOp>(loc, maxWorkersByOwnedSpan, one);
    workersClamped = builder.create<arith::MinUIOp>(loc, workersClamped,
                                                    maxWorkersByOwnedSpan);
  }

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

  /// Per-dimension independent merge: only candidates that partition a given
  /// dimension contribute to that dimension's MIN (not global MIN).
  /// Example: acquire1 64x128 (dim 0), acquire2 128x64 (dim 1) -> per-dim MIN
  /// 64x128; global MIN would be 64x64.
  unsigned nDims =
      static_cast<unsigned>(rankedCandidates.front().blockSizes.size());

  /// All candidates must have partitionDims of expected rank to map block
  /// sizes to DB dimensions.
  bool perDimFeasible = true;
  for (const auto &rc : rankedCandidates) {
    if (rc.partitionDims.size() != nDims) {
      perDimFeasible = false;
      break;
    }
  }

  /// Global MIN (fallback when per-dim merge is not applicable).
  SmallVector<Value> globalMerged = rankedCandidates.front().blockSizes;
  for (size_t i = 1; i < rankedCandidates.size(); ++i) {
    for (unsigned dim = 0; dim < nDims; ++dim) {
      globalMerged[dim] = builder.create<arith::MinUIOp>(
          loc, globalMerged[dim], rankedCandidates[i].blockSizes[dim]);
    }
  }

  if (!perDimFeasible || rankedCandidates.size() <= 1) {
    /// Fall back to global MIN when per-dim analysis is not possible or
    /// there is only a single candidate.
    return globalMerged;
  }

  /// Per-dimension merge: for each slot d, candidates that partition that
  /// DB dimension contribute; first candidate's partitionDims is the reference.
  ArrayRef<unsigned> refDims = rankedCandidates.front().partitionDims;

  SmallVector<Value> perDimMerged;
  perDimMerged.reserve(nDims);
  bool perDimDiffers = false;

  for (unsigned d = 0; d < nDims; ++d) {
    unsigned targetDbDim = refDims[d];
    Value mergedVal;
    unsigned contributorCount = 0;

    for (const auto &rc : rankedCandidates) {
      /// Candidate contributes only if it maps the same DB dimension at slot d.
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
      /// No candidate contributed; fall back to global MIN for safety.
      ARTS_DEBUG("per-dim merge: no contributors for dim "
                 << d << " (DB dim " << targetDbDim
                 << "), falling back to global MIN");
      return globalMerged;
    }

    perDimMerged.push_back(mergedVal);

    /// Track whether per-dim result would differ from global MIN.
    if (contributorCount < rankedCandidates.size())
      perDimDiffers = true;
  }

  /// Emit debug logging when per-dim merge produces a different result.
  if (perDimDiffers) {
    ARTS_DEBUG_REGION(
        ARTS_DBGS() << "[db_block_plan] per-dim merge differs from global MIN "
                    << "(nDims=" << nDims
                    << ", candidates=" << rankedCandidates.size() << ")\n";
        for (unsigned d = 0; d < nDims; ++d) {
          /// Count how many candidates contributed to this dim.
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
    std::optional<LoweringContractInfo> contract =
        getLoweringContract(acquire.getPtr());
    if (!contract)
      contract = getSemanticContract(acquire.getOperation());

    auto acquireMode = getPartitionMode(acquire.getOperation());
    bool hasBlockHaloContract = contract && contract->supportsBlockHalo();
    bool blockCapableAcquire =
        requiresBlockSize(info.mode) ||
        (acquireMode && requiresBlockSize(*acquireMode)) ||
        info.hasDistributionContract || hasBlockHaloContract;
    if (!blockCapableAcquire)
      continue;

    auto staticShape =
        contract ? contract->getStaticBlockShape() : std::nullopt;
    if (!staticShape || staticShape->size() < 2)
      continue;

    /// `stencil_block_shape` describes the full logical task tile, not
    /// necessarily the number of DB dimensions that this acquire proved safe to
    /// partition. For row-owned Jacobi/Poisson-style contracts we routinely
    /// carry a 2-D block shape like [rows, cols] while only one owner
    /// dimension is actually distributed. Promoting that full shape directly to
    /// an N-D DB plan manufactures extra outer DB dimensions and later
    /// mismatches task acquires that correctly operate on a 1-D owner space.
    unsigned supportedRank = 0;
    if (!info.partitionDims.empty())
      supportedRank = static_cast<unsigned>(info.partitionDims.size());
    else
      supportedRank = std::min<unsigned>(
          static_cast<unsigned>(info.partitionOffsets.size()),
          static_cast<unsigned>(info.partitionSizes.size()));
    supportedRank = std::min<unsigned>(
        supportedRank, static_cast<unsigned>(staticShape->size()));
    if (supportedRank < 2)
      continue;

    bool prefersContract = info.hasDistributionContract || hasBlockHaloContract;
    if (!prefersContract && hasPreferredContract)
      continue;
    if (prefersContract && !hasPreferredContract) {
      hasPreferredContract = true;
      contractShape.clear();
    }

    if (contractShape.empty()) {
      contractShape.reserve(supportedRank);
      for (unsigned dim = 0; dim < supportedRank; ++dim) {
        int64_t dimSize = (*staticShape)[dim];
        if (dimSize <= 0) {
          contractShape.clear();
          break;
        }
        contractShape.push_back(createConstantIndex(builder, loc, dimSize));
      }
      continue;
    }

    if (contractShape.size() != supportedRank)
      continue;

    for (unsigned dim = 0; dim < supportedRank; ++dim) {
      Value dimConst = createConstantIndex(builder, loc, (*staticShape)[dim]);
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
      blockSizeForPlan = computeDefaultBlockSize(
          input.allocOp, input.acquireInfos, builder, loc,
          input.useNodesForFallback, input.clampStencilFallbackWorkers);
      /// Keep fallback stencil block sizing aligned with the same worker-based
      /// owner span that ForOpt / ForLowering use to form task chunks.
      /// Shrinking only the DB block size further does not create additional
      /// task parallelism; it only makes each task touch more blocks, widening
      /// acquire spans and inflating db_ref traffic in memory-bound stencils.
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
