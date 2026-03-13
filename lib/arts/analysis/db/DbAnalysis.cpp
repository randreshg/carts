///==========================================================================///
/// File: DbAnalysis.cpp
/// Implementation of the DbAnalysis class for DB operation analysis.
///==========================================================================///

#include "arts/analysis/db/DbAnalysis.h"
#include "arts/analysis/AccessPatternAnalysis.h"
#include "arts/analysis/AnalysisManager.h"
#include "arts/analysis/db/DbAliasAnalysis.h"
#include "arts/analysis/db/DbPatternMatchers.h"
#include "arts/analysis/graphs/db/DbGraph.h"
#include "arts/analysis/graphs/db/DbNode.h"
#include "arts/analysis/graphs/edt/EdtGraph.h"
#include "arts/analysis/graphs/edt/EdtNode.h"
#include "arts/analysis/loop/LoopAnalysis.h"
#include "arts/utils/DbUtils.h"
#include "arts/utils/EdtUtils.h"
#include "arts/utils/OperationAttributes.h"
#include "arts/utils/Utils.h"
#include "arts/analysis/value/ValueAnalysis.h"
#include "mlir/Analysis/DataFlow/DeadCodeAnalysis.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/Support/Debug.h"

#include "arts/utils/Debug.h"
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

} // namespace

DbAnalysis::DbAnalysis(AnalysisManager &AM) : ArtsAnalysis(AM) {
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

  const DbAcquirePartitionFacts *facts = getAcquirePartitionFacts(acquire);
  if (facts)
    info.hasIndirectAccess = facts->hasIndirectAccess;
  if (facts) {
    info.hasDistributionContract = facts->hasDistributionContract;
    info.partitionDimsFromPeers = facts->partitionDimsFromPeers;
  }

  auto refineSizeFromMinInBlock = [&](Value offset, Value sizeHint) -> Value {
    if (!offset || !sizeHint)
      return Value();
    int64_t sizeConst = 0;
    bool sizeIsConst = ValueAnalysis::getConstantIndex(sizeHint, sizeConst);
    auto isSameConst = [&](Value v) -> bool {
      int64_t val = 0;
      return ValueAnalysis::getConstantIndex(v, val) && val == sizeConst;
    };
    for (Operation &op : *acquire->getBlock()) {
      Value refined;
      auto tryRefine = [&](Value lhs, Value rhs, Value result) {
        bool lhsIsHint = (lhs == sizeHint) || (sizeIsConst && isSameConst(lhs));
        bool rhsIsHint = (rhs == sizeHint) || (sizeIsConst && isSameConst(rhs));
        if (lhsIsHint && ValueAnalysis::dependsOn(rhs, offset))
          refined = result;
        else if (rhsIsHint && ValueAnalysis::dependsOn(lhs, offset))
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
    if (!facts || facts->partitionDims.empty())
      return;
    summary.partitionDims.assign(facts->partitionDims.begin(),
                                 facts->partitionDims.end());
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
        Value repOffset = DbUtils::pickRepresentativePartitionOffset(
            info.partitionOffsets, &offsetIdx);
        Value repSize = DbUtils::pickRepresentativePartitionSize(
            info.partitionSizes, offsetIdx);
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
      Value repOffset = DbUtils::pickRepresentativePartitionOffset(
          info.partitionOffsets, &offsetIdx);
      Value repSize = DbUtils::pickRepresentativePartitionSize(
          info.partitionSizes, offsetIdx);
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
          Value hintSize = DbUtils::pickRepresentativePartitionSize(
              info.partitionSizes, offsetIdx);
          Value hintOff = DbUtils::pickRepresentativePartitionOffset(
              info.partitionOffsets, &offsetIdx);
          bool hintIsConst =
              hintSize && ValueAnalysis::getConstantIndex(hintSize, hintConst);
          bool loopIsConst = ValueAnalysis::getConstantIndex(loopSize, loopConst);

          bool offsetRelated = false;
          if (hintOff && loopOffset) {
            Value hintOffStripped = ValueAnalysis::stripNumericCasts(hintOff);
            Value loopOff = ValueAnalysis::stripNumericCasts(loopOffset);
            if (hintOffStripped == loopOff)
              offsetRelated = true;
            if (!offsetRelated &&
                (ValueAnalysis::dependsOn(loopOff, hintOffStripped) ||
                 ValueAnalysis::dependsOn(hintOffStripped, loopOff)))
              offsetRelated = true;
            if (!offsetRelated) {
              int64_t hintOffConst = 0;
              int64_t loopOffConst = 0;
              if (ValueAnalysis::getConstantIndex(hintOffStripped, hintOffConst) &&
                  ValueAnalysis::getConstantIndex(loopOff, loopOffConst) &&
                  hintOffConst == loopOffConst)
                offsetRelated = true;
            }
          }

          bool loopSizeDependsOnOffset =
              hintOff && ValueAnalysis::dependsOn(loopSize, hintOff);

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

const DbAcquirePartitionFacts *
DbAnalysis::getAcquirePartitionFacts(DbAcquireOp acquire) {
  if (!acquire)
    return nullptr;

  func::FuncOp func = acquire->getParentOfType<func::FuncOp>();
  if (!func)
    return nullptr;

  DbGraph &graph = getOrCreateGraph(func);
  DbAcquireNode *acqNode = graph.getDbAcquireNode(acquire);
  if (!acqNode)
    return nullptr;
  return &acqNode->getPartitionFacts();
}

bool DbAnalysis::hasCrossElementSelfReadInLoop(DbAcquireOp acquire,
                                               ForOp loopOp) {
  if (!acquire || !loopOp)
    return false;

  func::FuncOp func = acquire->getParentOfType<func::FuncOp>();
  if (!func)
    return false;

  DbGraph &dbGraph = getOrCreateGraph(func);
  DbAcquireNode *acquireNode = dbGraph.getDbAcquireNode(acquire);
  if (!acquireNode)
    return false;

  EdtOp edt = acquireNode->getEdtUser();
  if (!edt || loopOp->getParentOfType<EdtOp>() != edt)
    return false;

  EdtGraph &edtGraph = getAnalysisManager().getEdtGraph(func);
  EdtNode *edtNode = edtGraph.getEdtNode(edt);
  if (!edtNode)
    return false;

  LoopAnalysis *loopAnalysis = getLoopAnalysis();
  if (!loopAnalysis)
    return false;
  LoopNode *loopNode = loopAnalysis->getLoopNode(loopOp.getOperation());
  if (!loopNode)
    return false;

  ArrayRef<LoopNode *> associatedLoops = edtNode->getAssociatedLoops();
  if (!associatedLoops.empty() &&
      llvm::find(associatedLoops, loopNode) == associatedLoops.end()) {
    ARTS_DEBUG("Loop not recorded on EDT node; falling back to structural EDT "
               "ancestry for cross-element self-read query");
  }

  DenseMap<DbRefOp, SetVector<Operation *>> dbRefToMemOps;
  acquireNode->collectAccessOperations(dbRefToMemOps);
  if (dbRefToMemOps.empty())
    return false;

  Value edtValue = acquireNode->getUseInEdt();
  Value loopIV = loopNode->getInductionVar();
  if (!edtValue || !loopIV)
    return false;

  SmallVector<AccessIndexInfo, 16> selfReadAccesses;
  for (auto &[dbRef, memOps] : dbRefToMemOps) {
    if (!DbUtils::isSameMemoryObject(dbRef.getSource(), edtValue))
      continue;

    for (Operation *memOp : memOps) {
      if (!memOp || !loopOp.getRegion().isAncestor(memOp->getParentRegion()))
        continue;

      SmallVector<Value> indexChain =
          DbUtils::collectFullIndexChain(dbRef, memOp);
      if (indexChain.empty())
        continue;

      AccessIndexInfo info;
      info.dbRefPrefix = dbRef.getIndices().size();
      info.indexChain.append(indexChain.begin(), indexChain.end());
      selfReadAccesses.push_back(std::move(info));
    }
  }

  if (selfReadAccesses.empty()) {
    ARTS_DEBUG("No self-read accesses found through DbGraph/EdtGraph analysis");
    return false;
  }

  AccessBoundsResult bounds =
      analyzeAccessBoundsFromIndices(selfReadAccesses, loopIV, loopIV);
  auto hasCrossElementOffset = [](const AccessBoundsResult &candidate) {
    return candidate.valid &&
           (candidate.isStencil || candidate.minOffset != 0 ||
            candidate.maxOffset != 0);
  };

  bool hasCrossElementSelfRead = hasCrossElementOffset(bounds);

  if (!hasCrossElementSelfRead) {
    loopOp.walk([&](scf::ForOp nestedLoop) {
      if (hasCrossElementSelfRead || nestedLoop == loopOp)
        return;
      Value nestedIV = nestedLoop.getInductionVar();
      if (!nestedIV)
        return;
      AccessBoundsResult nestedBounds =
          analyzeAccessBoundsFromIndices(selfReadAccesses, nestedIV, nestedIV);
      if (hasCrossElementOffset(nestedBounds)) {
        ARTS_DEBUG("DbAnalysis nested self-read fallback: valid="
                   << nestedBounds.valid << " min=" << nestedBounds.minOffset
                   << " max=" << nestedBounds.maxOffset
                   << " stencil=" << nestedBounds.isStencil
                   << " variable=" << nestedBounds.hasVariableOffset);
        hasCrossElementSelfRead = true;
      }
    });
  }

  ARTS_DEBUG("DbAnalysis cross-element self-read: valid="
             << bounds.valid << " min=" << bounds.minOffset
             << " max=" << bounds.maxOffset << " stencil=" << bounds.isStencil
             << " variable=" << bounds.hasVariableOffset << " -> "
             << hasCrossElementSelfRead);
  return hasCrossElementSelfRead;
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

/// Determine the access pattern for a DB allocation.
///
/// This function combines:
///   1. Runtime analysis via DbGraph acquire pattern summarization
///   2. Static metadata from MemrefAnalyzer (dominantAccessPattern)
///   3. Explicit operation attributes (if already annotated)
///
/// Priority order:
///   - Runtime analysis (most accurate, based on actual IR access patterns)
///   - Static metadata (from preprocessing/profiling)
///   - Existing attributes (from previous passes)
///   - Default to unknown if no information is available
std::optional<DbAccessPattern>
DbAnalysis::getAllocAccessPattern(DbAllocOp alloc) {
  if (!alloc)
    return std::nullopt;

  /// Check for existing operation attribute first (may have been set by
  /// ForOpt pass or other earlier analysis).
  std::optional<DbAccessPattern> attrPattern =
      getDbAccessPattern(alloc.getOperation());

  func::FuncOp func = alloc->getParentOfType<func::FuncOp>();
  if (!func)
    return attrPattern;

  DbGraph &graph = getOrCreateGraph(func);
  DbAllocNode *node = graph.getDbAllocNode(alloc);
  if (!node)
    return attrPattern;

  /// Runtime analysis: check acquire-level access patterns across all
  /// acquires of this allocation. This is the most accurate source as it's
  /// based on actual IR access patterns.
  AcquirePatternSummary summary = node->summarizeAcquirePatterns();
  if (summary.hasStencil)
    return DbAccessPattern::stencil;
  if (summary.hasIndexed)
    return DbAccessPattern::indexed;
  if (summary.hasUniform)
    return DbAccessPattern::uniform;

  /// Static metadata fallback: check MemrefMetadata dominantAccessPattern.
  /// This may be set by MemrefAnalyzer during preprocessing and provides
  /// valuable hints when runtime analysis is inconclusive.
  if (node->dominantAccessPattern) {
    AccessPatternType metaPattern = *node->dominantAccessPattern;

    /// Map MemrefMetadata AccessPatternType to DbAccessPattern.
    /// Note the semantic differences:
    ///   - AccessPatternType::Stencil → DbAccessPattern::stencil
    ///     (multiple offset accesses like A[i-1], A[i], A[i+1])
    ///   - AccessPatternType::Sequential/Strided → DbAccessPattern::uniform
    ///     (predictable, partitionable patterns)
    ///   - AccessPatternType::GatherScatter/Random → DbAccessPattern::indexed
    ///     (indirect/irregular access patterns)
    if (metaPattern == AccessPatternType::Stencil)
      return DbAccessPattern::stencil;
    if (metaPattern == AccessPatternType::Sequential ||
        metaPattern == AccessPatternType::Strided)
      return DbAccessPattern::uniform;
    if (metaPattern == AccessPatternType::GatherScatter ||
        metaPattern == AccessPatternType::Random)
      return DbAccessPattern::indexed;
  }

  /// Fallback to existing attribute if metadata didn't provide a pattern.
  if (attrPattern)
    return attrPattern;

  return DbAccessPattern::unknown;
}

DbAllocNode *DbAnalysis::getDbAllocNode(DbAllocOp alloc) {
  auto func = alloc->getParentOfType<func::FuncOp>();
  if (!func)
    return nullptr;
  return getOrCreateGraph(func).getDbAllocNode(alloc);
}

DbAcquireNode *DbAnalysis::getDbAcquireNode(DbAcquireOp acquire) {
  auto func = acquire->getParentOfType<func::FuncOp>();
  if (!func)
    return nullptr;
  return getOrCreateGraph(func).getDbAcquireNode(acquire);
}
