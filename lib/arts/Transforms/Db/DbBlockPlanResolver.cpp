///==========================================================================///
/// File: DbBlockPlanResolver.cpp
///
/// Block-plan resolution implementation for DbPartitioning.
///==========================================================================///

#include "arts/Transforms/Db/DbBlockPlanResolver.h"
#include "arts/Utils/ArtsUtils.h"
#include "arts/Utils/DbUtils.h"
#include "arts/Utils/ValueUtils.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/IR/Dominance.h"
#include <limits>

using namespace mlir;
using namespace mlir::arts;

namespace {

static bool isBlockLikeMode(PartitionMode mode) {
  return mode == PartitionMode::block || mode == PartitionMode::stencil;
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

    Value traced =
        ValueUtils::traceValueToDominating(bs, allocOp, builder, domInfo, loc);
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

static SmallVector<Value> collectCanonicalBlockSizeCandidates(
    DbAllocOp allocOp, ArrayRef<AcquirePartitionInfo> acquireInfos,
    OpBuilder &builder, DominanceInfo &domInfo, Location loc) {
  SmallVector<Value> candidates;

  for (const auto &info : acquireInfos) {
    if (!isBlockLikeMode(info.mode) || info.partitionSizes.empty())
      continue;
    if (info.needsFullRange)
      continue;

    DbAcquireOp acquire = info.acquire;
    if (!acquire)
      continue;

    unsigned offsetIdx = 0;
    Value blockIndex =
        DbUtils::pickRepresentativePartitionOffset(info.partitionOffsets, &offsetIdx);
    Value blockSizeVal =
        DbUtils::pickRepresentativePartitionSize(info.partitionSizes, offsetIdx);

    if (!blockIndex) {
      unsigned opIdx = 0;
      if (!acquire.getPartitionIndices().empty()) {
        SmallVector<Value> partIndices(acquire.getPartitionIndices().begin(),
                                       acquire.getPartitionIndices().end());
        blockIndex = DbUtils::pickRepresentativePartitionOffset(partIndices, &opIdx);
      } else if (!acquire.getPartitionOffsets().empty()) {
        SmallVector<Value> partOffsets(acquire.getPartitionOffsets().begin(),
                                       acquire.getPartitionOffsets().end());
        blockIndex = DbUtils::pickRepresentativePartitionOffset(partOffsets, &opIdx);
      }

      if (!acquire.getPartitionSizes().empty()) {
        SmallVector<Value> partSizes(acquire.getPartitionSizes().begin(),
                                     acquire.getPartitionSizes().end());
        blockSizeVal = DbUtils::pickRepresentativePartitionSize(partSizes, opIdx);
      }
    }

    Value baseCandidate =
        DbUtils::extractBaseBlockSizeCandidate(blockIndex, blockSizeVal);
    if (!baseCandidate)
      baseCandidate = blockSizeVal;
    if (!baseCandidate)
      continue;

    Value idxCandidate =
        ValueUtils::ensureIndexType(baseCandidate, builder, loc);
    if (!idxCandidate)
      continue;

    Value domCandidate = idxCandidate;
    if (!domInfo.properlyDominates(domCandidate, allocOp)) {
      domCandidate = ValueUtils::traceValueToDominating(domCandidate, allocOp,
                                                        builder, domInfo, loc);
    }

    if (!domCandidate)
      continue;

    if (ValueUtils::isZeroConstant(ValueUtils::stripNumericCasts(domCandidate)))
      continue;

    candidates.push_back(domCandidate);
  }

  return candidates;
}

static SmallVector<unsigned>
choosePartitionedDims(DbAllocOp allocOp, ArrayRef<AcquirePartitionInfo> infos,
                      unsigned nPartDims) {
  SmallVector<unsigned> chosen;
  if (nPartDims == 0)
    return chosen;

  auto dimsEqual = [](ArrayRef<unsigned> a, ArrayRef<unsigned> b) -> bool {
    if (a.size() != b.size())
      return false;
    for (unsigned i = 0; i < a.size(); ++i) {
      if (a[i] != b[i])
        return false;
    }
    return true;
  };

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
      if (!dimsEqual(info.partitionDims, candidate.partitionDims)) {
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
          blockSizeForPlan = ValueUtils::traceValueToDominating(
              dynamicCandidate, input.allocOp, builder, domInfo, loc);
        }
      }
    }

    if (!blockSizeForPlan) {
      blockSizeForPlan = computeDefaultBlockSize(input.allocOp, builder, loc,
                                                 input.useNodesForFallback);
    }

    if (!blockSizeForPlan)
      return failure();

    result.blockSizes.push_back(blockSizeForPlan);
  }

  Value one = arts::createOneIndex(builder, loc);
  for (unsigned i = 0; i < result.blockSizes.size(); ++i) {
    Value stripped = ValueUtils::stripNumericCasts(result.blockSizes[i]);
    if (ValueUtils::isZeroConstant(stripped))
      result.blockSizes[i] = one;
    result.blockSizes[i] =
        builder.create<arith::MaxUIOp>(loc, result.blockSizes[i], one);
  }

  result.partitionedDims =
      choosePartitionedDims(input.allocOp, input.acquireInfos,
                            static_cast<unsigned>(result.blockSizes.size()));

  return result;
}
