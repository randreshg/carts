///==========================================================================///
/// File: DbBlockIndexer.cpp
///
/// Block index localization for datablock partitioning.
///
/// Example transformation (N=100 rows, blockSize=25, 4 workers):
///
///   BEFORE (single allocation, global indices):
///     %db = arts.db_alloc memref<100xf64>
///     arts.edt {
///       %ref = arts.db_ref %db[0]
///       memref.load %ref[%j]                 /// global row j
///     }
///
///   AFTER (blocked, div/mod localization):
///     %db = arts.db_alloc memref<4x25xf64>   /// 4 blocks of 25 rows
///     arts.edt(%startBlock, %numBlocks) {
///       %block = %j / 25                     /// physical block
///       %relBlock = %block - %startBlock     /// worker-relative block
///       %localRow = %j % 25                  /// row within block
///       %ref = arts.db_ref %db[%relBlock]
///       memref.load %ref[%localRow]
///     }
///
/// Index localization formulas:
///   blockIdx = (globalRow / blockSize) - startBlock
///   localRow = globalRow % blockSize
///
/// For linearized access (single index into multi-dim memref):
///   1. De-linearize: globalRow = linear / stride, col = linear % stride
///   2. Apply div/mod: localRow = globalRow % blockSize
///   3. Re-linearize: localLinear = localRow * stride + col
///
/// Shared transformOps/transformDbRefUsers/transformUsesInParentRegion are
/// inherited from DbIndexerBase. Block-specific behavior is provided via
/// handleSubIndexOp override and the localize/localizeLinearized methods.
///==========================================================================///

#include "arts/transforms/db/block/DbBlockIndexer.h"
#include "arts/analysis/value/ValueAnalysis.h"
#include "arts/codegen/Codegen.h"
#include "arts/utils/BlockedAccessUtils.h"
#include "arts/utils/DbUtils.h"
#include "arts/utils/Debug.h"
#include "arts/utils/LoopUtils.h"
#include "arts/utils/Utils.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "polygeist/Ops.h"

ARTS_DEBUG_SETUP(db_transforms);

using namespace mlir;
using namespace mlir::arts;

struct LoopWindowLocalization {
  Value dbRefIdx;
  Value localIdx;
};

static std::optional<LoopWindowLocalization>
tryLocalizeFromLoopWindow(Value globalIdx, Value startBlock, Value blockSize,
                          OpBuilder &builder, Location loc) {
  if (!globalIdx || !blockSize)
    return std::nullopt;

  int64_t constOffset = 0;
  Value indexedExpr =
      ValueAnalysis::stripConstantOffset(globalIdx, &constOffset);
  indexedExpr = ValueAnalysis::stripNumericCasts(indexedExpr);

  auto resolveLoopCarrier =
      [&](Value candidateIv) -> std::optional<std::pair<scf::ForOp, Value>> {
    candidateIv = ValueAnalysis::stripNumericCasts(candidateIv);
    auto loopIv = dyn_cast<BlockArgument>(candidateIv);
    auto loop =
        loopIv ? dyn_cast_or_null<scf::ForOp>(loopIv.getOwner()->getParentOp())
               : scf::ForOp();
    if (!loop || loop.getInductionVar() != loopIv)
      return std::nullopt;

    Value invariantBase;
    if (!arts::matchLoopInvariantAddend(indexedExpr, loopIv, invariantBase))
      return std::nullopt;

    return std::make_pair(loop, invariantBase);
  };

  std::optional<std::pair<scf::ForOp, Value>> loopCarrier =
      resolveLoopCarrier(indexedExpr);
  if (!loopCarrier) {
    if (auto add = indexedExpr.getDefiningOp<arith::AddIOp>()) {
      loopCarrier = resolveLoopCarrier(add.getLhs());
      if (!loopCarrier)
        loopCarrier = resolveLoopCarrier(add.getRhs());
    }
  }
  if (!loopCarrier)
    return std::nullopt;

  auto [loop, invariantBase] = *loopCarrier;
  auto loopIv = cast<BlockArgument>(loop.getInductionVar());
  if (!loop || loop.getInductionVar() != loopIv ||
      !ValueAnalysis::isOneConstant(loop.getStep()) ||
      !loopWindowFitsSingleBlock(loop, blockSize))
    return std::nullopt;

  auto blockSizeConst = ValueAnalysis::tryFoldConstantIndex(
      ValueAnalysis::stripClampOne(blockSize));
  if (blockSizeConst && std::abs(constOffset) >= *blockSizeConst)
    return std::nullopt;

  Value zero = arts::createZeroIndex(builder, loc);
  Value one = arts::createOneIndex(builder, loc);
  Value negOne = arts::createConstantIndex(builder, loc, -1);

  Value lb = ValueAnalysis::ensureIndexType(loop.getLowerBound(), builder, loc);
  Value bs = ValueAnalysis::ensureIndexType(blockSize, builder, loc);
  Value sb = startBlock
                 ? ValueAnalysis::ensureIndexType(startBlock, builder, loc)
                 : zero;

  Value anchor = lb;
  if (invariantBase) {
    Value base = ValueAnalysis::ensureIndexType(invariantBase, builder, loc);
    anchor = builder.create<arith::AddIOp>(loc, base, lb);
  }

  Value baseBlock = builder.create<arith::DivUIOp>(loc, anchor, bs);
  Value baseLocal;
  if (invariantBase && arts::isAlignedToBlock(invariantBase, bs) &&
      arts::isKnownNonNegative(lb)) {
    /// When the invariant portion is already aligned to a block boundary, the
    /// in-block coordinate starts at the loop lower bound itself.
    baseLocal = lb;
  } else {
    baseLocal = builder.create<arith::RemUIOp>(loc, anchor, bs);
  }
  Value ivDelta = builder.create<arith::SubIOp>(loc, loopIv, lb);
  Value localRaw = builder.create<arith::AddIOp>(loc, baseLocal, ivDelta);
  if (constOffset > 0) {
    localRaw = builder.create<arith::AddIOp>(
        loc, localRaw, arts::createConstantIndex(builder, loc, constOffset));
  } else if (constOffset < 0) {
    localRaw = builder.create<arith::SubIOp>(
        loc, localRaw, arts::createConstantIndex(builder, loc, -constOffset));
  }

  Value baseRelBlock = builder.create<arith::SubIOp>(loc, baseBlock, sb);
  if (constOffset == 0 && ValueAnalysis::sameValue(baseLocal, lb) &&
      arts::isKnownNonNegative(lb) && arts::isLoopIvBoundedBy(loopIv, bs)) {
    return LoopWindowLocalization{baseRelBlock, loopIv};
  }

  Value belowZero = builder.create<arith::CmpIOp>(
      loc, arith::CmpIPredicate::slt, localRaw, zero);
  Value aboveBlock = builder.create<arith::CmpIOp>(
      loc, arith::CmpIPredicate::sge, localRaw, bs);

  Value adjust = builder.create<arith::SelectOp>(loc, belowZero, negOne, zero);
  adjust = builder.create<arith::SelectOp>(loc, aboveBlock, one, adjust);

  Value dbRefIdx = builder.create<arith::AddIOp>(loc, baseRelBlock, adjust);

  Value localIdx = localRaw;
  Value localMinusBlock = builder.create<arith::SubIOp>(loc, localRaw, bs);
  Value localPlusBlock = builder.create<arith::AddIOp>(loc, localRaw, bs);
  localIdx =
      builder.create<arith::SelectOp>(loc, belowZero, localPlusBlock, localIdx);
  localIdx = builder.create<arith::SelectOp>(loc, aboveBlock, localMinusBlock,
                                             localIdx);

  return LoopWindowLocalization{dbRefIdx, localIdx};
}

///===----------------------------------------------------------------------===///
/// Constructor
///===----------------------------------------------------------------------===///

DbBlockIndexer::DbBlockIndexer(const PartitionInfo &info,
                               ArrayRef<Value> startBlocks, unsigned outerRank,
                               unsigned innerRank, bool allocSingleBlock,
                               bool acquireSingleBlock, Value dominantZero)
    : DbIndexerBase(info, outerRank, innerRank),
      blockSizes(info.sizes.begin(), info.sizes.end()),
      partitionedDims(info.partitionedDims.begin(), info.partitionedDims.end()),
      startBlocks(startBlocks.begin(), startBlocks.end()),
      allocSingleBlock(allocSingleBlock),
      acquireSingleBlock(acquireSingleBlock), dominantZero(dominantZero) {}

///===----------------------------------------------------------------------===///
/// localize: multi-dimensional div/mod index localization
///===----------------------------------------------------------------------===///

LocalizedIndices DbBlockIndexer::localize(ArrayRef<Value> globalIndices,
                                          OpBuilder &builder, Location loc) {
  LocalizedIndices result;
  auto zero = [&]() {
    return dominantZero ? dominantZero : arts::createZeroIndex(builder, loc);
  };
  auto one = [&]() { return arts::createOneIndex(builder, loc); };
  auto trySimplifySingleBlockLocal = [&](Value globalIdx, Value startBlock,
                                         Value blockSize, Value &dbRefIdx,
                                         Value &localIdx) -> bool {
    if (!acquireSingleBlock)
      return false;

    if (auto loopLocalized = tryLocalizeFromLoopWindow(
            globalIdx, startBlock, blockSize, builder, loc)) {
      /// Single-block acquires already select the owning chunk, so blocked
      /// reindexing only needs the in-block coordinate.
      dbRefIdx = zero();
      localIdx = loopLocalized->localIdx;
      return true;
    }

    if (auto local =
            extractLocalFromBlockBase(globalIdx, startBlock, blockSize)) {
      Value localIdxVal = ValueAnalysis::ensureIndexType(*local, builder, loc);
      if (localIdxVal && isLoopIvBoundedBy(localIdxVal, blockSize)) {
        dbRefIdx = zero();
        localIdx = localIdxVal;
        return true;
      }
    }

    return false;
  };

  unsigned nPartDims = numPartitionedDims();
  ARTS_DEBUG("DbBlockIndexer::localize with "
             << globalIndices.size()
             << " indices, numPartitionedDims=" << nPartDims
             << ", innerRank=" << innerRank << ", outerRank=" << outerRank);

  if (globalIndices.empty()) {
    /// For empty indices with multi-dimensional partitioning, we need
    /// nPartDims zeros for db_ref (to select partition 0,0,...) and
    /// 1 zero for the memref inner access.
    for (unsigned i = 0; i < nPartDims; ++i)
      result.dbRefIndices.push_back(zero());
    /// Ensure at least 1 index for dbRef
    if (result.dbRefIndices.empty())
      result.dbRefIndices.push_back(zero());
    result.memrefIndices.push_back(zero());
    return result;
  }

  /// N-D block partitioning: apply div/mod to each partitioned dimension
  /// If partitionedDims is provided, map partitioning to non-leading dims.
  if (!partitionedDims.empty()) {
    unsigned rank = globalIndices.size();
    bool dimsInRange = true;
    for (unsigned d : partitionedDims) {
      if (d >= rank) {
        dimsInRange = false;
        break;
      }
    }
    if (innerRank >= rank && dimsInRange) {
      /// Initialize memref indices with global indices (or zero padding).
      result.memrefIndices.reserve(innerRank);
      for (unsigned d = 0; d < innerRank; ++d) {
        if (d < rank)
          result.memrefIndices.push_back(globalIndices[d]);
        else
          result.memrefIndices.push_back(zero());
      }

      for (unsigned p = 0; p < partitionedDims.size(); ++p) {
        unsigned dim = partitionedDims[p];
        Value globalIdx = globalIndices[dim];
        Value bs =
            (p < blockSizes.size() && blockSizes[p]) ? blockSizes[p] : one();
        bs = builder.create<arith::MaxUIOp>(loc, bs, one());

        Value sb = (p < startBlocks.size() && startBlocks[p]) ? startBlocks[p]
                                                              : zero();
        Value dbRefIdx;
        Value localIdx;
        bool simplified = false;
        if (trySimplifySingleBlockLocal(globalIdx, sb, bs, dbRefIdx, localIdx))
          simplified = true;
        if (!allocSingleBlock && !acquireSingleBlock) {
          if (auto loopLocalized =
                  tryLocalizeFromLoopWindow(globalIdx, sb, bs, builder, loc)) {
            dbRefIdx = loopLocalized->dbRefIdx;
            localIdx = loopLocalized->localIdx;
            simplified = true;
          }
        }
        if (!simplified && !allocSingleBlock && !acquireSingleBlock) {
          if (auto local = extractLocalFromBlockBase(globalIdx, sb, bs)) {
            Value localIdxVal =
                ValueAnalysis::ensureIndexType(*local, builder, loc);
            if (isLoopIvBoundedBy(localIdxVal, bs)) {
              dbRefIdx = zero();
              localIdx = localIdxVal;
              simplified = true;
            }
          }
        }
        if (!simplified) {
          if (allocSingleBlock || acquireSingleBlock) {
            dbRefIdx = zero();
          } else {
            Value physBlock =
                builder.create<arith::DivUIOp>(loc, globalIdx, bs);
            dbRefIdx = builder.create<arith::SubIOp>(loc, physBlock, sb);
          }
          if (acquireSingleBlock) {
            Value blockBase = builder.create<arith::MulIOp>(loc, sb, bs);
            localIdx = builder.create<arith::SubIOp>(loc, globalIdx, blockBase);
          } else {
            localIdx = builder.create<arith::RemUIOp>(loc, globalIdx, bs);
          }
        }
        result.dbRefIndices.push_back(dbRefIdx);

        if (dim < result.memrefIndices.size()) {
          result.memrefIndices[dim] = localIdx;
        }
      }

      /// Ensure at least one dbRef index
      if (result.dbRefIndices.empty())
        result.dbRefIndices.push_back(zero());
      return result;
    }
    ARTS_DEBUG("  Non-leading partition dims not applicable; falling back");
  }

  /// N-D block partitioning (leading dims): apply div/mod to each partitioned
  /// dimension For each partitioned dimension d, compute:
  ///   dbRefIdx[d]  = (globalIdx[d] / blockSize[d]) - startBlock[d]
  ///   memrefIdx[d] = globalIdx[d] % blockSize[d]
  ///
  /// However, we must respect innerRank (the element type's rank).
  /// For 2D array with 1D partitioning (e.g., memref<?x?xf64> ->
  /// memref<?xf64>):
  ///   - access [i, j] with nPartDims=1, innerRank=1
  ///   - dbRefIndices = [div(i)]
  ///   - memrefIndices = [j] (NOT [rem(i), j] which would be 2 indices!)
  /// Count non-partitioned dimensions that need to go into memrefIndices
  unsigned nonPartDims =
      globalIndices.size() > nPartDims ? globalIndices.size() - nPartDims : 0;

  for (unsigned d = 0; d < nPartDims && d < globalIndices.size(); ++d) {
    Value globalIdx = globalIndices[d];
    Value bs = (d < blockSizes.size() && blockSizes[d]) ? blockSizes[d] : one();
    bs = builder.create<arith::MaxUIOp>(loc, bs, one());

    Value sb =
        (d < startBlocks.size() && startBlocks[d]) ? startBlocks[d] : zero();
    Value dbRefIdx;
    Value localIdx;
    bool simplified = false;
    if (trySimplifySingleBlockLocal(globalIdx, sb, bs, dbRefIdx, localIdx))
      simplified = true;
    if (!allocSingleBlock && !acquireSingleBlock) {
      if (auto loopLocalized =
              tryLocalizeFromLoopWindow(globalIdx, sb, bs, builder, loc)) {
        dbRefIdx = loopLocalized->dbRefIdx;
        localIdx = loopLocalized->localIdx;
        simplified = true;
      }
    }
    if (!simplified && !allocSingleBlock && !acquireSingleBlock) {
      if (auto local = extractLocalFromBlockBase(globalIdx, sb, bs)) {
        Value localIdxVal =
            ValueAnalysis::ensureIndexType(*local, builder, loc);
        if (isLoopIvBoundedBy(localIdxVal, bs)) {
          dbRefIdx = zero();
          localIdx = localIdxVal;
          simplified = true;
        }
      }
    }
    if (!simplified) {
      if (allocSingleBlock || acquireSingleBlock) {
        dbRefIdx = zero();
      } else {
        /// Always compute full formula: relBlock = (globalIdx / bs) -
        /// startBlock The startBlock = 0 case will naturally simplify to
        /// physBlock
        Value physBlock = builder.create<arith::DivUIOp>(loc, globalIdx, bs);
        dbRefIdx = builder.create<arith::SubIOp>(loc, physBlock, sb);
      }
      if (acquireSingleBlock) {
        Value blockBase = builder.create<arith::MulIOp>(loc, sb, bs);
        localIdx = builder.create<arith::SubIOp>(loc, globalIdx, blockBase);
      } else {
        localIdx = builder.create<arith::RemUIOp>(loc, globalIdx, bs);
      }
    }
    result.dbRefIndices.push_back(dbRefIdx);

    /// Only add rem(localIdx) if there's room after reserving for
    /// non-partitioned dims
    if (result.memrefIndices.size() + nonPartDims < innerRank) {
      result.memrefIndices.push_back(localIdx);
      ARTS_DEBUG("    Dim " << d << ": dbRef=" << dbRefIdx
                            << ", memref=" << localIdx);
    } else {
      ARTS_DEBUG("    Dim " << d << ": dbRef=" << dbRefIdx
                            << " (skipping rem, innerRank=" << innerRank
                            << ")");
    }
  }

  /// Pad dbRefIndices with zeros if globalIndices has fewer dimensions than
  /// nPartDims. This happens when accessing a lower-dimensional array with
  /// multi-dimensional partitioning.
  for (unsigned d = globalIndices.size(); d < nPartDims; ++d) {
    result.dbRefIndices.push_back(zero());
    ARTS_DEBUG("    Dim " << d << ": dbRef=0 (padded)");
  }

  /// Pass through remaining non-partitioned dimensions, limited by innerRank
  for (unsigned d = nPartDims;
       d < globalIndices.size() && result.memrefIndices.size() < innerRank;
       ++d) {
    result.memrefIndices.push_back(globalIndices[d]);
    ARTS_DEBUG("    Pass-through dim " << d << ": " << globalIndices[d]);
  }

  ARTS_DEBUG("  -> dbRef: " << result.dbRefIndices.size()
                            << ", memref: " << result.memrefIndices.size());
  return result;
}

///===----------------------------------------------------------------------===///
/// localizeLinearized: de-linearize, div/mod, re-linearize
///===----------------------------------------------------------------------===///

LocalizedIndices DbBlockIndexer::localizeLinearized(Value globalLinearIndex,
                                                    Value stride,
                                                    OpBuilder &builder,
                                                    Location loc) {
  LocalizedIndices result;
  auto one = [&]() { return arts::createOneIndex(builder, loc); };
  auto zero = [&]() {
    return dominantZero ? dominantZero : arts::createZeroIndex(builder, loc);
  };

  ARTS_DEBUG("DbBlockIndexer::localizeLinearized - using div/mod for 2D");

  /// De-linearize: globalLinear = globalRow * stride + col
  Value globalRow =
      builder.create<arith::DivUIOp>(loc, globalLinearIndex, stride);
  Value globalCol =
      builder.create<arith::RemUIOp>(loc, globalLinearIndex, stride);

  /// Get block sizes for both dimensions
  /// For 2D blocking: bsRow for row dimension, bsCol for column dimension
  Value bsRow = (!blockSizes.empty() && blockSizes[0]) ? blockSizes[0] : one();
  bsRow = builder.create<arith::MaxUIOp>(loc, bsRow, one());

  /// Column block size: use blockSizes[1] if available, otherwise same as row
  Value bsCol =
      (blockSizes.size() > 1 && blockSizes[1]) ? blockSizes[1] : bsRow;
  bsCol = builder.create<arith::MaxUIOp>(loc, bsCol, one());

  if (allocSingleBlock || acquireSingleBlock) {
    result.dbRefIndices.push_back(zero());
  } else {
    Value sb =
        (!startBlocks.empty() && startBlocks[0]) ? startBlocks[0] : zero();

    /// dbRefIdx = (globalRow / blockSize) - startBlock
    Value physBlock = builder.create<arith::DivUIOp>(loc, globalRow, bsRow);
    Value relBlock = builder.create<arith::SubIOp>(loc, physBlock, sb);
    result.dbRefIndices.push_back(relBlock);
  }

  /// Compute local row (modulo block size)
  Value localRow;
  if (acquireSingleBlock) {
    Value sb =
        (!startBlocks.empty() && startBlocks[0]) ? startBlocks[0] : zero();
    Value blockBase = builder.create<arith::MulIOp>(loc, sb, bsRow);
    localRow = builder.create<arith::SubIOp>(loc, globalRow, blockBase);
  } else {
    localRow = builder.create<arith::RemUIOp>(loc, globalRow, bsRow);
  }

  /// Compute local col (modulo block size)
  /// For 2D blocks, the column also needs to be localized within the block
  Value localCol = builder.create<arith::RemUIOp>(loc, globalCol, bsCol);

  /// If innerRank >= 2, return separate indices to match element type rank
  /// Otherwise, re-linearize with LOCAL stride
  if (innerRank >= 2) {
    result.memrefIndices.push_back(localRow);
    result.memrefIndices.push_back(localCol);
    ARTS_DEBUG("  innerRank=" << innerRank
                              << ", returning separate [localRow, localCol]");
  } else {
    /// Re-linearize with LOCAL stride (block size), not global stride
    /// Block memory is [bsRow x bsCol], so stride is bsCol
    Value localLinear = builder.create<arith::MulIOp>(loc, localRow, bsCol);
    localLinear = builder.create<arith::AddIOp>(loc, localLinear, localCol);
    result.memrefIndices.push_back(localLinear);
    ARTS_DEBUG("  innerRank=" << innerRank
                              << ", re-linearized to single index");
  }

  return result;
}

///===----------------------------------------------------------------------===///
/// handleSubIndexOp: Block-specific subindex rewriting
///===----------------------------------------------------------------------===///

bool DbBlockIndexer::handleSubIndexOp(
    Operation *op, Value dbPtr, Type elementType, ArtsCodegen &AC,
    llvm::SetVector<Operation *> &opsToRemove) {
  auto subindex = dyn_cast<polygeist::SubIndexOp>(op);
  if (!subindex)
    return false;

  Location loc = op->getLoc();
  auto zero = AC.createIndexConstant(0, loc);

  ARTS_DEBUG(
      "  Handling subindex: elementType rank="
      << (isa<MemRefType>(elementType) ? cast<MemRefType>(elementType).getRank()
                                       : 0)
      << ", subindex result type rank="
      << (isa<MemRefType>(subindex.getResult().getType())
              ? cast<MemRefType>(subindex.getResult().getType()).getRank()
              : 0));

  SmallVector<Value> dbRefIndices;
  if (outerRank > 0) {
    Value globalIdx = subindex.getIndex();

    if (allocSingleBlock) {
      for (unsigned i = 0; i < outerRank; ++i)
        dbRefIndices.push_back(zero);
    } else {
      /// Apply div/mod like load/store (see localize())
      Value one = AC.createIndexConstant(1, loc);
      Value bs = (blockSizes.size() > 0 && blockSizes[0]) ? blockSizes[0] : one;
      bs = AC.getBuilder().create<arith::MaxUIOp>(loc, bs, one);
      Value sb =
          (startBlocks.size() > 0 && startBlocks[0]) ? startBlocks[0] : zero;

      Value physBlock =
          AC.getBuilder().create<arith::DivUIOp>(loc, globalIdx, bs);
      Value relBlock =
          AC.getBuilder().create<arith::SubIOp>(loc, physBlock, sb);

      dbRefIndices.push_back(relBlock); // Block-relative, not raw!
    }

    if (!allocSingleBlock) {
      for (unsigned i = 1; i < outerRank; ++i)
        dbRefIndices.push_back(zero);
    }
  } else {
    /// Fallback to a single zero index if there is no outer dimension.
    dbRefIndices.push_back(zero);
  }

  auto dbRef = AC.create<DbRefOp>(loc, elementType, dbPtr, dbRefIndices);

  /// Replace subindex uses with db_ref result and remove the subindex.
  subindex.replaceAllUsesWith(dbRef.getResult());
  opsToRemove.insert(subindex.getOperation());
  ARTS_DEBUG("  Replaced subindex with db_ref (block mode)");
  return true;
}

///===----------------------------------------------------------------------===///
/// localizeCoordinates: Block-specific coordinate localization
///===----------------------------------------------------------------------===///

SmallVector<Value>
DbBlockIndexer::localizeCoordinates(ArrayRef<Value> globalIndices,
                                    ArrayRef<Value> sliceOffsets,
                                    unsigned numIndexedDims, Type elementType,
                                    OpBuilder &builder, Location loc) {
  SmallVector<Value> result;
  auto zero = [&]() {
    return dominantZero ? dominantZero : arts::createZeroIndex(builder, loc);
  };

  ARTS_DEBUG("DbBlockIndexer::localizeCoordinates with " << globalIndices.size()
                                                         << " indices");

  /// Handle empty case
  if (globalIndices.empty()) {
    result.push_back(zero());
    return result;
  }

  /// BLOCK MODE: partitioned dimensions use div by blockSize
  unsigned nPartDims = numPartitionedDims();
  for (unsigned d = 0; d < globalIndices.size(); ++d) {
    Value globalIdx = globalIndices[d];

    if (d < numIndexedDims) {
      /// Indexed dimension: local index is always 0
      result.push_back(zero());
    } else if (d < numIndexedDims + nPartDims &&
               d - numIndexedDims < blockSizes.size()) {
      /// Partitioned dimension: compute block-relative index
      unsigned partIdx = d - numIndexedDims;
      Value bs = blockSizes[partIdx];
      Value sb = (partIdx < startBlocks.size()) ? startBlocks[partIdx] : zero();
      if (bs) {
        if (allocSingleBlock || acquireSingleBlock) {
          result.push_back(zero());
        } else {
          /// dbRefIdx = (globalIdx / blockSize) - startBlock
          Value physBlock = builder.create<arith::DivUIOp>(loc, globalIdx, bs);
          Value relBlock = builder.create<arith::SubIOp>(loc, physBlock, sb);
          result.push_back(relBlock);
        }
        ARTS_DEBUG("  Dim " << d << ": div/mod by blockSize[" << partIdx
                            << "]");
      } else {
        result.push_back(globalIdx);
      }
    } else if (d - numIndexedDims < sliceOffsets.size()) {
      /// Other sliced dimensions: subtract offset
      unsigned offsetIdx = d - numIndexedDims;
      Value offset = sliceOffsets[offsetIdx];
      Value local = builder.create<arith::SubIOp>(loc, globalIdx, offset);
      result.push_back(local);
    } else {
      /// Pass through unchanged
      result.push_back(globalIdx);
    }
  }

  return result;
}
