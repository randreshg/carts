///==========================================================================///
/// File: BlockInfoComputer.cpp
///
/// Implementation of BlockInfoComputer -- block offset/size computation
/// extracted from DbAcquireNode.
///==========================================================================///

#include "arts/Analysis/Graphs/Db/BlockInfoComputer.h"
#include "arts/Analysis/AccessPatternAnalysis.h"
#include "arts/Analysis/ArtsAnalysisManager.h"
#include "arts/Analysis/Db/DbAnalysis.h"
#include "arts/Analysis/Graphs/Db/DbNode.h"
#include "arts/Analysis/Graphs/Db/MemoryAccessClassifier.h"
#include "arts/Analysis/Graphs/Db/PartitionBoundsAnalyzer.h"
#include "arts/Analysis/Loop/LoopAnalysis.h"
#include "arts/ArtsDialect.h"
#include "arts/Utils/ArtsUtils.h"
#include "arts/Utils/DbUtils.h"
#include "arts/Utils/ValueUtils.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/Dominance.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/ADT/SmallVector.h"

using namespace mlir;
using namespace mlir::arts;
#include "arts/Utils/ArtsDebug.h"
ARTS_DEBUG_SETUP(block_info_computer);

///===----------------------------------------------------------------------===///
/// File-local helpers
///===----------------------------------------------------------------------===///

using AccessBoundsInfo = AccessBoundsResult;

/// Analyze all memory accesses to compute actual bounds relative to block base.
static AccessBoundsInfo
analyzeAccessBoundsLocal(DbAcquireNode *node, Value blockOffset, Value loopIV,
                         std::optional<unsigned> partitionDim = std::nullopt) {
  if (!node || !blockOffset || !loopIV)
    return AccessBoundsInfo{};

  DenseMap<DbRefOp, SetVector<Operation *>> dbRefToMemOps;
  MemoryAccessClassifier::collectAccessOperations(node, dbRefToMemOps);

  SmallVector<AccessIndexInfo, 16> accesses;

  for (auto &[dbRef, memOps] : dbRefToMemOps) {
    for (Operation *memOp : memOps) {
      SmallVector<Value> indexChain =
          DbUtils::collectFullIndexChain(dbRef, memOp);
      if (indexChain.empty())
        continue;
      AccessIndexInfo info;
      info.dbRefPrefix = dbRef.getIndices().size();
      info.indexChain.append(indexChain.begin(), indexChain.end());
      accesses.push_back(std::move(info));
    }
  }

  return analyzeAccessBoundsFromIndices(accesses, loopIV, blockOffset,
                                        partitionDim);
}

/// Find the best loop node for bounds analysis (same logic as in
/// PartitionBoundsAnalyzer).
static LoopNode *findBestLoopNode(ArrayRef<LoopNode *> loopNodes,
                                  Value firstDynIdx, Value partitionOffset,
                                  bool offsetIsZero) {
  LoopNode *bestNode = nullptr;
  int bestDepth = -1;
  bool foundPreferred = false;

  for (LoopNode *loopNode : loopNodes) {
    if (!loopNode->dependsOnInductionVar(firstDynIdx))
      continue;
    if (!offsetIsZero && !ValueUtils::dependsOn(firstDynIdx, partitionOffset))
      continue;

    Value loopIV = loopNode->getInductionVar();
    bool offsetDependsOnIV = !offsetIsZero && partitionOffset &&
                             ValueUtils::dependsOn(partitionOffset, loopIV);
    bool isPreferred = !offsetDependsOnIV;
    int depth = loopNode->getNestingDepth();

    if (isPreferred) {
      if (!foundPreferred || depth > bestDepth) {
        foundPreferred = true;
        bestDepth = depth;
        bestNode = loopNode;
      }
    } else if (!foundPreferred && depth > bestDepth) {
      bestDepth = depth;
      bestNode = loopNode;
    }
  }

  return bestNode;
}

///===----------------------------------------------------------------------===///
/// BlockInfoComputer implementation
///===----------------------------------------------------------------------===///

LogicalResult BlockInfoComputer::computeBlockInfo(DbAcquireNode *node,
                                                  Value &blockOffset,
                                                  Value &blockSize) {
  DbAcquireOp dbAcquireOp = node->getDbAcquireOp();
  ARTS_DEBUG("computeBlockInfo for acquire: " << dbAcquireOp);

  auto cached = node->getComputedBlockInfo();
  if (cached) {
    blockOffset = cached->first;
    blockSize = cached->second;
    ARTS_DEBUG("  using cached block info");
    return success();
  }

  auto [partitionOffset, partitionSize] = node->getPartitionInfo();

  auto fullRangeFallback = [&](StringRef reason) -> LogicalResult {
    DbAllocNode *allocNode = node->getRootAlloc();
    if (!allocNode) {
      ARTS_DEBUG("  no allocation node for full-range fallback");
      return failure();
    }
    DbAllocOp allocOp = allocNode->getDbAllocOp();
    if (!allocOp || allocOp.getElementSizes().empty()) {
      ARTS_DEBUG("  missing element sizes for full-range fallback");
      return failure();
    }

    OpBuilder builder(dbAcquireOp);
    Location loc = dbAcquireOp.getLoc();
    builder.setInsertionPoint(dbAcquireOp);

    blockOffset = arts::createZeroIndex(builder, loc);
    blockSize = allocOp.getElementSizes().front();
    ARTS_DEBUG("  full-range fallback (" << reason << ")");
    return success();
  };

  EdtOp edt = node->getEdtUser();
  if (!edt) {
    ARTS_DEBUG("  no EDT user found");
    return fullRangeFallback("no EDT user");
  }

  if (!MemoryAccessClassifier::hasMemoryAccesses(node) && !partitionOffset &&
      !partitionSize)
    return fullRangeFallback("pass-through acquire");

  ArtsAnalysisManager &AM = node->getAnalysis()->getAnalysisManager();
  LoopAnalysis &loopAnalysis = AM.getLoopAnalysis();
  SmallVector<LoopNode *> forLoopNodes;
  SmallVector<LoopNode *> whileLoopNodes;
  loopAnalysis.collectLoopsInOperation<scf::ForOp>(edt, forLoopNodes);
  loopAnalysis.collectLoopsInOperation<arts::ForOp>(edt, forLoopNodes);
  loopAnalysis.collectLoopsInOperation<scf::WhileOp>(edt, whileLoopNodes);

  scf::WhileOp blockWhile;
  if (!whileLoopNodes.empty()) {
    blockWhile = cast<scf::WhileOp>(whileLoopNodes[0]->getLoopOp());
  }
  if (blockWhile)
    ARTS_DEBUG("  found scf.while at " << blockWhile.getLoc());
  if (!forLoopNodes.empty())
    ARTS_DEBUG("  found " << forLoopNodes.size() << " for-like loops in EDT");

  if (partitionOffset && partitionSize) {
    if (succeeded(computeBlockInfoFromHints(node, blockOffset, blockSize))) {
      if (!forLoopNodes.empty()) {
        Value loopOffset, loopSize;
        if (succeeded(computeBlockInfoFromForLoop(node, forLoopNodes,
                                                  loopOffset, loopSize))) {
          bool useLoopSize = false;
          int64_t hintConst = 0;
          int64_t loopConst = 0;
          bool hintIsConst = ValueUtils::getConstantIndex(blockSize, hintConst);
          bool loopIsConst = ValueUtils::getConstantIndex(loopSize, loopConst);
          bool offsetRelated = false;
          if (loopOffset && blockOffset) {
            Value loopOffStripped = ValueUtils::stripNumericCasts(loopOffset);
            Value blockOffStripped = ValueUtils::stripNumericCasts(blockOffset);
            if (loopOffStripped == blockOffStripped)
              offsetRelated = true;
            if (!offsetRelated &&
                (ValueUtils::dependsOn(loopOffset, blockOffset) ||
                 ValueUtils::dependsOn(blockOffset, loopOffset)))
              offsetRelated = true;
            if (!offsetRelated) {
              int64_t loopOffConst = 0;
              int64_t blockOffConst = 0;
              if (ValueUtils::getConstantIndex(loopOffStripped, loopOffConst) &&
                  ValueUtils::getConstantIndex(blockOffStripped,
                                               blockOffConst) &&
                  loopOffConst == blockOffConst)
                offsetRelated = true;
            }
          }

          bool loopSizeDependsOnOffset =
              blockOffset && ValueUtils::dependsOn(loopSize, blockOffset);

          if (loopSizeDependsOnOffset)
            useLoopSize = true;
          if (offsetRelated && !loopIsConst)
            useLoopSize = true;
          if (offsetRelated && hintIsConst && loopIsConst &&
              hintConst != loopConst)
            useLoopSize = true;

          if (useLoopSize) {
            blockSize = loopSize;
            ARTS_DEBUG("  overriding blockSize from loops: " << blockSize);
          }
        }
        if (partitionOffset &&
            !ValueUtils::dependsOn(blockSize, partitionOffset)) {
          func::FuncOp func = dbAcquireOp->getParentOfType<func::FuncOp>();
          if (func) {
            OpBuilder builder(dbAcquireOp);
            Location loc = dbAcquireOp.getLoc();
            DominanceInfo domInfo(func);
            for (LoopNode *loopNode : forLoopNodes) {
              Value loopUB = loopNode->getUpperBound();
              if (!loopUB)
                continue;
              Value ubDom = ValueUtils::traceValueToDominating(
                  loopUB, dbAcquireOp, builder, domInfo, loc);
              if (!ubDom)
                continue;
              if (ValueUtils::dependsOn(ubDom, partitionOffset)) {
                blockSize = ubDom;
                ARTS_DEBUG("  overriding blockSize from loop upper bound: "
                           << blockSize);
                break;
              }
            }
          }
        }
      }
      return success();
    }
  }

  if (blockWhile) {
    Value offsetForCheck;
    if (succeeded(computeBlockInfoFromWhile(node, blockWhile, blockOffset,
                                            blockSize, &offsetForCheck))) {
      Value checkOffset = offsetForCheck ? offsetForCheck : blockOffset;
      if (!PartitionBoundsAnalyzer::canPartitionWithOffset(node, checkOffset))
        return fullRangeFallback("scf.while offset not derived from access");
      return success();
    }
    ARTS_DEBUG("  computeBlockInfoFromWhile failed");
    return fullRangeFallback("scf.while block info");
  }

  llvm::sort(forLoopNodes, [](LoopNode *a, LoopNode *b) {
    return a->getNestingDepth() < b->getNestingDepth();
  });

  return computeBlockInfoFromForLoop(node, forLoopNodes, blockOffset,
                                     blockSize);
}

LogicalResult BlockInfoComputer::computeBlockInfoFromWhile(
    DbAcquireNode *node, scf::WhileOp whileOp, Value &blockOffset,
    Value &blockSize, Value *offsetForCheck) {
  DbAcquireOp dbAcquireOp = node->getDbAcquireOp();
  if (!whileOp || !dbAcquireOp)
    return failure();
  Block &before = whileOp.getBefore().front();
  auto condition = dyn_cast<scf::ConditionOp>(before.getTerminator());
  if (!condition)
    return failure();

  LoopAnalysis &loopAnalysis =
      node->getAnalysis()->getAnalysisManager().getLoopAnalysis();
  LoopNode *loopNode = loopAnalysis.getOrCreateLoopNode(whileOp);
  if (!loopNode)
    return failure();

  Value loopIV = loopNode->getInductionVar();
  auto ivArg = loopIV.dyn_cast<BlockArgument>();
  if (!ivArg)
    return failure();
  if (ivArg.getOwner() != &before)
    return failure();

  unsigned ivIndex = ivArg.getArgNumber();
  if (ivIndex >= whileOp.getInits().size())
    return failure();
  Value initValue = whileOp.getInits()[ivIndex];
  if (offsetForCheck)
    *offsetForCheck = initValue;

  SmallVector<Value> whileBounds;
  arts::collectWhileBounds(condition.getCondition(), loopIV, whileBounds);
  if (whileBounds.empty())
    return failure();

  OpBuilder builder(dbAcquireOp);
  Location loc = dbAcquireOp.getLoc();
  builder.setInsertionPoint(dbAcquireOp);
  func::FuncOp func = dbAcquireOp->getParentOfType<func::FuncOp>();
  if (!func)
    return failure();
  DominanceInfo domInfo(func);

  Value initDom = ValueUtils::traceValueToDominating(initValue, dbAcquireOp,
                                                     builder, domInfo, loc);
  if (!initDom)
    return failure();

  Value startIdx = ValueUtils::ensureIndexType(initDom, builder, loc);
  if (!startIdx)
    return failure();

  SmallVector<Value> blockSizes;
  Value initStripped = ValueUtils::stripNumericCasts(initValue);
  for (Value bound : whileBounds) {
    Value boundStripped = ValueUtils::stripNumericCasts(bound);
    if (auto addOp = boundStripped.getDefiningOp<arith::AddIOp>()) {
      Value lhs = addOp.getLhs();
      Value rhs = addOp.getRhs();
      Value lhsStripped = ValueUtils::stripNumericCasts(lhs);
      Value rhsStripped = ValueUtils::stripNumericCasts(rhs);
      Value sizeCandidate;
      if (lhsStripped == initStripped)
        sizeCandidate = rhs;
      else if (rhsStripped == initStripped)
        sizeCandidate = lhs;

      if (sizeCandidate) {
        Value sizeDom = ValueUtils::traceValueToDominating(
            sizeCandidate, dbAcquireOp, builder, domInfo, loc);
        Value sizeIdx = ValueUtils::ensureIndexType(sizeDom, builder, loc);
        if (sizeIdx) {
          blockSizes.push_back(sizeIdx);
          continue;
        }
      }
    }

    Value boundDom = ValueUtils::traceValueToDominating(bound, dbAcquireOp,
                                                        builder, domInfo, loc);
    Value boundIdx = ValueUtils::ensureIndexType(boundDom, builder, loc);
    if (!boundIdx)
      continue;
    blockSizes.push_back(
        builder.create<arith::SubIOp>(loc, boundIdx, startIdx));
  }

  if (blockSizes.empty())
    return failure();

  Value minSize = blockSizes.front();
  for (Value candidate : llvm::drop_begin(blockSizes))
    minSize = builder.create<arith::MinSIOp>(loc, minSize, candidate);

  Value zero = arts::createZeroIndex(builder, loc);
  minSize = builder.create<arith::MaxSIOp>(loc, minSize, zero);

  auto [partitionOffset, partitionSize] = node->getPartitionInfo();
  std::optional<unsigned> partitionDim =
      partitionOffset ? PartitionBoundsAnalyzer::getPartitionOffsetDim(
                            node, partitionOffset, /*requireLeading=*/false)
                      : std::nullopt;
  AccessBoundsInfo bounds =
      analyzeAccessBoundsLocal(node, initValue, loopIV, partitionDim);
  if (!bounds.valid)
    return failure();

  if (!node->getStencilBounds()) {
    node->setStencilBoundsInternal(StencilBounds::create(
        bounds.minOffset, bounds.maxOffset, bounds.isStencil, bounds.valid));
  }

  Value adjustedOffset = startIdx;
  Value adjustedSize = minSize;

  if (bounds.minOffset < 0) {
    Value absAdj = arts::createConstantIndex(builder, loc, -bounds.minOffset);
    Value cond = builder.create<arith::CmpIOp>(loc, arith::CmpIPredicate::uge,
                                               startIdx, absAdj);
    Value sub = builder.create<arith::SubIOp>(loc, startIdx, absAdj);
    Value zeroIdx = arts::createZeroIndex(builder, loc);
    adjustedOffset = builder.create<arith::SelectOp>(loc, cond, sub, zeroIdx);
  } else if (bounds.minOffset > 0) {
    Value adj = arts::createConstantIndex(builder, loc, bounds.minOffset);
    adjustedOffset = builder.create<arith::AddIOp>(loc, startIdx, adj);
  }

  int64_t sizeAdjustment = bounds.maxOffset - bounds.minOffset;
  if (sizeAdjustment != 0) {
    Value adjustment = arts::createConstantIndex(builder, loc, sizeAdjustment);
    adjustedSize = builder.create<arith::AddIOp>(loc, minSize, adjustment);
  }

  blockOffset = adjustedOffset;
  blockSize = adjustedSize;
  return success();
}

LogicalResult BlockInfoComputer::computeBlockInfoFromHints(DbAcquireNode *node,
                                                           Value &blockOffset,
                                                           Value &blockSize) {
  auto [partitionOffset, partitionSize] = node->getPartitionInfo();
  DbAcquireOp dbAcquireOp = node->getDbAcquireOp();

  if (!partitionOffset || !partitionSize)
    return failure();

  ARTS_DEBUG("  computeBlockInfoFromHints: " << partitionOffset << " / "
                                             << partitionSize);
  if (!node->getOriginalBounds())
    node->setOriginalBounds(std::make_pair(partitionOffset, partitionSize));

  if (!PartitionBoundsAnalyzer::getPartitionOffsetDim(
          node, partitionOffset, /*requireLeading=*/false)) {
    ARTS_DEBUG("  offset hints not derived from access; failing");
    return failure();
  }

  std::optional<unsigned> partitionDim =
      PartitionBoundsAnalyzer::getPartitionOffsetDim(node, partitionOffset,
                                                     /*requireLeading=*/false);
  Value partitionIdx;
  Value firstDynIdx;
  DenseMap<DbRefOp, SetVector<Operation *>> dbRefToMemOps;
  MemoryAccessClassifier::collectAccessOperations(node, dbRefToMemOps);
  for (auto &[dbRef, memOps] : dbRefToMemOps) {
    for (Operation *memOp : memOps) {
      SmallVector<Value> fullChain =
          DbUtils::collectFullIndexChain(dbRef, memOp);
      if (!partitionIdx && partitionDim) {
        unsigned memrefStart = dbRef.getIndices().size();
        unsigned chainIdx = memrefStart + *partitionDim;
        if (chainIdx < fullChain.size())
          partitionIdx = fullChain[chainIdx];
      }
      for (Value idx : fullChain) {
        int64_t constVal;
        if (!ValueUtils::getConstantIndex(idx, constVal)) {
          firstDynIdx = idx;
          break;
        }
      }
      if (partitionIdx || firstDynIdx)
        break;
    }
    if (partitionIdx || firstDynIdx)
      break;
  }

  Value analysisOffset = partitionOffset;
  analysisOffset = ValueUtils::stripConstantOffset(analysisOffset, nullptr);

  bool offsetIsZero =
      ValueUtils::isZeroConstant(ValueUtils::stripNumericCasts(analysisOffset));
  Value loopIdx = partitionIdx ? partitionIdx : firstDynIdx;
  if (!loopIdx) {
    if (offsetIsZero) {
      blockOffset = partitionOffset;
      blockSize = partitionSize;
      return success();
    }
    return failure();
  }

  EdtOp edt = node->getEdtUser();
  ArtsAnalysisManager &AM = node->getAnalysis()->getAnalysisManager();
  LoopAnalysis &loopAnalysis = AM.getLoopAnalysis();
  SmallVector<LoopNode *> forLoopNodes;
  loopAnalysis.collectForLoopsInOperation(edt, forLoopNodes);

  LoopNode *boundsLoopNode =
      findBestLoopNode(forLoopNodes, loopIdx, analysisOffset, offsetIsZero);

  if (boundsLoopNode) {
    Value loopIV = boundsLoopNode->getInductionVar();
    AccessBoundsInfo bounds =
        analyzeAccessBoundsLocal(node, analysisOffset, loopIV, partitionDim);

    if (bounds.valid) {
      if (!node->getStencilBounds())
        node->setStencilBoundsInternal(
            StencilBounds::create(bounds.minOffset, bounds.maxOffset,
                                  bounds.isStencil, bounds.valid));

      OpBuilder builder(dbAcquireOp);
      Location loc = dbAcquireOp.getLoc();
      builder.setInsertionPoint(dbAcquireOp);

      if (bounds.minOffset != 0) {
        Value adjustment =
            arts::createConstantIndex(builder, loc, bounds.minOffset);
        blockOffset =
            builder.create<arith::AddIOp>(loc, partitionOffset, adjustment);
      } else {
        blockOffset = partitionOffset;
      }

      int64_t sizeAdjustment = bounds.maxOffset - bounds.minOffset;
      if (sizeAdjustment != 0) {
        Value adjustment =
            arts::createConstantIndex(builder, loc, sizeAdjustment);
        blockSize =
            builder.create<arith::AddIOp>(loc, partitionSize, adjustment);
      } else {
        blockSize = partitionSize;
      }

      return success();
    }
  }

  blockOffset = partitionOffset;
  blockSize = partitionSize;
  return success();
}

LogicalResult BlockInfoComputer::computeBlockInfoFromForLoop(
    DbAcquireNode *node, ArrayRef<LoopNode *> loopNodes, Value &blockOffset,
    Value &blockSize, Value *offsetForCheck) {
  DbAcquireOp dbAcquireOp = node->getDbAcquireOp();
  Value ptrArg = node->getUseInEdt();
  EdtOp edtUser = node->getEdtUser();
  if (!ptrArg) {
    ARTS_DEBUG("  computeBlockInfoFromForLoop: no EDT block argument");
    return failure();
  }
  if (!edtUser) {
    ARTS_DEBUG("  computeBlockInfoFromForLoop: no EDT user");
    return failure();
  }

  if (loopNodes.empty()) {
    ARTS_DEBUG("  computeBlockInfoFromForLoop: no scf.for found");
    return failure();
  }

  OpBuilder builder(dbAcquireOp);
  Location loc = dbAcquireOp.getLoc();
  builder.setInsertionPoint(dbAcquireOp);

  auto [partitionOffset, partitionSize] = node->getPartitionInfo();
  std::optional<unsigned> partitionDim =
      partitionOffset ? PartitionBoundsAnalyzer::getPartitionOffsetDim(
                            node, partitionOffset, /*requireLeading=*/false)
                      : std::nullopt;

  auto getLoopUpperBound = [&](LoopNode *ln) -> Value {
    return ln ? ln->getUpperBound() : Value();
  };

  for (LoopNode *loopNode : loopNodes) {
    Value offsetCandidate;
    Operation *loopOp = loopNode->getLoopOp();
    if (!loopOp || !isa<scf::ForOp, arts::ForOp>(loopOp))
      continue;
    Value loopIV = loopNode->getInductionVar();
    auto pickCandidateFromIndex = [&](Value idx) -> Value {
      Value stripped = ValueUtils::stripNumericCasts(idx);
      if (stripped == loopIV)
        return arts::createZeroIndex(builder, loc);

      if (auto addOp = stripped.getDefiningOp<arith::AddIOp>()) {
        Value lhs = addOp.getLhs();
        Value rhs = addOp.getRhs();
        bool lhsIV = loopNode->dependsOnInductionVarNormalized(lhs);
        bool rhsIV = loopNode->dependsOnInductionVarNormalized(rhs);
        if (lhsIV && !rhsIV && loopNode->isValueLoopInvariant(rhs))
          return rhs;
        if (rhsIV && !lhsIV && loopNode->isValueLoopInvariant(lhs))
          return lhs;
      }
      return Value();
    };

    for (Operation *user : ptrArg.getUsers()) {
      auto refOp = dyn_cast<DbRefOp>(user);
      if (!refOp || !loopOp->isAncestor(refOp))
        continue;

      SetVector<Operation *> memOps;
      DbUtils::collectReachableMemoryOps(refOp.getResult(), memOps,
                                         &edtUser.getBody());
      for (Operation *memUser : memOps) {
        if (!loopOp->isAncestor(memUser))
          continue;

        SmallVector<Value> memIndices =
            DbUtils::getMemoryAccessIndices(memUser);
        if (memIndices.empty())
          continue;
        Value idx = memIndices.front();
        if (partitionDim && *partitionDim < memIndices.size())
          idx = memIndices[*partitionDim];
        offsetCandidate = pickCandidateFromIndex(idx);

        if (offsetCandidate)
          break;
      }

      if (!offsetCandidate) {
        for (Value refIdx : refOp.getIndices()) {
          offsetCandidate = pickCandidateFromIndex(refIdx);
          if (offsetCandidate)
            break;
        }
      }
      if (offsetCandidate)
        break;
    }

    if (!offsetCandidate) {
      ARTS_DEBUG("  no offset candidate found in loop at " << loopOp->getLoc());
      continue;
    }

    AccessBoundsInfo bounds =
        analyzeAccessBoundsLocal(node, offsetCandidate, loopIV, partitionDim);
    if (!bounds.valid) {
      ARTS_DEBUG("  bounds analysis failed for loop at " << loopOp->getLoc());
      continue;
    }

    if (!node->getStencilBounds())
      node->setStencilBoundsInternal(StencilBounds::create(
          bounds.minOffset, bounds.maxOffset, bounds.isStencil, bounds.valid));

    func::FuncOp func = dbAcquireOp->getParentOfType<func::FuncOp>();
    if (!func)
      continue;
    DominanceInfo domInfo(func);

    Value offsetDom = ValueUtils::traceValueToDominating(
        offsetCandidate, dbAcquireOp, builder, domInfo, loc);
    Value loopUpperBound = getLoopUpperBound(loopNode);
    if (!loopUpperBound)
      continue;
    Value sizeDom = ValueUtils::traceValueToDominating(
        loopUpperBound, dbAcquireOp, builder, domInfo, loc);
    if (!offsetDom || !sizeDom) {
      ARTS_DEBUG("  offset/size does not dominate for loop at "
                 << loopOp->getLoc());
      continue;
    }

    Value adjustedOffset = ValueUtils::ensureIndexType(offsetDom, builder, loc);
    Value adjustedSize = ValueUtils::ensureIndexType(sizeDom, builder, loc);
    if (!adjustedOffset || !adjustedSize)
      continue;

    if (bounds.minOffset < 0) {
      Value absAdj = arts::createConstantIndex(builder, loc, -bounds.minOffset);
      Value cond = builder.create<arith::CmpIOp>(loc, arith::CmpIPredicate::uge,
                                                 adjustedOffset, absAdj);
      Value sub = builder.create<arith::SubIOp>(loc, adjustedOffset, absAdj);
      Value zeroIdx = arts::createZeroIndex(builder, loc);
      adjustedOffset = builder.create<arith::SelectOp>(loc, cond, sub, zeroIdx);
    } else if (bounds.minOffset > 0) {
      Value adj = arts::createConstantIndex(builder, loc, bounds.minOffset);
      adjustedOffset = builder.create<arith::AddIOp>(loc, adjustedOffset, adj);
    }

    int64_t sizeAdjustment = bounds.maxOffset - bounds.minOffset;
    if (sizeAdjustment != 0) {
      Value adjustment =
          arts::createConstantIndex(builder, loc, sizeAdjustment);
      adjustedSize =
          builder.create<arith::AddIOp>(loc, adjustedSize, adjustment);
    }

    blockOffset = adjustedOffset;
    blockSize = adjustedSize;
    if (offsetForCheck)
      *offsetForCheck = offsetCandidate;
    ARTS_DEBUG("  selected loop " << loopOp->getLoc() << " blockOffset="
                                  << blockOffset << " blockSize=" << blockSize);

    Value checkOffset = offsetForCheck ? *offsetForCheck : blockOffset;
    if (!PartitionBoundsAnalyzer::getPartitionOffsetDim(
            node, checkOffset, /*requireLeading=*/false))
      return failure();
    return success();
  }

  ARTS_DEBUG("  no suitable scf.for loop found for block info");
  return failure();
}
