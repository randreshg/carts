///==========================================================================///
/// File: PartitionBoundsAnalyzer.cpp
///
/// Implementation of PartitionBoundsAnalyzer -- partition bound computation
/// and eligibility checking extracted from DbAcquireNode.
///==========================================================================///

#include "arts/Analysis/Graphs/Db/PartitionBoundsAnalyzer.h"
#include "arts/Analysis/AccessPatternAnalysis.h"
#include "arts/Analysis/ArtsAnalysisManager.h"
#include "arts/Analysis/Db/DbAnalysis.h"
#include "arts/Analysis/Edt/EdtAnalysis.h"
#include "arts/Analysis/Graphs/Db/DbNode.h"
#include "arts/Analysis/Graphs/Db/MemoryAccessClassifier.h"
#include "arts/Analysis/Graphs/Edt/EdtGraph.h"
#include "arts/Analysis/Graphs/Edt/EdtNode.h"
#include "arts/Analysis/Loop/LoopAnalysis.h"
#include "arts/ArtsDialect.h"
#include "arts/Utils/ArtsUtils.h"
#include "arts/Utils/DbUtils.h"
#include "arts/Utils/OperationAttributes.h"
#include "arts/Utils/ValueUtils.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/BuiltinTypes.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SetVector.h"

using namespace mlir;
using namespace mlir::arts;
#include "arts/Utils/ArtsDebug.h"
ARTS_DEBUG_SETUP(partition_bounds_analyzer);

///===----------------------------------------------------------------------===///
/// File-local helpers
///===----------------------------------------------------------------------===///

using AccessBoundsInfo = AccessBoundsResult;

/// Find the best loop node for bounds analysis.
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

/// Analyze all memory accesses to compute actual bounds relative to block base.
static AccessBoundsInfo
analyzeAccessBounds(DbAcquireNode *node, Value blockOffset, Value loopIV,
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

///===----------------------------------------------------------------------===///
/// PartitionBoundsAnalyzer implementation
///===----------------------------------------------------------------------===///

bool PartitionBoundsAnalyzer::hasValidEdtAndAccesses(DbAcquireNode *node) {
  auto skip = [&](StringRef reason) {
    ARTS_DEBUG("Skipping acquire " << node->getDbAcquireOp() << ": " << reason);
    return false;
  };

  if (!node->getDbAcquireOp())
    return skip("invalid acquire operation");

  if (!node->getEdtUserOp()) {
    if (!MemoryAccessClassifier::hasMemoryAccesses(node)) {
      ARTS_DEBUG("No EDT user and no memory accesses - full-range acquire");
      return true;
    }
    DbAllocNode *allocNode = node->getRootAlloc();
    if (allocNode) {
      DbAllocOp allocOp = allocNode->getDbAllocOp();
      if (allocOp && !allocOp.getElementSizes().empty()) {
        ARTS_DEBUG("No EDT user - full-range acquire with alloc sizes");
        return true;
      }
    }
    return skip("no EDT user and missing allocation sizes");
  }

  EdtOp edt = node->getEdtUser();
  if (!edt || !edt.getOperation())
    return skip("missing EDT");

  bool hasMemAccesses = MemoryAccessClassifier::hasMemoryAccesses(node);
  bool isTaskEdt = (edt.getType() == EdtType::task);
  bool isPassThrough = !isTaskEdt && !hasMemAccesses;

  if (!isTaskEdt && !isPassThrough)
    return skip("not a task EDT");

  if (isTaskEdt && !hasMemAccesses)
    return skip("no memory accesses");

  DbAllocNode *allocNode = node->getRootAlloc();
  if (!allocNode)
    return skip("missing allocation node");

  if (!allocNode->isParallelFriendly()) {
    ARTS_DEBUG("Memref not marked parallel-friendly - allowing for heuristic "
               "evaluation");
  }

  ArtsAnalysisManager &AM = node->getAnalysis()->getAnalysisManager();
  EdtAnalysis &edtAnalysis = AM.getEdtAnalysis();

  func::FuncOp func = edt->getParentOfType<func::FuncOp>();
  if (!func)
    return skip("unable to find parent function");

  EdtGraph &edtGraph = edtAnalysis.getOrCreateEdtGraph(func);
  EdtNode *edtNode = edtGraph.getEdtNode(edt);
  if (!edtNode)
    return skip("missing EDT node");

  if (isTaskEdt && !edtNode->hasParallelLoopMetadata()) {
    ARTS_DEBUG("Skipping parallel loop metadata check: treating as full-range");
    node->setPartitionInfo(Value(), Value());
  }

  return true;
}

bool PartitionBoundsAnalyzer::computePartitionBounds(DbAcquireNode *node) {
  DbAcquireOp mutableAcquire = DbAcquireOp(node->getDbAcquireOp().getOperation());

  node->setPartitionInfo(Value(), Value());
  node->setHasNonConstantOffset(false);

  auto pickRepresentative = [&](ValueRange vals, unsigned &idx) -> Value {
    idx = 0;
    for (unsigned i = 0; i < vals.size(); ++i) {
      Value v = vals[i];
      if (!v)
        continue;
      int64_t c = 0;
      if (!ValueUtils::getConstantIndex(ValueUtils::stripNumericCasts(v), c)) {
        idx = i;
        return v;
      }
    }
    for (unsigned i = 0; i < vals.size(); ++i) {
      if (vals[i]) {
        idx = i;
        return vals[i];
      }
    }
    return Value();
  };

  unsigned offsetIdx = 0;
  auto hintMode = mutableAcquire.getPartitionMode();
  bool preferPartitionIndices =
      !hintMode || *hintMode == PartitionMode::fine_grained;

  Value partitionOffset, partitionSize;

  if (preferPartitionIndices && !mutableAcquire.getPartitionIndices().empty())
    partitionOffset =
        pickRepresentative(mutableAcquire.getPartitionIndices(), offsetIdx);
  else if (!mutableAcquire.getPartitionOffsets().empty())
    partitionOffset =
        pickRepresentative(mutableAcquire.getPartitionOffsets(), offsetIdx);
  else if (!mutableAcquire.getPartitionIndices().empty())
    partitionOffset =
        pickRepresentative(mutableAcquire.getPartitionIndices(), offsetIdx);
  if (!mutableAcquire.getPartitionSizes().empty()) {
    if (offsetIdx < mutableAcquire.getPartitionSizes().size())
      partitionSize = mutableAcquire.getPartitionSizes()[offsetIdx];
    else
      partitionSize = mutableAcquire.getPartitionSizes().front();
  }

  if (!partitionOffset && !partitionSize) {
    if (auto mode = mutableAcquire.getPartitionMode();
        mode &&
        (*mode == PartitionMode::block || *mode == PartitionMode::stencil ||
         *mode == PartitionMode::fine_grained)) {
      offsetIdx = 0;
      bool preferDbIndices = (*mode == PartitionMode::fine_grained);
      if (preferDbIndices && !mutableAcquire.getIndices().empty())
        partitionOffset =
            pickRepresentative(mutableAcquire.getIndices(), offsetIdx);
      else if (!mutableAcquire.getOffsets().empty())
        partitionOffset =
            pickRepresentative(mutableAcquire.getOffsets(), offsetIdx);
      else if (!mutableAcquire.getIndices().empty())
        partitionOffset =
            pickRepresentative(mutableAcquire.getIndices(), offsetIdx);
      if (!mutableAcquire.getSizes().empty()) {
        if (offsetIdx < mutableAcquire.getSizes().size())
          partitionSize = mutableAcquire.getSizes()[offsetIdx];
        else
          partitionSize = mutableAcquire.getSizes().front();
      }
    }
  }

  node->setPartitionInfo(partitionOffset, partitionSize);

  if (!node->getEdtUserOp())
    return true;

  bool hasPartitionHint = !mutableAcquire.getPartitionIndices().empty() ||
                          !mutableAcquire.getPartitionOffsets().empty() ||
                          !mutableAcquire.getPartitionSizes().empty();
  if (partitionOffset && !canPartitionWithOffset(node, partitionOffset)) {
    if (hasPartitionHint) {
      ARTS_DEBUG("Partition offset not derived from access; "
                 "continuing with partition hint");
    } else {
      ARTS_DEBUG("Partition offset not derived from access; "
                 "allowing for heuristic evaluation");
    }
  }

  if (!partitionOffset || !partitionSize)
    return true;

  Value analysisOffset = partitionOffset;
  analysisOffset = ValueUtils::stripConstantOffset(analysisOffset, nullptr);

  node->setOriginalBounds(std::make_pair(partitionOffset, partitionSize));
  ArtsAnalysisManager &AM = node->getAnalysis()->getAnalysisManager();
  LoopAnalysis &loopAnalysis = AM.getLoopAnalysis();

  DenseMap<DbRefOp, SetVector<Operation *>> dbRefToMemOps;
  MemoryAccessClassifier::collectAccessOperations(node, dbRefToMemOps);

  std::optional<unsigned> partitionDim =
      analysisOffset
          ? getPartitionOffsetDim(node, analysisOffset, /*requireLeading=*/false)
          : std::nullopt;

  Value partitionIdx;
  Value firstDynIdx;
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

  bool offsetIsZero =
      ValueUtils::isZeroConstant(ValueUtils::stripNumericCasts(analysisOffset));
  Value loopIdx = partitionIdx ? partitionIdx : firstDynIdx;
  if (!loopIdx) {
    if (offsetIsZero) {
      ARTS_DEBUG("  No dynamic index - allowing zero-offset partition hints");
      node->setComputedBlockInfo(std::make_pair(partitionOffset, partitionSize));
      return true;
    }
    ARTS_DEBUG("  No dynamic index - allowing for heuristic evaluation");
    return true;
  }

  EdtOp edt = node->getEdtUser();
  SmallVector<LoopNode *, 4> loopNodes;
  loopAnalysis.collectForLoopsInOperation(edt, loopNodes);

  if (loopNodes.empty()) {
    ARTS_DEBUG("  No for loops - allowing for heuristic evaluation");
    return true;
  }

  LoopNode *blockLoopNode =
      findBestLoopNode(loopNodes, loopIdx, analysisOffset, offsetIsZero);

  if (!blockLoopNode) {
    ARTS_DEBUG("  No block loop found - allowing for heuristic evaluation");
    return true;
  }

  Value loopIV = blockLoopNode->getInductionVar();
  AccessBoundsInfo bounds =
      analyzeAccessBounds(node, analysisOffset, loopIV, partitionDim);

  if (!bounds.valid) {
    if (bounds.hasVariableOffset && partitionDim)
      node->setHasNonConstantOffset(true);
    ARTS_DEBUG(
        "  Access bounds analysis failed - allowing for heuristic evaluation");
    return true;
  }

  DbAcquireOp dbAcquireOp = node->getDbAcquireOp();
  ARTS_DEBUG("  Bounds analysis for acquire "
             << dbAcquireOp << ": minOffset=" << bounds.minOffset
             << ", maxOffset=" << bounds.maxOffset
             << ", isStencil=" << bounds.isStencil);

  node->setStencilBoundsInternal(StencilBounds::create(
      bounds.minOffset, bounds.maxOffset, bounds.isStencil, bounds.valid));

  OpBuilder builder(dbAcquireOp);
  Location loc = dbAcquireOp.getLoc();
  builder.setInsertionPoint(dbAcquireOp);

  if (bounds.valid && bounds.isStencil && bounds.centerOffset != 0)
    setStencilCenterOffset(dbAcquireOp.getOperation(), bounds.centerOffset);

  Value adjustedOffset = partitionOffset;
  Value adjustedSize = partitionSize;

  if (bounds.minOffset < 0) {
    Value absAdj = arts::createConstantIndex(builder, loc, -bounds.minOffset);
    Value cond = builder.create<arith::CmpIOp>(loc, arith::CmpIPredicate::uge,
                                               partitionOffset, absAdj);
    Value sub = builder.create<arith::SubIOp>(loc, partitionOffset, absAdj);
    Value zero = arts::createZeroIndex(builder, loc);
    adjustedOffset = builder.create<arith::SelectOp>(loc, cond, sub, zero);
  } else if (bounds.minOffset > 0) {
    Value adj = arts::createConstantIndex(builder, loc, bounds.minOffset);
    adjustedOffset = builder.create<arith::AddIOp>(loc, partitionOffset, adj);
  }

  int64_t sizeAdjustment = bounds.maxOffset - bounds.minOffset;
  if (sizeAdjustment != 0) {
    Value adjustment = arts::createConstantIndex(builder, loc, sizeAdjustment);
    adjustedSize =
        builder.create<arith::AddIOp>(loc, partitionSize, adjustment);
  }

  node->setComputedBlockInfo(std::make_pair(adjustedOffset, adjustedSize));
  return true;
}

bool PartitionBoundsAnalyzer::canBePartitioned(DbAcquireNode *node) {
  if (!node->getEdtUserOp())
    return false;

  if (!hasValidEdtAndAccesses(node))
    return false;

  if (!computePartitionBounds(node))
    return false;

  bool childFailed = false;
  node->forEachChildNode([&](NodeBase *child) {
    if (childFailed)
      return;
    auto *childAcq = dyn_cast<DbAcquireNode>(child);
    if (childAcq && !canBePartitioned(childAcq)) {
      ARTS_DEBUG("Skipping acquire " << node->getDbAcquireOp()
                                     << ": nested acquire child failed");
      childFailed = true;
    }
  });

  if (childFailed)
    return false;

  ARTS_DEBUG("PASS: acquire can be partitioned: " << node->getDbAcquireOp());
  return true;
}

bool PartitionBoundsAnalyzer::canPartitionWithOffset(DbAcquireNode *node,
                                                     Value offset) {
  return getPartitionOffsetDim(node, offset, /*requireLeading=*/false)
      .has_value();
}

std::optional<unsigned>
PartitionBoundsAnalyzer::getPartitionOffsetDim(DbAcquireNode *node,
                                               Value offset,
                                               bool requireLeading) {
  ARTS_DEBUG("getPartitionOffsetDim called with offset="
             << offset << " requireLeading=" << requireLeading);
  if (!offset) {
    ARTS_DEBUG("  -> returning none (no offset)");
    return std::nullopt;
  }
  Value offsetStripped = ValueUtils::stripNumericCasts(offset);
  ARTS_DEBUG("  offsetStripped=" << offsetStripped << " isZero="
                                 << ValueUtils::isZeroConstant(offsetStripped));

  int64_t constVal;
  if (ValueUtils::getConstantIndex(offsetStripped, constVal)) {
    ARTS_DEBUG("  -> returning none (constant offset "
               << constVal << " cannot be partition variable)");
    return std::nullopt;
  }

  int64_t offsetConst = 0;
  Value offsetBase =
      ValueUtils::stripConstantOffset(offsetStripped, &offsetConst);
  if (offsetBase && offsetBase != offsetStripped) {
    ARTS_DEBUG("  normalized offset base=" << offsetBase
                                           << " const=" << offsetConst);
    offsetStripped = offsetBase;
  }

  Value normalizedOffset = ValueUtils::stripSelectClamp(offsetStripped);
  auto matchesPartitionOffset = [&](Value candidate) -> bool {
    if (!candidate || !normalizedOffset)
      return false;
    Value normalizedCandidate =
        ValueUtils::stripNumericCasts(ValueUtils::stripSelectClamp(candidate));
    Value normalized = ValueUtils::stripNumericCasts(normalizedOffset);
    if (ValueUtils::sameValue(normalizedCandidate, normalized) ||
        ValueUtils::areValuesEquivalent(normalizedCandidate, normalized))
      return true;
    return ValueUtils::dependsOn(normalizedCandidate, normalized);
  };

  auto dependsOnPartitionOffset = [&](Value candidate,
                                      Value &dependencyRoot) -> bool {
    dependencyRoot = Value();
    if (!candidate)
      return false;
    if (ValueUtils::dependsOn(candidate, offsetStripped)) {
      dependencyRoot = offsetStripped;
      return true;
    }
    if (normalizedOffset && normalizedOffset != offsetStripped &&
        ValueUtils::dependsOn(candidate, normalizedOffset)) {
      dependencyRoot = normalizedOffset;
      return true;
    }
    return false;
  };

  DenseMap<DbRefOp, SetVector<Operation *>> dbRefToMemOps;
  MemoryAccessClassifier::collectAccessOperations(node, dbRefToMemOps);

  if (dbRefToMemOps.empty()) {
    ARTS_DEBUG("  -> returning dim 0 (no memory operations)");
    return 0u;
  }
  ARTS_DEBUG("  found " << dbRefToMemOps.size() << " db_ref users with memOps");

  std::optional<unsigned> foundDim;
  for (auto &[dbRef, memOps] : dbRefToMemOps) {
    for (Operation *memOp : memOps) {
      SmallVector<Value> fullChain =
          DbUtils::collectFullIndexChain(dbRef, memOp);
      if (fullChain.empty())
        continue;

      unsigned memrefStart = dbRef.getIndices().size();
      if (memrefStart >= fullChain.size())
        continue;

      for (unsigned i = 0; i < memrefStart; ++i) {
        if (ValueUtils::dependsOn(fullChain[i], offsetStripped)) {
          ARTS_DEBUG("  partition offset appears in db_ref index; "
                     "disabling blocked partitioning");
          return std::nullopt;
        }
      }

      int64_t firstDynPos = -1;
      int64_t matchedPos = -1;
      Value matchedIdx;
      for (unsigned i = memrefStart; i < fullChain.size(); ++i) {
        int64_t constVal = 0;
        bool isConst = ValueUtils::getConstantIndex(fullChain[i], constVal);
        if (!isConst && firstDynPos < 0)
          firstDynPos = static_cast<int64_t>(i - memrefStart);

        Value dependencyRoot;
        bool directDepends =
            dependsOnPartitionOffset(fullChain[i], dependencyRoot);
        bool matchedOffset =
            directDepends || matchesPartitionOffset(fullChain[i]);
        if (matchedOffset) {
          if (directDepends) {
            if (auto stride =
                    ValueUtils::getOffsetStride(fullChain[i], dependencyRoot)) {
              if (*stride != 1) {
                ARTS_DEBUG("  partition offset has stride "
                           << *stride << "; disabling blocked partitioning");
                return std::nullopt;
              }
            }
            if (arts::isIndirectIndex(fullChain[i], dependencyRoot)) {
              ARTS_DEBUG("  indirect index detected; disabling blocked "
                         "partitioning");
              return std::nullopt;
            }
          }
          int64_t dim = static_cast<int64_t>(i - memrefStart);
          if (matchedPos >= 0 && matchedPos != dim) {
            if (matchedIdx && fullChain[i] == matchedIdx) {
              ARTS_DEBUG("  same index in multiple dims (diagonal); "
                         "keeping leading dim "
                         << matchedPos);
              continue;
            }
            ARTS_DEBUG("  partition offset maps to multiple dims; "
                       "disabling blocked partitioning");
            return std::nullopt;
          }
          matchedPos = dim;
          matchedIdx = fullChain[i];
          if (requireLeading && firstDynPos >= 0 && matchedPos != firstDynPos) {
            ARTS_DEBUG("  partition offset maps to non-leading dim; "
                       "disabling blocked partitioning");
            return std::nullopt;
          }
        }
      }

      if (matchedPos < 0) {
        ARTS_DEBUG("  -> returning none (offset not in access pattern)");
        return std::nullopt;
      }

      unsigned matchedIdxPos = memrefStart + matchedPos;
      if (matchedIdxPos < fullChain.size()) {
        Value idx = fullChain[matchedIdxPos];
        int64_t constVal;
        if (!ValueUtils::getConstantIndex(idx, constVal)) {
          Value dependencyRoot;
          if (dependsOnPartitionOffset(idx, dependencyRoot) &&
              arts::isIndirectIndex(idx, dependencyRoot)) {
            ARTS_DEBUG("  indirect index detected on partitioned dim; "
                       "disabling blocked partitioning");
            return std::nullopt;
          }
        }
      }

      Value idxAtPos =
          matchedIdx ? matchedIdx : fullChain[memrefStart + matchedPos];
      bool dependsOnLoopInit =
          LoopNode::dependsOnLoopInitNormalized(idxAtPos, offsetStripped) ||
          (normalizedOffset && normalizedOffset != offsetStripped &&
           LoopNode::dependsOnLoopInitNormalized(idxAtPos, normalizedOffset));
      if (!dependsOnLoopInit && !matchesPartitionOffset(idxAtPos)) {
        ARTS_DEBUG("  -> returning none (offset not in access pattern)");
        return std::nullopt;
      }

      if (foundDim && *foundDim != static_cast<unsigned>(matchedPos)) {
        ARTS_DEBUG("  partition offset maps to inconsistent dims; "
                   "disabling blocked partitioning");
        return std::nullopt;
      }
      foundDim = static_cast<unsigned>(matchedPos);
    }
  }

  if (!foundDim) {
    ARTS_DEBUG("  -> returning dim 0 (no dynamic index)");
    return 0u;
  }

  ARTS_DEBUG("  -> returning dim " << *foundDim);
  return foundDim;
}

bool PartitionBoundsAnalyzer::needsFullRange(DbAcquireNode *node,
                                             Value partitionOffset) {
  if (partitionOffset) {
    auto dimOpt = getPartitionOffsetDim(node, partitionOffset,
                                        /*requireLeading=*/false);
    if (dimOpt) {
      DenseMap<DbRefOp, SetVector<Operation *>> dbRefToMemOps;
      MemoryAccessClassifier::collectAccessOperations(node, dbRefToMemOps);
      bool indirectInPartitionedDim = false;
      for (auto &[dbRef, memOps] : dbRefToMemOps) {
        for (Operation *memOp : memOps) {
          SmallVector<Value> fullChain =
              DbUtils::collectFullIndexChain(dbRef, memOp);
          unsigned memrefStart = dbRef.getIndices().size();
          unsigned idxPos = memrefStart + *dimOpt;
          if (idxPos >= fullChain.size()) {
            indirectInPartitionedDim = true;
            break;
          }
          Value idx = fullChain[idxPos];
          int64_t constVal;
          if (!ValueUtils::getConstantIndex(idx, constVal) &&
              arts::isIndirectIndex(idx, partitionOffset)) {
            indirectInPartitionedDim = true;
            break;
          }
        }
        if (indirectInPartitionedDim)
          break;
      }
      if (indirectInPartitionedDim) {
        ARTS_DEBUG("  needsFullRange: indirect access in partitioned dim");
        return true;
      }
    } else {
      if (MemoryAccessClassifier::hasIndirectAccess(node)) {
        ARTS_DEBUG("  needsFullRange: indirect access detected");
        return true;
      }
    }
  } else {
    if (MemoryAccessClassifier::hasIndirectAccess(node)) {
      ARTS_DEBUG("  needsFullRange: indirect access detected");
      return true;
    }
  }

  if (node->getHasNonConstantOffset()) {
    ARTS_DEBUG("  needsFullRange: non-constant offset in partitioned dim");
    return true;
  }

  if (!MemoryAccessClassifier::hasStores(node) &&
      node->getAccessPattern() == AccessPattern::Stencil &&
      partitionOffset) {
    if (!getPartitionOffsetDim(node, partitionOffset, /*requireLeading=*/true)) {
      ARTS_DEBUG(
          "  needsFullRange: stencil access on non-leading partition dim");
      return true;
    }
  }

  if (!partitionOffset || !canPartitionWithOffset(node, partitionOffset)) {
    ARTS_DEBUG("  needsFullRange: "
               << (partitionOffset ? "partition offset not in access pattern"
                                   : "no partition offset (needs full range)"));
    return true;
  }

  return false;
}
