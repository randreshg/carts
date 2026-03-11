///==========================================================================///
/// File: DbAnalysis.cpp
/// Implementation of the DbAnalysis class for DB operation analysis.
///==========================================================================///

#include "arts/Analysis/Db/DbAnalysis.h"
#include "arts/Analysis/AccessPatternAnalysis.h"
#include "arts/Analysis/ArtsAnalysisManager.h"
#include "arts/Analysis/Db/DbAliasAnalysis.h"
#include "arts/Analysis/Db/DbPatternMatchers.h"
#include "arts/Analysis/Graphs/Db/DbGraph.h"
#include "arts/Analysis/Graphs/Db/DbNode.h"
#include "arts/Analysis/Loop/LoopAnalysis.h"
#include "arts/Utils/ArtsUtils.h"
#include "arts/Utils/DbUtils.h"
#include "arts/Utils/EdtUtils.h"
#include "arts/Utils/OperationAttributes.h"
#include "arts/Utils/ValueUtils.h"
#include "mlir/Analysis/DataFlow/DeadCodeAnalysis.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/Support/Debug.h"

#include "arts/Utils/ArtsDebug.h"
ARTS_DEBUG_SETUP(db_analysis);

using namespace mlir;
using namespace mlir::arts;

namespace {

static EdtDistributionPattern
classifyDistributionPattern(const DbAnalysis::LoopDbAccessSummary &summary) {
  if (summary.hasTriangularBound)
    return EdtDistributionPattern::triangular;
  if (summary.hasStencilAccessHint || summary.hasStencilOffset)
    return EdtDistributionPattern::stencil;
  if (summary.hasMatmulUpdate)
    return EdtDistributionPattern::matmul;
  return EdtDistributionPattern::uniform;
}

static bool isTiling2DTaskAcquire(DbAcquireOp acquire) {
  if (!acquire)
    return false;
  auto [edt, blockArg] = EdtUtils::getEdtBlockArgumentForAcquire(acquire);
  if (!edt)
    return false;
  auto kind = getEdtDistributionKind(edt.getOperation());
  return kind && *kind == EdtDistributionKind::tiling_2d;
}

static Value pickRepresentativePartitionOffset(ArrayRef<Value> offsets,
                                               unsigned *outIdx = nullptr) {
  if (outIdx)
    *outIdx = 0;
  for (unsigned i = 0; i < offsets.size(); ++i) {
    Value off = offsets[i];
    if (!off)
      continue;
    int64_t c = 0;
    if (!ValueUtils::getConstantIndex(ValueUtils::stripNumericCasts(off), c)) {
      if (outIdx)
        *outIdx = i;
      return off;
    }
  }
  for (unsigned i = 0; i < offsets.size(); ++i) {
    if (offsets[i]) {
      if (outIdx)
        *outIdx = i;
      return offsets[i];
    }
  }
  return Value();
}

static Value pickRepresentativePartitionSize(ArrayRef<Value> sizes,
                                             unsigned idx) {
  if (sizes.empty())
    return Value();
  if (idx < sizes.size() && sizes[idx])
    return sizes[idx];
  return sizes.front();
}

} // namespace

DbAnalysis::DbAnalysis(ArtsAnalysisManager &AM) : ArtsAnalysis(AM) {
  ARTS_DEBUG("Initializing DbAnalysis");
  dbAliasAnalysis = std::make_unique<DbAliasAnalysis>(this);
}

DbAnalysis::~DbAnalysis() { ARTS_DEBUG("Destroying DbAnalysis"); }

DbGraph &DbAnalysis::getOrCreateGraph(func::FuncOp func) {
  ARTS_INFO("Getting or creating DbGraph for function: " << func.getName());
  auto it = functionGraphMap.find(func);
  if (it != functionGraphMap.end())
    return *it->second.get();

  ARTS_DEBUG(" - Creating new DbGraph for function: " << func.getName());
  auto newGraph = std::make_unique<DbGraph>(func, this);
  newGraph->build();

  DbGraph *graphPtr = newGraph.get();
  functionGraphMap[func] = std::move(newGraph);
  return *graphPtr;
}

bool DbAnalysis::invalidateGraph(func::FuncOp func) {
  ARTS_INFO("Invalidating DbGraph for function: " << func.getName());
  auto it = functionGraphMap.find(func);
  if (it != functionGraphMap.end()) {
    functionGraphMap.erase(it);
    loopAccessSummaryByOp.clear();
    if (dbAliasAnalysis)
      dbAliasAnalysis->resetCache();
    return true;
  }
  return false;
}

void DbAnalysis::invalidate() {
  ARTS_INFO("Invalidating all DbGraphs");
  functionGraphMap.clear();
  loopAccessSummaryByOp.clear();
  if (dbAliasAnalysis)
    dbAliasAnalysis->resetCache();
}

void DbAnalysis::print(func::FuncOp func) {
  ARTS_INFO("Printing DbGraph for function: " << func.getName());
  DbGraph &graph = getOrCreateGraph(func);
  graph.print(ARTS_DBGS());
}

LoopAnalysis *DbAnalysis::getLoopAnalysis() {
  return &getAnalysisManager().getLoopAnalysis();
}

DbAnalysis::LoopDbAccessSummary
DbAnalysis::analyzeLoopDbAccessPatterns(ForOp forOp) {
  LoopDbAccessSummary summary;
  if (!forOp) {
    summary.distributionPattern = EdtDistributionPattern::unknown;
    return summary;
  }

  if (auto it = loopAccessSummaryByOp.find(forOp.getOperation());
      it != loopAccessSummaryByOp.end())
    return it->second;

  Block &forBody = forOp.getRegion().front();
  if (forBody.getNumArguments() == 0) {
    summary.distributionPattern = EdtDistributionPattern::unknown;
    loopAccessSummaryByOp[forOp.getOperation()] = summary;
    return summary;
  }

  Value loopIV = forBody.getArgument(0);
  summary.hasTriangularBound = hasTriangularBoundPattern(forOp);
  summary.hasMatmulUpdate = detectMatmulUpdatePattern(forOp, getLoopAnalysis());

  Region &forRegion = forOp.getRegion();
  EdtOp edt = forOp->getParentOfType<EdtOp>();
  func::FuncOp func = forOp->getParentOfType<func::FuncOp>();
  if (!edt || !func) {
    summary.distributionPattern = classifyDistributionPattern(summary);
    loopAccessSummaryByOp[forOp.getOperation()] = summary;
    return summary;
  }

  DbGraph &graph = getOrCreateGraph(func);
  graph.forEachAcquireNode([&](DbAcquireNode *acqNode) {
    if (!acqNode || acqNode->getEdtUser() != edt)
      return;

    DbAllocNode *rootAlloc = acqNode->getRootAlloc();
    if (!rootAlloc || !rootAlloc->getDbAllocOp())
      return;
    Operation *allocOp = rootAlloc->getDbAllocOp().getOperation();

    DenseMap<DbRefOp, SetVector<Operation *>> dbRefToMemOps;
    acqNode->collectAccessOperations(dbRefToMemOps);

    SmallVector<AccessIndexInfo, 16> accesses;
    bool hasLoopAccess = false;
    for (auto &[dbRef, memOps] : dbRefToMemOps) {
      for (Operation *memOp : memOps) {
        Region *memRegion = memOp ? memOp->getParentRegion() : nullptr;
        if (!memRegion)
          continue;
        if (memRegion != &forRegion && !forRegion.isAncestor(memRegion))
          continue;
        hasLoopAccess = true;

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

    if (!hasLoopAccess)
      return;

    DbAccessPattern combinedPattern = DbAccessPattern::unknown;

    if (!accesses.empty()) {
      AccessBoundsResult bounds =
          analyzeAccessBoundsFromIndices(accesses, loopIV, loopIV);
      if (bounds.valid) {
        if (bounds.isStencil || bounds.minOffset != 0 ||
            bounds.maxOffset != 0) {
          combinedPattern =
              mergeDbAccessPattern(combinedPattern, DbAccessPattern::stencil);
          summary.hasStencilOffset = true;
        } else {
          combinedPattern =
              mergeDbAccessPattern(combinedPattern, DbAccessPattern::uniform);
        }
      } else if (bounds.hasVariableOffset) {
        combinedPattern =
            mergeDbAccessPattern(combinedPattern, DbAccessPattern::indexed);
      }
    }

    auto it = summary.allocPatterns.find(allocOp);
    if (it == summary.allocPatterns.end()) {
      summary.allocPatterns[allocOp] = combinedPattern;
    } else {
      it->second = mergeDbAccessPattern(it->second, combinedPattern);
    }
  });

  summary.distributionPattern = classifyDistributionPattern(summary);
  loopAccessSummaryByOp[forOp.getOperation()] = summary;
  return summary;
}

std::optional<DbAnalysis::LoopDbAccessSummary>
DbAnalysis::getLoopDbAccessSummary(ForOp forOp) {
  if (!forOp)
    return std::nullopt;
  if (auto it = loopAccessSummaryByOp.find(forOp.getOperation());
      it != loopAccessSummaryByOp.end())
    return it->second;
  return analyzeLoopDbAccessPatterns(forOp);
}

std::optional<EdtDistributionPattern>
DbAnalysis::getLoopDistributionPattern(ForOp forOp) {
  if (!forOp)
    return std::nullopt;
  return analyzeLoopDbAccessPatterns(forOp).distributionPattern;
}

bool DbAnalysis::hasNonInternodeConsumerForWrittenDb(EdtOp producerEdt) {
  if (!producerEdt)
    return false;

  func::FuncOp parentFunc = producerEdt->getParentOfType<func::FuncOp>();
  if (!parentFunc)
    return false;

  DbGraph &graph = getOrCreateGraph(parentFunc);
  llvm::DenseSet<DbAllocNode *> writtenDbAllocs;

  graph.forEachAcquireNode([&](DbAcquireNode *acquireNode) {
    if (!acquireNode)
      return;

    if (acquireNode->getEdtUser() != producerEdt)
      return;
    if (!acquireNode->isWriterAccess())
      return;

    if (DbAllocNode *root = acquireNode->getRootAlloc())
      writtenDbAllocs.insert(root);
  });

  if (writtenDbAllocs.empty())
    return false;

  bool hasNonInternodeConsumer = false;
  graph.forEachAcquireNode([&](DbAcquireNode *acquireNode) {
    if (hasNonInternodeConsumer || !acquireNode)
      return;

    DbAllocNode *root = acquireNode->getRootAlloc();
    if (!root || !writtenDbAllocs.contains(root))
      return;

    EdtOp consumerEdt = acquireNode->getEdtUser();
    if (!consumerEdt) {
      hasNonInternodeConsumer = true;
      return;
    }

    if (consumerEdt == producerEdt)
      return;

    if (consumerEdt.getConcurrency() != EdtConcurrency::internode)
      hasNonInternodeConsumer = true;
  });

  return hasNonInternodeConsumer;
}

DbAnalysis::AcquirePartitionSummary
DbAnalysis::analyzeAcquirePartition(DbAcquireOp acquire, OpBuilder &builder) {
  AcquirePartitionSummary info;
  if (!acquire)
    return info;

  if (auto modeAttr = getPartitionMode(acquire.getOperation()))
    info.mode = *modeAttr;
  else
    info.mode = DbUtils::getPartitionModeFromStructure(acquire);
  if (info.mode == PartitionMode::coarse) {
    if (!acquire.getPartitionIndices().empty())
      info.mode = PartitionMode::fine_grained;
    else if (!acquire.getPartitionOffsets().empty() ||
             !acquire.getPartitionSizes().empty())
      info.mode = PartitionMode::block;
  }

  DbAcquireNode *acqNode = nullptr;
  func::FuncOp func = acquire->getParentOfType<func::FuncOp>();
  if (func) {
    DbGraph &graph = getOrCreateGraph(func);
    acqNode = graph.getDbAcquireNode(acquire);
  }

  if (acqNode)
    info.hasIndirectAccess = acqNode->hasIndirectAccess();

  auto refineSizeFromMinInBlock = [&](Value offset, Value sizeHint) -> Value {
    if (!offset || !sizeHint)
      return Value();
    int64_t sizeConst = 0;
    bool sizeIsConst = ValueUtils::getConstantIndex(sizeHint, sizeConst);
    auto isSameConst = [&](Value v) -> bool {
      int64_t val = 0;
      return ValueUtils::getConstantIndex(v, val) && val == sizeConst;
    };
    for (Operation &op : *acquire->getBlock()) {
      Value refined;
      auto tryRefine = [&](Value lhs, Value rhs, Value result) {
        bool lhsIsHint = (lhs == sizeHint) || (sizeIsConst && isSameConst(lhs));
        bool rhsIsHint = (rhs == sizeHint) || (sizeIsConst && isSameConst(rhs));
        if (lhsIsHint && ValueUtils::dependsOn(rhs, offset))
          refined = result;
        else if (rhsIsHint && ValueUtils::dependsOn(lhs, offset))
          refined = result;
      };

      if (auto minOp = dyn_cast<arith::MinUIOp>(&op)) {
        tryRefine(minOp.getLhs(), minOp.getRhs(), minOp.getResult());
      } else if (auto minOp = dyn_cast<arith::MinSIOp>(&op)) {
        tryRefine(minOp.getLhs(), minOp.getRhs(), minOp.getResult());
      }

      if (refined)
        return refined;
    }
    return Value();
  };

  auto inferPartitionDims = [&](AcquirePartitionSummary &summary) {
    if (!acqNode || summary.partitionOffsets.empty())
      return;
    summary.partitionDims.clear();
    SmallVector<unsigned, 4> seen;
    for (Value off : summary.partitionOffsets) {
      if (!off) {
        summary.partitionDims.clear();
        return;
      }
      auto dimOpt =
          acqNode->getPartitionOffsetDim(off, /*requireLeading=*/false);
      if (!dimOpt) {
        summary.partitionDims.clear();
        return;
      }
      bool alreadySeen = false;
      for (unsigned seenDim : seen) {
        if (seenDim == *dimOpt) {
          alreadySeen = true;
          break;
        }
      }
      if (alreadySeen) {
        summary.partitionDims.clear();
        return;
      }
      seen.push_back(*dimOpt);
      summary.partitionDims.push_back(*dimOpt);
    }
  };

  auto applyTiling2DPartitionDimsFallback =
      [&](AcquirePartitionSummary &summary) {
        if (!isTiling2DTaskAcquire(acquire))
          return;
        if (!summary.partitionDims.empty())
          return;
        if (summary.partitionOffsets.size() < 2)
          return;
        summary.partitionDims.clear();
        for (unsigned d = 0; d < summary.partitionOffsets.size(); ++d)
          summary.partitionDims.push_back(d);
      };

  switch (info.mode) {
  case PartitionMode::coarse: {
    info.isValid = true;
    if (acqNode) {
      Value offset, size;
      if (succeeded(acqNode->computeBlockInfo(offset, size))) {
        info.partitionOffsets.push_back(offset);
        info.partitionSizes.push_back(size);
        unsigned offsetIdx = 0;
        Value repOffset = pickRepresentativePartitionOffset(
            info.partitionOffsets, &offsetIdx);
        Value repSize =
            pickRepresentativePartitionSize(info.partitionSizes, offsetIdx);
        if (Value refined = refineSizeFromMinInBlock(repOffset, repSize)) {
          if (offsetIdx < info.partitionSizes.size())
            info.partitionSizes[offsetIdx] = refined;
          else if (!info.partitionSizes.empty())
            info.partitionSizes[0] = refined;
        }
        inferPartitionDims(info);
      } else {
        info.isValid = false;
      }
    } else {
      info.isValid = false;
    }
    break;
  }

  case PartitionMode::fine_grained: {
    if (!acquire.getPartitionIndices().empty()) {
      for (Value idx : acquire.getPartitionIndices())
        info.partitionOffsets.push_back(idx);
      Value one = arts::createOneIndex(builder, acquire.getLoc());
      for (unsigned i = 0; i < info.partitionOffsets.size(); ++i)
        info.partitionSizes.push_back(one);
      info.isValid = true;
      inferPartitionDims(info);
    }
    break;
  }

  case PartitionMode::block:
  case PartitionMode::stencil: {
    /// Prefer explicit partition_* hints when available. If they were already
    /// materialized into offsets/sizes (post-rewrite acquires), reuse those so
    /// we do not regress valid block acquires back to coarse.
    ValueRange effectiveOffsets = acquire.getPartitionOffsets();
    ValueRange effectiveSizes = acquire.getPartitionSizes();
    if (effectiveOffsets.empty()) {
      effectiveOffsets = acquire.getOffsets();
      effectiveSizes = acquire.getSizes();
    }

    if (!effectiveOffsets.empty()) {
      for (Value off : effectiveOffsets)
        info.partitionOffsets.push_back(off);
      Value one = arts::createOneIndex(builder, acquire.getLoc());
      for (unsigned i = 0; i < info.partitionOffsets.size(); ++i)
        info.partitionSizes.push_back(
            i < effectiveSizes.size() ? effectiveSizes[i] : one);
      info.isValid = true;

      unsigned offsetIdx = 0;
      Value repOffset =
          pickRepresentativePartitionOffset(info.partitionOffsets, &offsetIdx);
      Value repSize =
          pickRepresentativePartitionSize(info.partitionSizes, offsetIdx);
      if (Value refined = refineSizeFromMinInBlock(repOffset, repSize)) {
        if (offsetIdx < info.partitionSizes.size())
          info.partitionSizes[offsetIdx] = refined;
        else if (!info.partitionSizes.empty())
          info.partitionSizes[0] = refined;
      }

      if (acqNode) {
        Value loopOffset, loopSize;
        if (succeeded(acqNode->computeBlockInfo(loopOffset, loopSize))) {
          bool useLoopSize = false;
          int64_t hintConst = 0;
          int64_t loopConst = 0;
          Value hintSize =
              pickRepresentativePartitionSize(info.partitionSizes, offsetIdx);
          Value hintOff = pickRepresentativePartitionOffset(
              info.partitionOffsets, &offsetIdx);
          bool hintIsConst =
              hintSize && ValueUtils::getConstantIndex(hintSize, hintConst);
          bool loopIsConst = ValueUtils::getConstantIndex(loopSize, loopConst);

          bool offsetRelated = false;
          if (hintOff && loopOffset) {
            Value hintOffStripped = ValueUtils::stripNumericCasts(hintOff);
            Value loopOff = ValueUtils::stripNumericCasts(loopOffset);
            if (hintOffStripped == loopOff)
              offsetRelated = true;
            if (!offsetRelated &&
                (ValueUtils::dependsOn(loopOff, hintOffStripped) ||
                 ValueUtils::dependsOn(hintOffStripped, loopOff)))
              offsetRelated = true;
            if (!offsetRelated) {
              int64_t hintOffConst = 0;
              int64_t loopOffConst = 0;
              if (ValueUtils::getConstantIndex(hintOffStripped, hintOffConst) &&
                  ValueUtils::getConstantIndex(loopOff, loopOffConst) &&
                  hintOffConst == loopOffConst)
                offsetRelated = true;
            }
          }

          bool loopSizeDependsOnOffset =
              hintOff && ValueUtils::dependsOn(loopSize, hintOff);

          if (loopSizeDependsOnOffset)
            useLoopSize = true;
          if (offsetRelated && !loopIsConst)
            useLoopSize = true;
          if (offsetRelated && hintIsConst && loopIsConst &&
              hintConst != loopConst)
            useLoopSize = true;

          if (useLoopSize) {
            if (offsetIdx < info.partitionSizes.size())
              info.partitionSizes[offsetIdx] = loopSize;
            else if (!info.partitionSizes.empty())
              info.partitionSizes[0] = loopSize;
            else
              info.partitionSizes.push_back(loopSize);
          }
        }
      }
      inferPartitionDims(info);
      applyTiling2DPartitionDimsFallback(info);
      break;
    }

    if (acqNode) {
      Value offset, size;
      if (succeeded(acqNode->computeBlockInfo(offset, size))) {
        info.partitionOffsets.push_back(offset);
        info.partitionSizes.push_back(size);
        info.isValid = true;
        inferPartitionDims(info);
        applyTiling2DPartitionDimsFallback(info);
      } else {
        info.mode = PartitionMode::coarse;
      }
    }
    break;
  }
  }

  return info;
}

std::optional<AccessPattern>
DbAnalysis::getAcquireAccessPattern(DbAcquireOp acquire) {
  if (!acquire)
    return std::nullopt;

  func::FuncOp func = acquire->getParentOfType<func::FuncOp>();
  if (!func)
    return std::nullopt;

  DbGraph &graph = getOrCreateGraph(func);
  DbAcquireNode *node = graph.getDbAcquireNode(acquire);
  if (!node)
    return std::nullopt;
  return node->getAccessPattern();
}

std::optional<DbAccessPattern>
DbAnalysis::getAllocAccessPattern(DbAllocOp alloc) {
  if (!alloc)
    return std::nullopt;

  std::optional<DbAccessPattern> attrPattern =
      getDbAccessPattern(alloc.getOperation());

  func::FuncOp func = alloc->getParentOfType<func::FuncOp>();
  if (!func)
    return attrPattern;

  DbGraph &graph = getOrCreateGraph(func);
  DbAllocNode *node = graph.getDbAllocNode(alloc);
  if (!node)
    return attrPattern;

  AcquirePatternSummary summary = node->summarizeAcquirePatterns();
  if (summary.hasStencil)
    return DbAccessPattern::stencil;
  if (summary.hasIndexed)
    return DbAccessPattern::indexed;
  if (summary.hasUniform)
    return DbAccessPattern::uniform;
  if (attrPattern)
    return attrPattern;
  return DbAccessPattern::unknown;
}
