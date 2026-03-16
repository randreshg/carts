///==========================================================================///
/// DbPartitioning.cpp - Datablock partitioning controller pass
///
/// Controller responsibilities:
///   Phase 1: query graph-backed facts from DbAnalysis / DbNode
///   Phase 2: assemble heuristic snapshots and obtain H1 decisions
///   Phase 3: reconcile/normalize those decisions for one allocation
///   Phase 4: build planner inputs and invoke the rewriter
///
/// Non-responsibilities:
///   - does not own canonical per-acquire facts (DbGraph/DbNode do)
///   - does not choose policy directly (DbHeuristics does)
///   - does not localize indices directly (rewriters/indexers do)
///
/// Transformation (shape-focused):
///   BEFORE:
///     %acq = arts.db_acquire ... partition_mode = coarse
///     (single full-range acquire)
///
///   AFTER (example block mode):
///     %acq = arts.db_acquire ... partition_mode = block
///       partition_offsets(...), partition_sizes(...)
///     (task-local range acquire; same program dependencies, smaller transfer)
///
/// Example:
///   coarse single-range acquire -> block/stencil/element-wise layout rewrite
///
/// This pass must preserve dependency semantics: it refines acquire metadata
/// and rewrite plans, but does not weaken EDT/event ordering.
///==========================================================================///

/// Dialects
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/BuiltinAttributes.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/Value.h"
#include "mlir/Support/LLVM.h"
/// Arts
#include "../../PassDetails.h"
#include "arts/Dialect.h"
#include "arts/analysis/AnalysisManager.h"
#include "arts/analysis/db/DbAnalysis.h"
#include "arts/analysis/graphs/db/DbGraph.h"
#include "arts/analysis/heuristics/DbHeuristics.h"
#include "arts/analysis/loop/LoopAnalysis.h"
#include "arts/analysis/loop/LoopNode.h"
#include "arts/analysis/value/ValueAnalysis.h"
#include "arts/passes/Passes.h"
/// Other
#include "mlir/IR/OpDefinition.h"
#include "mlir/IR/Operation.h"
#include "mlir/IR/Visitors.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Support/LogicalResult.h"
/// Debug
#include "arts/analysis/graphs/db/DbNode.h"
#include "arts/analysis/graphs/edt/EdtGraph.h"
#include "arts/analysis/graphs/edt/EdtNode.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/ADT/SmallVector.h"
#include <algorithm>
#include <cstdint>
#include <optional>

#include "arts/transforms/db/DbBlockPlanResolver.h"
#include "arts/transforms/db/DbPartitionPlanner.h"
#include "arts/transforms/db/DbPartitionTypes.h"
#include "arts/transforms/db/DbRewriter.h"
#include "arts/utils/DbUtils.h"
#include "arts/utils/Debug.h"
#include "arts/utils/EdtUtils.h"
#include "arts/utils/LoweringContractUtils.h"
#include "arts/utils/OperationAttributes.h"
#include "arts/utils/StencilAttributes.h"
#include "arts/utils/Utils.h"
ARTS_DEBUG_SETUP(db_partitioning);

using namespace mlir;
using namespace mlir::func;
using namespace mlir::arith;
using namespace mlir::polygeist;
using namespace mlir::arts;

namespace {

static bool
hasExplicitStencilContract(const DbAnalysis::AcquireContractSummary *summary) {
  return summary && summary->contract.hasExplicitStencilContract();
}

/// Validate that element-wise partition indices match actual EDT accesses.
/// Returns false if this is a block-wise pattern (indices are block corners,
/// but accesses span a range) - indicating we should fall back to block mode.
///
/// Detection criteria for block-wise pattern:
/// 1. Partition hints have indices[] (looks like element-wise)
/// 2. BUT the enclosing loop steps by > 1 (BLOCK_SIZE)
/// 3. OR partition indices don't appear in EDT body accesses
static bool
validateElementWiseIndices(ArrayRef<AcquirePartitionInfo> acquireInfos,
                           ArrayRef<DbAcquireNode *> allocAcquireNodes) {
  DenseMap<DbAcquireOp, DbAcquireNode *> acquireNodeMap;
  acquireNodeMap.reserve(allocAcquireNodes.size());
  for (DbAcquireNode *acqNode : allocAcquireNodes) {
    if (!acqNode)
      continue;
    DbAcquireOp acquire = acqNode->getDbAcquireOp();
    if (acquire)
      acquireNodeMap[acquire] = acqNode;
  }

  for (const auto &info : acquireInfos) {
    DbAcquireOp acquire = info.acquire;
    auto partitionIndices = acquire.getPartitionIndices();
    if (partitionIndices.empty())
      continue;

    auto it = acquireNodeMap.find(acquire);
    if (it != acquireNodeMap.end() &&
        !it->second->validateElementWisePartitioning())
      return false;
  }
  return true; /// Valid element-wise
}

/// Extract all partition offsets for N-D support.
/// Unified function that handles both single and multi-dimensional cases.
/// Use front() on the result for 1-D compatibility.
static SmallVector<Value>
getPartitionOffsetsND(DbAcquireNode *acqNode,
                      const AcquirePartitionInfo *info) {
  if (info && !info->partitionOffsets.empty())
    return info->partitionOffsets;

  SmallVector<Value> result;
  DbAcquireOp acqOp = acqNode->getDbAcquireOp();
  auto mode = acqOp.getPartitionMode();
  bool preferPartitionIndices = !mode || *mode == PartitionMode::fine_grained;

  /// Fine-grained uses partition indices; block/stencil use offsets.
  auto indices = acqOp.getPartitionIndices();
  auto offsets = acqOp.getPartitionOffsets();
  if (preferPartitionIndices && !indices.empty()) {
    result.append(indices.begin(), indices.end());
    return result;
  }
  if (!offsets.empty()) {
    result.append(offsets.begin(), offsets.end());
    return result;
  }
  if (!indices.empty()) {
    result.append(indices.begin(), indices.end());
    return result;
  }

  /// Finally, try getPartitionInfo for single-dim case
  auto [offset, size] = acqNode->getPartitionInfo();
  if (offset)
    result.push_back(offset);

  return result;
}

static bool hasInternodeAcquireUser(ArrayRef<DbAcquireNode *> acquireNodes) {
  for (DbAcquireNode *acqNode : acquireNodes) {
    if (!acqNode)
      continue;
    EdtOp edt = acqNode->getEdtUser();
    if (edt && edt.getConcurrency() == EdtConcurrency::internode)
      return true;
  }
  return false;
}

static AcquirePatternSummary
summarizeAcquirePatterns(ArrayRef<DbAcquireNode *> acquireNodes,
                         DbAllocNode *allocNode, DbAnalysis *dbAnalysis) {
  AcquirePatternSummary summary;
  bool sawSummary = false;

  for (DbAcquireNode *acqNode : acquireNodes) {
    if (!acqNode)
      continue;
    DbAcquireOp acquire = acqNode->getDbAcquireOp();
    if (!acquire || !dbAnalysis)
      continue;
    auto contractSummary = dbAnalysis->getAcquireContractSummary(acquire);
    if (!contractSummary)
      continue;
    sawSummary = true;

    switch (contractSummary->accessPattern) {
    case AccessPattern::Uniform:
      summary.hasUniform = true;
      break;
    case AccessPattern::Stencil:
      summary.hasStencil = true;
      break;
    case AccessPattern::Indexed:
      summary.hasIndexed = true;
      break;
    case AccessPattern::Unknown:
      break;
    }

    if (contractSummary->hasIndirectAccess())
      summary.hasIndexed = true;
    if (hasExplicitStencilContract(&*contractSummary))
      summary.hasStencil = true;
    if (contractSummary->usesMatmulSemantics())
      summary.hasUniform = true;
  }

  if (sawSummary)
    return summary;
  return allocNode ? allocNode->summarizeAcquirePatterns()
                   : AcquirePatternSummary{};
}

static bool isAcquireInfoConsistent(const AcquirePartitionInfo &lhs,
                                    const AcquirePartitionInfo &rhs) {
  if (lhs.mode != rhs.mode)
    return false;

  if (lhs.mode != PartitionMode::block || lhs.partitionSizes.empty() ||
      rhs.partitionSizes.empty()) {
    return true;
  }

  Value lhsSize = lhs.partitionSizes.front();
  Value rhsSize = rhs.partitionSizes.front();
  if (lhsSize == rhsSize)
    return true;

  int64_t lhsConst = 0;
  int64_t rhsConst = 0;
  bool lhsIsConst = ValueAnalysis::getConstantIndex(lhsSize, lhsConst);
  bool rhsIsConst = ValueAnalysis::getConstantIndex(rhsSize, rhsConst);
  return lhsIsConst && rhsIsConst && lhsConst == rhsConst;
}

struct AcquireModeReconcileResult {
  bool modesConsistent = true;
  bool allBlockFullRange = false;
};

static AcquireModeReconcileResult
reconcileAcquireModes(PartitioningContext &ctx,
                      SmallVectorImpl<AcquirePartitionInfo> &acquireInfos,
                      bool canUseBlock) {
  AcquireModeReconcileResult result;
  if (acquireInfos.empty())
    return result;

  for (size_t i = 1; i < acquireInfos.size(); ++i) {
    if (!isAcquireInfoConsistent(acquireInfos.front(), acquireInfos[i])) {
      result.modesConsistent = false;
      break;
    }
  }

  bool anyBlockCapable = false;
  bool anyBlockNotFullRange = false;
  size_t count = std::min(ctx.acquires.size(), acquireInfos.size());
  for (size_t i = 0; i < count; ++i) {
    if (!ctx.acquires[i].canBlock)
      continue;
    anyBlockCapable = true;
    if (!acquireInfos[i].needsFullRange)
      anyBlockNotFullRange = true;
  }

  result.allBlockFullRange = anyBlockCapable && !anyBlockNotFullRange;

  ctx.canElementWise = ctx.anyCanElementWise();
  ctx.canBlock = canUseBlock && ctx.anyCanBlock();
  ctx.allBlockFullRange = result.allBlockFullRange;
  if (!ctx.canBlock)
    return result;

  bool hasCoarse =
      llvm::any_of(acquireInfos, [](const AcquirePartitionInfo &i) {
        return i.mode == PartitionMode::coarse;
      });
  bool hasBlock = llvm::any_of(acquireInfos, [](const AcquirePartitionInfo &i) {
    return requiresBlockSize(i.mode);
  });

  if (hasCoarse && hasBlock) {
    ctx.canElementWise = false;
    for (auto &info : acquireInfos) {
      if (info.mode == PartitionMode::coarse)
        info.needsFullRange = true;
    }
  } else if (hasCoarse) {
    ctx.canElementWise = false;
  }

  return result;
}

static bool isLowerBoundGuaranteedByControlFlow(Operation *op, Value loopIV) {
  if (!loopIV)
    return false;

  auto matchesIV = [&loopIV](Value v) {
    if (v == loopIV)
      return true;
    if (auto cast = v.getDefiningOp<arith::IndexCastOp>())
      return cast.getIn() == loopIV;
    return false;
  };

  for (Operation *parent = op->getParentOp(); parent;
       parent = parent->getParentOp()) {
    auto ifOp = dyn_cast<scf::IfOp>(parent);
    if (!ifOp || ifOp.getElseRegion().empty())
      continue;
    if (!ifOp.getElseRegion().isAncestor(op->getParentRegion()))
      continue;

    auto cmp = ifOp.getCondition().getDefiningOp<arith::CmpIOp>();
    if (!cmp || cmp.getPredicate() != arith::CmpIPredicate::eq)
      continue;

    Value lhs = cmp.getLhs();
    Value rhs = cmp.getRhs();
    if ((matchesIV(lhs) && ValueAnalysis::isZeroConstant(rhs)) ||
        (matchesIV(rhs) && ValueAnalysis::isZeroConstant(lhs)))
      return true;
  }
  return false;
}

static void applyBoundsValid(DbAcquireOp acquireOp,
                             ArrayRef<int64_t> boundsCheckFlags, Value loopIV) {
  if (acquireOp.hasMultiplePartitionEntries()) {
    ARTS_DEBUG("  Skipping bounds generation for multi-entry stencil acquire");
    return;
  }

  Location loc = acquireOp.getLoc();
  OpBuilder builder(acquireOp);
  auto indices = acquireOp.getIndices();
  SmallVector<Value> sourceSizes =
      DbUtils::getSizesFromDb(acquireOp.getSourcePtr());
  bool lowerBoundGuarded =
      isLowerBoundGuaranteedByControlFlow(acquireOp, loopIV);

  Value zero = arts::createZeroIndex(builder, loc);
  Value boundsValid;
  SmallVector<Value> indicesVec(indices.begin(), indices.end());
  for (size_t i = 0; i < boundsCheckFlags.size() && i < indices.size(); ++i) {
    if (boundsCheckFlags[i] == 0 || i >= sourceSizes.size())
      continue;

    Value idx = indices[i];
    Value size = sourceSizes[i];
    Value geZero = builder.create<arith::CmpIOp>(loc, arith::CmpIPredicate::sge,
                                                 idx, zero);
    Value dimValid;
    if (lowerBoundGuarded) {
      dimValid = builder.create<arith::CmpIOp>(loc, arith::CmpIPredicate::slt,
                                               idx, size);
    } else {
      Value ltSize = builder.create<arith::CmpIOp>(
          loc, arith::CmpIPredicate::slt, idx, size);
      dimValid = builder.create<arith::AndIOp>(loc, geZero, ltSize);
    }
    boundsValid =
        boundsValid ? builder.create<arith::AndIOp>(loc, boundsValid, dimValid)
                    : dimValid;

    Value nonNegative = builder.create<arith::SelectOp>(loc, geZero, idx, zero);
    Value one = arts::createOneIndex(builder, loc);
    Value hasExtent = builder.create<arith::CmpIOp>(
        loc, arith::CmpIPredicate::sgt, size, zero);
    Value lastIdxRaw = builder.create<arith::SubIOp>(loc, size, one);
    Value lastIdx =
        builder.create<arith::SelectOp>(loc, hasExtent, lastIdxRaw, zero);
    indicesVec[i] = builder.create<arith::MinUIOp>(loc, nonNegative, lastIdx);
  }

  if (!boundsValid)
    return;

  SmallVector<Value> offsetsVec(acquireOp.getOffsets().begin(),
                                acquireOp.getOffsets().end());
  SmallVector<Value> sizesVec(acquireOp.getSizes().begin(),
                              acquireOp.getSizes().end());
  SmallVector<Value> partIndices(acquireOp.getPartitionIndices().begin(),
                                 acquireOp.getPartitionIndices().end());
  SmallVector<Value> partOffsets(acquireOp.getPartitionOffsets().begin(),
                                 acquireOp.getPartitionOffsets().end());
  SmallVector<Value> partSizes(acquireOp.getPartitionSizes().begin(),
                               acquireOp.getPartitionSizes().end());

  auto partitionMode = getPartitionMode(acquireOp.getOperation());
  auto newAcquire = builder.create<DbAcquireOp>(
      loc, acquireOp.getMode(), acquireOp.getSourceGuid(),
      acquireOp.getSourcePtr(), partitionMode, indicesVec, offsetsVec, sizesVec,
      partIndices, partOffsets, partSizes, boundsValid);
  transferAttributes(acquireOp.getOperation(), newAcquire.getOperation(),
                     /*excludes=*/{});
  newAcquire.copyPartitionSegmentsFrom(acquireOp);
  transferValueContract(acquireOp.getPtr(), newAcquire.getPtr(), builder, loc);

  acquireOp.getGuid().replaceAllUsesWith(newAcquire.getGuid());
  acquireOp.getPtr().replaceAllUsesWith(newAcquire.getPtr());
  acquireOp.erase();
}

static void lowerStencilAcquireBounds(ModuleOp module,
                                      LoopAnalysis &loopAnalysis) {
  SmallVector<std::tuple<DbAcquireOp, SmallVector<int64_t>, Value>> pending;

  module.walk([&](DbAcquireOp acquireOp) {
    if (acquireOp.getBoundsValid())
      return;
    auto indices = acquireOp.getIndices();
    if (indices.empty())
      return;

    SmallVector<LoopNode *> enclosingLoops;
    loopAnalysis.collectEnclosingLoops(acquireOp, enclosingLoops);
    if (enclosingLoops.empty())
      return;

    LoopNode *loopNode = enclosingLoops.back();
    SmallVector<int64_t> boundsCheckFlags;
    bool needsBoundsCheck = false;
    for (Value idx : indices) {
      int64_t flag = 0;
      LoopNode::IVExpr expr = loopNode->analyzeIndexExpr(idx);
      if (expr.isAnalyzable() && expr.offset.has_value() && *expr.offset != 0) {
        flag = 1;
        needsBoundsCheck = true;
      } else if (expr.dependsOnIV && !expr.isAnalyzable()) {
        flag = 1;
        needsBoundsCheck = true;
      }
      boundsCheckFlags.push_back(flag);
    }

    if (needsBoundsCheck)
      pending.push_back({acquireOp, std::move(boundsCheckFlags),
                         loopNode->getInductionVar()});
  });

  for (auto &[acquireOp, flags, loopIV] : pending)
    applyBoundsValid(acquireOp, flags, loopIV);
}

/// Analyze a single acquire to determine its partition mode and chunk info.
static AcquirePartitionInfo
computeAcquirePartitionInfo(DbAnalysis &dbAnalysis, DbAcquireOp acquire,
                            const DbAnalysis::AcquireContractSummary *summary,
                            const DbAcquirePartitionFacts *facts,
                            OpBuilder &builder) {
  AcquirePartitionInfo info;
  info.acquire = acquire;
  DbAnalysis::AcquirePartitionSummary partitionSummary;
  partitionSummary = dbAnalysis.analyzeAcquirePartition(acquire, builder);
  info.mode = partitionSummary.mode;
  info.partitionOffsets.assign(partitionSummary.partitionOffsets.begin(),
                               partitionSummary.partitionOffsets.end());
  info.partitionSizes.assign(partitionSummary.partitionSizes.begin(),
                             partitionSummary.partitionSizes.end());
  info.partitionDims.assign(partitionSummary.partitionDims.begin(),
                            partitionSummary.partitionDims.end());
  info.accessPattern =
      dbAnalysis.resolveCanonicalAcquireAccessPattern(acquire, summary, facts);
  info.isValid = partitionSummary.isValid;
  info.hasIndirectAccess = summary ? summary->hasIndirectAccess()
                                   : partitionSummary.hasIndirectAccess;
  info.hasDistributionContract = summary
                                     ? summary->hasDistributionContract()
                                     : partitionSummary.hasDistributionContract;
  info.preservesDepMode = static_cast<bool>(acquire.getPreserveAccessMode());

  if (summary && summary->contract.supportsBlockHalo() &&
      !summary->contract.ownerDims.empty()) {
    unsigned contractRank = 0;
    for (int64_t dim : summary->contract.ownerDims) {
      if (dim >= 0)
        contractRank =
            std::max<unsigned>(contractRank, static_cast<unsigned>(dim) + 1);
    }
    if (contractRank == 0)
      contractRank = static_cast<unsigned>(summary->contract.ownerDims.size());

    SmallVector<unsigned, 4> contractDims =
        resolveContractOwnerDims(summary->contract, contractRank);
    if (!contractDims.empty()) {
      if (!info.partitionOffsets.empty() &&
          contractDims.size() > info.partitionOffsets.size())
        contractDims.resize(info.partitionOffsets.size());
      info.partitionDims.assign(contractDims.begin(), contractDims.end());
    }

    if (info.mode == PartitionMode::coarse &&
        (summary->hasBlockHints() || summary->inferredBlock())) {
      info.mode = summary->contract.isStencilFamily() ? PartitionMode::stencil
                                                      : PartitionMode::block;
    }
  }

  return info;
}

static void resetCoarseAcquireRanges(DbAllocOp allocOp, DbAllocNode *allocNode,
                                     OpBuilder &builder) {
  if (!allocNode || allocOp.getSizes().empty())
    return;

  Value zero = arts::createZeroIndex(builder, allocOp.getLoc());
  SmallVector<Value> fullOffsets;
  SmallVector<Value> fullSizes;
  for (Value size : allocOp.getSizes()) {
    fullOffsets.push_back(zero);
    fullSizes.push_back(size);
  }

  SmallVector<DbAcquireNode *, 16> acquireNodes =
      allocNode->collectAllAcquireNodes();
  for (DbAcquireNode *acqNode : acquireNodes) {
    DbAcquireOp acqOp = acqNode->getDbAcquireOp();
    if (!acqOp)
      continue;
    /// preserve_access_mode protects the acquire's access mode contract, not
    /// the allocation layout. If the allocation fell back to coarse, always
    /// reset the DB-space slice to match the coarse layout.
    acqOp.getOffsetsMutable().assign(fullOffsets);
    acqOp.getSizesMutable().assign(fullSizes);
    /// Clear partition hints for coarse allocations to avoid redundant
    /// per-task hint plumbing and enable invariant acquire hoisting.
    acqOp.getPartitionIndicesMutable().clear();
    acqOp.getPartitionOffsetsMutable().clear();
    acqOp.getPartitionSizesMutable().clear();
    /// Keep acquire partition attribute consistent with coarse allocation.
    setPartitionMode(acqOp.getOperation(), PartitionMode::coarse);
  }
}

/// EXT-PART-5: Heuristic-to-contract feedback.
///
/// After DbPartitioning resolves block sizes and partition dimensions for an
/// allocation, write the chosen values back into the LoweringContractOp on the
/// alloc's ptr so downstream passes can see them without re-deriving the
/// information from scattered attributes.
///
/// This reads any existing contract, merges in the resolved blockShape,
/// ownerDims, and distributionVersion, then upserts the updated contract.
/// Returns true if a contract was updated.
static bool
feedbackPartitionDecisionToContract(DbAllocOp newAllocOp,
                                    ArrayRef<Value> blockSizes,
                                    ArrayRef<unsigned> partitionedDims,
                                    PartitionMode mode, OpBuilder &builder) {
  if (!newAllocOp)
    return false;

  Value allocPtr = newAllocOp.getPtr();
  if (!allocPtr)
    return false;

  /// Read the existing contract (if any) to preserve upstream-seeded fields
  /// such as depPattern, distributionKind, distributionPattern, halo offsets,
  /// stencilIndependentDims, etc.
  LoweringContractInfo info;
  if (auto existing = getLoweringContract(allocPtr))
    info = *existing;

  /// Populate resolved block shape from the block-plan resolver output.
  if (!blockSizes.empty() &&
      (info.ownerDims.empty() || blockSizes.size() == info.ownerDims.size())) {
    info.blockShape.assign(blockSizes.begin(), blockSizes.end());
  }

  /// Populate ownerDims from the resolved partitioned dimensions only when the
  /// contract does not already carry an explicit higher-rank semantic owner
  /// shape. For stencil families, ownerDims describe the semantic footprint
  /// rank and must not be collapsed to a narrower layout choice here.
  if (!partitionedDims.empty() &&
      (!info.hasExplicitStencilContract() || info.ownerDims.empty())) {
    info.ownerDims.clear();
    info.ownerDims.reserve(partitionedDims.size());
    for (unsigned dim : partitionedDims)
      info.ownerDims.push_back(static_cast<int64_t>(dim));
  }

  /// Bump distributionVersion so downstream passes can detect that the
  /// contract was enriched by the partitioning heuristic.
  int64_t currentVersion = info.distributionVersion.value_or(0);
  info.distributionVersion = currentVersion + 1;

  /// Mark as post-DB refined so consumers can distinguish contracts that
  /// survived partitioning from pre-partitioning seeds.
  info.postDbRefined = true;

  /// Partitioning may learn a narrower layout than the semantic owner-dim
  /// footprint carried by stencil contracts. Drop any ranked payload that no
  /// longer matches the semantic rank before materializing the contract.
  normalizeLoweringContractInfo(info);

  /// Guard: if the enriched contract is empty (no meaningful data), skip.
  if (info.empty())
    return false;

  OpBuilder::InsertionGuard guard(builder);
  builder.setInsertionPointAfter(newAllocOp);
  upsertLoweringContract(builder, newAllocOp.getLoc(), allocPtr, info);

  ARTS_DEBUG("  EXT-PART-5: fed back partition decision to contract"
             << " (blockShape=" << blockSizes.size()
             << ", ownerDims=" << partitionedDims.size()
             << ", version=" << (currentVersion + 1) << ")");
  return true;
}

} // namespace

namespace {
struct DbPartitioningPass
    : public arts::DbPartitioningBase<DbPartitioningPass> {
  DbPartitioningPass(mlir::arts::AnalysisManager *AM) : AM(AM) {
    assert(AM && "AnalysisManager must be provided externally");
  }

  void runOnOperation() override;

  ///===--------------------------------------------------------------------===///
  /// Main Partitioning Methods
  ///===--------------------------------------------------------------------===///

  /// Iterates over allocations and applies partitioning rewriters.
  bool partitionDb();
  bool gatherPartitionFacts();
  bool evaluatePolicyBuildPlansAndApply();
  void validateAndPostCheck(bool changed);

  ///===--------------------------------------------------------------------===///
  /// Stencil Bounds Analysis
  ///===--------------------------------------------------------------------===///

  /// Analyze stencil access patterns and generate bounds check flags.
  void analyzeStencilBounds();

  ///===--------------------------------------------------------------------===///
  /// Graph Management
  ///===--------------------------------------------------------------------===///

  /// Invalidate and rebuild the datablock graph after transformations.
  void invalidateAndRebuildGraph();

  /// Partition an allocation based on per-acquire analysis and heuristics
  FailureOr<DbAllocOp> partitionAlloc(DbAllocOp allocOp,
                                      DbAllocNode *allocNode);

  /// Expand multi-entry acquires into separate DbAcquireOps.nt.
  bool expandMultiEntryAcquires();

private:
  /// Build per-acquire capability info (canBlock, canElementWise, etc.) and
  /// populate ctx.acquires for heuristic voting. Returns true if the caller
  /// should early-return (preserve explicit contract layout).
  bool buildPerAcquireCapabilities(
      PartitioningContext &ctx,
      SmallVectorImpl<AcquirePartitionInfo> &acquireInfos,
      ArrayRef<DbAcquireNode *> allocAcquireNodes,
      ArrayRef<std::optional<DbAnalysis::AcquireContractSummary>>
          acquireContractSummaries,
      ArrayRef<const DbAcquirePartitionFacts *> acquireFacts,
      SmallVectorImpl<Value> &blockSizesForNDBlock, DbAllocOp allocOp,
      DbAllocNode *allocNode, DbAnalysis &dbAnalysis, bool canUseBlock,
      std::optional<int64_t> &writerBlockSizeHint,
      std::optional<int64_t> &anyBlockSizeHint, OpBuilder &builder);

  /// Compute stencil halo bounds from acquire nodes and multi-entry structure.
  /// Returns the computed StencilInfo, or std::nullopt if stencil mode should
  /// be downgraded (e.g. single-sided halo).
  std::optional<StencilInfo> computeStencilHaloInfo(
      DbAllocOp allocOp, DbAllocNode *allocNode,
      ArrayRef<DbAcquireNode *> allocAcquireNodes,
      ArrayRef<std::optional<DbAnalysis::AcquireContractSummary>>
          acquireContractSummaries,
      ArrayRef<const DbAcquirePartitionFacts *> acquireFacts,
      SmallVectorImpl<AcquirePartitionInfo> &acquireInfos,
      bool allocHasStencilPattern, DbAnalysis &dbAnalysis,
      PartitioningDecision &decision, PartitioningContext &ctx,
      OpBuilder &builder, Location loc);

  /// Resolve block-plan sizes and partition dimensions, and reconcile
  /// per-acquire dimension compatibility.
  bool resolveBlockPlanSizes(
      SmallVectorImpl<Value> &blockSizesForPlan,
      SmallVectorImpl<unsigned> &partitionedDimsForPlan,
      SmallVectorImpl<AcquirePartitionInfo> &acquireInfos,
      SmallVectorImpl<Value> &blockSizesForNDBlock,
      PartitioningDecision &decision, PartitioningContext &ctx,
      std::optional<StencilInfo> &stencilInfo, DbAllocOp allocOp,
      DbAllocNode *allocNode, DbHeuristics &heuristics, OpBuilder &builder,
      Location loc);

  /// Assemble the rewrite plan from partition decision and apply it via
  /// DbRewriter. Returns the rewritten DbAllocOp on success.
  FailureOr<DbAllocOp> assembleAndApplyRewritePlan(
      PartitioningDecision &decision,
      SmallVectorImpl<AcquirePartitionInfo> &acquireInfos,
      SmallVectorImpl<Value> &blockSizesForPlan,
      SmallVectorImpl<unsigned> &partitionedDimsForPlan,
      SmallVectorImpl<Value> &newOuterSizes,
      SmallVectorImpl<Value> &newInnerSizes,
      std::optional<StencilInfo> &stencilInfo, DbAllocOp allocOp,
      DbHeuristics &heuristics, OpBuilder &builder);

  ModuleOp module;
  mlir::arts::AnalysisManager *AM = nullptr;
};
} // namespace

void DbPartitioningPass::runOnOperation() {
  module = getOperation();

  ARTS_INFO_HEADER(DbPartitioningPass);
  ARTS_DEBUG_REGION(module.dump(););

  assert(AM && "AnalysisManager must be provided externally");

  /// Phase 1: Gather facts (IR normalization for analysis + graph rebuild).
  bool changed = gatherPartitionFacts();

  /// Phase 2-4: Evaluate policy, build rewrite plans, and apply rewrites.
  changed |= evaluatePolicyBuildPlansAndApply();

  /// Phase 5: Validate/post-check and invalidate stale analyses.
  validateAndPostCheck(changed);

  ARTS_INFO_FOOTER(DbPartitioningPass);
  ARTS_DEBUG_REGION(module.dump(););
}

bool DbPartitioningPass::gatherPartitionFacts() {
  /// Expand multi-entry acquires before graph construction so each acquire can
  /// be analyzed independently.
  bool changed = expandMultiEntryAcquires();
  invalidateAndRebuildGraph();
  return changed;
}

bool DbPartitioningPass::evaluatePolicyBuildPlansAndApply() {
  return partitionDb();
}

void DbPartitioningPass::validateAndPostCheck(bool changed) {
  analyzeStencilBounds();
  if (!changed)
    return;
  ARTS_INFO(" Module has changed, invalidating analyses");
  AM->invalidate();
}

namespace {
static SmallVector<DbAcquireOp> createExpandedAcquires(DbAcquireOp original,
                                                       OpBuilder &builder) {
  SmallVector<DbAcquireOp> expanded;
  Location loc = original.getLoc();
  size_t numEntries = original.getNumPartitionEntries();

  for (size_t i = 0; i < numEntries; ++i) {
    PartitionMode entryMode = original.getPartitionEntryMode(i);
    SmallVector<Value> entryIndices = original.getPartitionIndicesForEntry(i);
    SmallVector<Value> entryOffsets = original.getPartitionOffsetsForEntry(i);
    SmallVector<Value> entrySizes = original.getPartitionSizesForEntry(i);

    ARTS_DEBUG("    Entry " << i << ": mode=" << static_cast<int>(entryMode)
                            << ", indices=" << entryIndices.size()
                            << ", offsets=" << entryOffsets.size());

    auto newAcquire = builder.create<DbAcquireOp>(
        loc, original.getMode(), original.getSourceGuid(),
        original.getSourcePtr(), entryMode,
        /*indices=*/SmallVector<Value>{},
        /*offsets=*/
        SmallVector<Value>(original.getOffsets().begin(),
                           original.getOffsets().end()),
        /*sizes=*/
        SmallVector<Value>(original.getSizes().begin(),
                           original.getSizes().end()),
        /*partition_indices=*/entryIndices,
        /*partition_offsets=*/entryOffsets,
        /*partition_sizes=*/entrySizes);

    /// Structural expansion must preserve the same high-level contract that
    /// downstream DB planning saw on the original multi-entry acquire.
    transferContract(original.getOperation(), newAcquire.getOperation(),
                     original.getPtr(), newAcquire.getPtr(), builder, loc);

    /// Preserve per-entry segmentation for single-entry acquires so
    /// getPartitionInfos() can infer dimensionality.
    SmallVector<int32_t> indicesSegs = {
        static_cast<int32_t>(entryIndices.size())};
    SmallVector<int32_t> offsetsSegs = {
        static_cast<int32_t>(entryOffsets.size())};
    SmallVector<int32_t> sizesSegs = {static_cast<int32_t>(entrySizes.size())};
    SmallVector<int32_t> entryModes = {static_cast<int32_t>(entryMode)};
    newAcquire.setPartitionSegments(indicesSegs, offsetsSegs, sizesSegs,
                                    entryModes);
    if (original.getPreserveAccessMode())
      newAcquire.setPreserveAccessMode();
    if (original.getPreserveDepEdge())
      newAcquire.setPreserveDepEdge();

    expanded.push_back(newAcquire);
    ARTS_DEBUG("    Created expanded acquire: " << newAcquire);
  }

  return expanded;
}

static SmallVector<Value> rebuildEdtDeps(EdtOp edt, DbAcquireOp original,
                                         ArrayRef<DbAcquireOp> expanded) {
  SmallVector<Value> deps;
  for (Value dep : edt.getDependencies()) {
    if (dep == original.getPtr()) {
      for (DbAcquireOp acq : expanded)
        deps.push_back(acq.getPtr());
    } else {
      deps.push_back(dep);
    }
  }
  return deps;
}

static SmallVector<BlockArgument>
insertExpandedBlockArgs(EdtOp edt, BlockArgument originalArg, size_t numEntries,
                        Type argType, Location loc) {
  Block &edtBlock = edt.getBody().front();
  SmallVector<BlockArgument> args;
  args.push_back(originalArg);
  for (size_t i = 1; i < numEntries; ++i) {
    unsigned insertPos = originalArg.getArgNumber() + i;
    BlockArgument newArg = edtBlock.insertArgument(insertPos, argType, loc);
    args.push_back(newArg);
  }
  return args;
}

static SmallVector<DbRefOp> collectDbRefUsers(BlockArgument arg) {
  SmallVector<DbRefOp> refs;
  for (Operation *user : arg.getUsers())
    if (auto dbRef = dyn_cast<DbRefOp>(user))
      refs.push_back(dbRef);
  return refs;
}

static SmallVector<Value> collectAccessIndices(DbRefOp dbRef) {
  SmallVector<Value> indices;
  DbUtils::forEachReachableMemoryAccess(
      dbRef.getResult(),
      [&](const DbUtils::MemoryAccessInfo &access) {
        indices = access.indices;
        return indices.empty() ? WalkResult::advance()
                               : WalkResult::interrupt();
      },
      dbRef->getParentRegion());
  return indices;
}

static SmallVector<Value> collectAccessIndicesFromUser(Operation *refUser) {
  SmallVector<Value> direct = DbUtils::getMemoryAccessIndices(refUser);
  if (!direct.empty())
    return direct;

  for (Value result : refUser->getResults()) {
    if (!isa<MemRefType>(result.getType()))
      continue;
    SmallVector<Value> indices;
    DbUtils::forEachReachableMemoryAccess(
        result,
        [&](const DbUtils::MemoryAccessInfo &access) {
          indices = access.indices;
          return indices.empty() ? WalkResult::advance()
                                 : WalkResult::interrupt();
        },
        refUser->getParentRegion());
    if (!indices.empty())
      return indices;
  }
  return {};
}

struct EntryMatch {
  size_t index = 0;
  bool found = false;
  int score = -1;
};

static SmallVector<Value> getEntryAnchors(DbAcquireOp original, size_t entry) {
  SmallVector<Value> anchors = original.getPartitionIndicesForEntry(entry);
  if (!anchors.empty())
    return anchors;
  return original.getPartitionOffsetsForEntry(entry);
}

static int indexMatchStrength(Value idx, Value anchor) {
  if (!idx || !anchor)
    return 0;

  idx = ValueAnalysis::stripSelectClamp(idx);
  anchor = ValueAnalysis::stripSelectClamp(anchor);
  idx = ValueAnalysis::stripNumericCasts(idx);
  anchor = ValueAnalysis::stripNumericCasts(anchor);

  if (ValueAnalysis::sameValue(idx, anchor) ||
      ValueAnalysis::areValuesEquivalent(idx, anchor))
    return 4;

  if (ValueAnalysis::dependsOn(idx, anchor) ||
      ValueAnalysis::dependsOn(anchor, idx))
    return 2;

  if (auto cast = idx.getDefiningOp<arith::IndexCastOp>())
    idx = ValueAnalysis::stripSelectClamp(cast.getIn());
  if (auto blockArg = dyn_cast<BlockArgument>(idx)) {
    if (auto forOp = dyn_cast<scf::ForOp>(blockArg.getOwner()->getParentOp())) {
      if (blockArg == forOp.getInductionVar()) {
        Value lb = forOp.getLowerBound();
        lb = ValueAnalysis::stripSelectClamp(lb);
        if (ValueAnalysis::sameValue(lb, anchor) ||
            ValueAnalysis::areValuesEquivalent(lb, anchor) ||
            ValueAnalysis::dependsOn(lb, anchor) ||
            ValueAnalysis::dependsOn(anchor, lb))
          return 1;
      }
    }
  }

  return 0;
}

static int scoreEntry(ArrayRef<Value> indices, ArrayRef<Value> anchors) {
  int score = 0;
  size_t n = std::min(indices.size(), anchors.size());
  for (size_t d = 0; d < n; ++d) {
    Value idx = indices[d];
    Value anchor = anchors[d];
    if (!idx || !anchor)
      continue;
    score += indexMatchStrength(idx, anchor);
  }
  return score;
}

static EntryMatch matchDbRefEntry(DbRefOp dbRef, ArrayRef<Value> accessIndices,
                                  DbAcquireOp original, size_t numEntries) {
  EntryMatch match;

  auto tryMatch = [&](ArrayRef<Value> indices) {
    if (indices.empty())
      return;
    for (size_t i = 0; i < numEntries; ++i) {
      SmallVector<Value> anchors = getEntryAnchors(original, i);
      if (anchors.empty())
        continue;
      int score = scoreEntry(indices, anchors);
      if (score > match.score) {
        match.score = score;
        match.index = i;
      }
    }
  };

  SmallVector<Value> dbRefIndices(dbRef.getIndices().begin(),
                                  dbRef.getIndices().end());
  tryMatch(dbRefIndices);
  tryMatch(accessIndices);
  match.found = match.score > 0;
  return match;
}

} // namespace

/// Expand multi-entry acquires into separate DbAcquireOps.
/// When an OpenMP task has `depend(in: A[i][j], A[i-1][j])`, CreateDbs captures
/// both entries in a single DbAcquireOp with segment attributes. This function
/// expands each entry into its own acquire with a corresponding EDT block arg.
///
/// BEFORE:
///   %acq = arts.db_acquire [inout] (%guid, %ptr)
///     partition_indices = [%i, %j, %i_minus_1, %j]
///     partition_indices_segments = [2, 2]  /// Two entries, each with 2
///     indices partition_entry_modes = [fine_grained, fine_grained]
///   arts.edt (%acq) { ^bb0(%arg0): ... }
///
/// AFTER:
///   %acq0 = arts.db_acquire [inout] (%guid, %ptr)
///     partitioning(fine_grained, indices[%i, %j])
///   %acq1 = arts.db_acquire [inout] (%guid, %ptr)
///     partitioning(fine_grained, indices[%i_minus_1, %j])
///   arts.edt (%acq0, %acq1) { ^bb0(%arg0, %arg1): ... }
bool DbPartitioningPass::expandMultiEntryAcquires() {
  ARTS_DEBUG_HEADER(ExpandMultiEntryAcquires);
  bool changed = false;

  SmallVector<DbAcquireOp> acquiresToExpand;
  module.walk([&](DbAcquireOp acquire) {
    if (acquire.hasMultiplePartitionEntries())
      acquiresToExpand.push_back(acquire);
  });

  if (acquiresToExpand.empty()) {
    ARTS_DEBUG("  No multi-entry acquires to expand");
    return false;
  }

  ARTS_DEBUG("  Found " << acquiresToExpand.size()
                        << " multi-entry acquires to expand");

  for (DbAcquireOp original : acquiresToExpand) {
    size_t numEntries = original.getNumPartitionEntries();
    if (numEntries <= 1)
      continue;

    /// Check for stencil pattern - skip expansion only when we need ESD.
    /// If the stencil entries are explicit fine-grained deps (e.g., u[i-1],
    /// u[i], u[i+1]), prefer expansion to element-wise acquires.
    int64_t minOffset = 0, maxOffset = 0;
    if (DbAnalysis::hasMultiEntryStencilPattern(original, minOffset,
                                                maxOffset)) {
      bool allFineGrainedEntries = true;
      for (size_t entryIdx = 0; entryIdx < numEntries; ++entryIdx) {
        if (original.getPartitionEntryMode(entryIdx) !=
            PartitionMode::fine_grained) {
          allFineGrainedEntries = false;
          break;
        }
      }

      if (!allFineGrainedEntries) {
        ARTS_DEBUG("  Stencil pattern detected for acquire with "
                   << numEntries << " entries: haloLeft=" << (-minOffset)
                   << ", haloRight=" << maxOffset
                   << " - skipping expansion for ESD mode");

        /// Set stencil partition mode to preserve multi-entry structure
        /// The partitioning pass will handle this with stencil/ESD mode
        original.setPartitionModeAttr(PartitionModeAttr::get(
            original.getContext(), PartitionMode::stencil));
        continue;
      }

      ARTS_DEBUG("  Stencil pattern detected for acquire with "
                 << numEntries << " entries"
                 << " - expanding for explicit fine-grained deps");
    }

    ARTS_DEBUG("  Expanding acquire with " << numEntries
                                           << " entries: " << original);

    OpBuilder builder(original);

    /// Find the EDT that uses this acquire
    auto [edtUser, blockArg] = getEdtBlockArgumentForAcquire(original);
    if (!edtUser) {
      ARTS_DEBUG("    No EDT user found, skipping expansion");
      continue;
    }

    /// Create separate acquires for each partition entry
    SmallVector<DbAcquireOp> expandedAcquires =
        createExpandedAcquires(original, builder);

    /// Update EDT dependencies: replace original with all expanded acquires
    edtUser.setDependencies(
        rebuildEdtDeps(edtUser, original, expandedAcquires));

    /// Add new block arguments for expanded acquires (first replaces original)
    Type argType = blockArg.getType();
    SmallVector<BlockArgument> newBlockArgs = insertExpandedBlockArgs(
        edtUser, blockArg, numEntries, argType, original.getLoc());

    /// Remap DbRef operations to use their correct block arguments.
    for (DbRefOp dbRef : collectDbRefUsers(blockArg)) {
      SmallVector<Operation *> users(dbRef.getResult().getUsers().begin(),
                                     dbRef.getResult().getUsers().end());
      SmallVector<std::pair<Operation *, size_t>> matchedUsers;
      SmallVector<Operation *> unmatchedUsers;
      llvm::DenseMap<size_t, unsigned> entryCounts;

      for (Operation *user : users) {
        SmallVector<Value> accessIndices = collectAccessIndicesFromUser(user);
        if (accessIndices.empty()) {
          unmatchedUsers.push_back(user);
          continue;
        }

        EntryMatch match =
            matchDbRefEntry(dbRef, accessIndices, original, numEntries);
        if (!match.found && numEntries > 1) {
          ARTS_DEBUG("      WARNING: Could not match db_ref user to partition "
                     "entry, defaulting to entry 0");
        }

        ARTS_DEBUG("      db_ref user matched entry "
                   << match.index
                   << " (access indices: " << accessIndices.size()
                   << ", found: " << match.found << ")");

        matchedUsers.emplace_back(user, match.index);
        entryCounts[match.index]++;
      }

      if (entryCounts.empty()) {
        SmallVector<Value> accessIndices = collectAccessIndices(dbRef);
        EntryMatch match =
            matchDbRefEntry(dbRef, accessIndices, original, numEntries);

        if (!match.found && numEntries > 1) {
          ARTS_DEBUG(
              "      WARNING: Could not match db_ref to partition entry, "
              "defaulting to entry 0");
        }

        ARTS_DEBUG("      db_ref matched entry "
                   << match.index
                   << " (access indices: " << accessIndices.size()
                   << ", found: " << match.found << ")");

        dbRef.getSourceMutable().assign(newBlockArgs[match.index]);
        continue;
      }

      if (entryCounts.size() == 1) {
        size_t entry = entryCounts.begin()->first;
        ARTS_DEBUG("      db_ref matched entry " << entry
                                                 << " (single-entry match)");
        dbRef.getSourceMutable().assign(newBlockArgs[entry]);
        continue;
      }

      /// Multiple entries matched: clone db_ref per entry and remap uses.
      OpBuilder refBuilder(dbRef);
      llvm::DenseMap<size_t, DbRefOp> entryRefs;
      for (const auto &entryPair : entryCounts) {
        size_t entry = entryPair.first;
        DbRefOp newRef =
            refBuilder.create<DbRefOp>(dbRef.getLoc(), dbRef.getType(),
                                       newBlockArgs[entry], dbRef.getIndices());
        entryRefs[entry] = newRef;
      }

      size_t fallbackEntry = entryRefs.count(0) ? 0 : entryRefs.begin()->first;
      DbRefOp fallbackRef = entryRefs[fallbackEntry];

      for (const auto &matchPair : matchedUsers) {
        Operation *user = matchPair.first;
        size_t entry = matchPair.second;
        DbRefOp refForEntry = entryRefs[entry];
        user->replaceUsesOfWith(dbRef.getResult(), refForEntry.getResult());
      }

      for (Operation *user : unmatchedUsers) {
        user->replaceUsesOfWith(dbRef.getResult(), fallbackRef.getResult());
      }

      dbRef.erase();
    }

    /// Create db_release operations for new block arguments (entries 1+)
    /// The original block arg (entry 0) already has a release from CreateDbs
    DbReleaseOp existingRelease = nullptr;
    for (Operation *user : blockArg.getUsers()) {
      if (auto rel = dyn_cast<DbReleaseOp>(user)) {
        existingRelease = rel;
        break;
      }
    }
    if (existingRelease) {
      OpBuilder releaseBuilder(existingRelease);
      for (size_t i = 1; i < numEntries; ++i) {
        releaseBuilder.create<DbReleaseOp>(original.getLoc(), newBlockArgs[i]);
      }
      ARTS_DEBUG("    Created " << (numEntries - 1)
                                << " db_release ops for expanded block args");
    }

    /// Erase original acquire
    original.erase();
    changed = true;

    ARTS_DEBUG("    Expanded into " << expandedAcquires.size()
                                    << " separate acquires");
  }

  return changed;
}

/// Partition allocations based on disjoint access proof.
bool DbPartitioningPass::partitionDb() {
  ARTS_DEBUG_HEADER(PartitionDb);
  bool changed = false;

  /// PHASE 1: Partition allocations
  module.walk([&](func::FuncOp func) {
    DbGraph &graph = AM->getDbAnalysis().getOrCreateGraph(func);
    graph.forEachAllocNode([&](DbAllocNode *allocNode) {
      if (!allocNode)
        return;
      DbAllocOp allocOp = allocNode->getDbAllocOp();
      if (!allocOp || !allocNode->canBePartitioned())
        return;

      auto promotedOr = partitionAlloc(allocOp, allocNode);
      if (failed(promotedOr))
        return;

      DbAllocOp promoted = promotedOr.value();
      if (promoted.getOperation() != allocOp.getOperation())
        changed = true;
    });
  });

  if (changed)
    invalidateAndRebuildGraph();

  ARTS_DEBUG_FOOTER(PartitionDb);
  return changed;
}

/// Analyze allocation acquires and apply appropriate partitioning rewriter.
/// TODO(REFACTOR): partitionAlloc is ~1000 lines. Consider splitting into:
/// (a) buildPartitionContext (per-acquire capability loop),
/// (b) resolveStencilAndBlockPlan (stencil info + block-plan resolution),
/// (c) assembleAndApplyRewritePlan (rewrite plan assembly + application).
FailureOr<DbAllocOp>
DbPartitioningPass::partitionAlloc(DbAllocOp allocOp, DbAllocNode *allocNode) {
  if (!allocOp || !allocOp.getOperation())
    return failure();

  if (allocOp.getElementSizes().empty())
    return failure();

  auto &heuristics = AM->getDbHeuristics();

  /// Step 0: Check existing partition attribute
  if (auto existingMode = getPartitionMode(allocOp)) {
    /// Fine-grained allocations are already in their terminal shape.
    /// Rewriting them again adds compile-time churn without improving
    /// localization.
    if (*existingMode == PartitionMode::fine_grained) {
      ARTS_DEBUG("  Already partitioned as "
                 << mlir::arts::stringifyPartitionMode(*existingMode)
                 << " - SKIP");
      heuristics.recordDecision("Partition-AlreadyPartitioned", false,
                                "allocation already fine-grained, skipping",
                                allocOp.getOperation(), {});
      return allocOp;
    }

    /// For block/stencil, continue with loop-aware re-analysis.
    /// CreateDbs establishes an initial partition shape, but DbPartitioning
    /// still needs a chance to reconcile acquire ranges and EDT-local
    /// db_ref localization (especially for single-block layouts).
    ARTS_DEBUG("  Partition mode is "
               << mlir::arts::stringifyPartitionMode(*existingMode)
               << " - re-analyzing with loop structure");
    heuristics.recordDecision(
        "Partition-ReanalyzeExisting", true,
        "re-analyzing existing partition mode with loop-aware rewrite",
        allocOp.getOperation(), {});
  }

  /// Already fine-grained by structure - skip
  if (allocNode && DbAnalysis::isFineGrained(allocNode->getDbAllocOp())) {
    ARTS_DEBUG("  Already fine-grained by structure - SKIP");
    heuristics.recordDecision(
        "Partition-AlreadyFineGrained", false,
        "allocation already fine-grained by structure, skipping",
        allocOp.getOperation(), {});
    return allocOp;
  }

  OpBuilder builder(allocOp);
  Location loc = allocOp.getLoc();
  builder.setInsertionPoint(allocOp);

  SmallVector<DbAcquireNode *, 16> allocAcquireNodes =
      allocNode ? allocNode->collectAllAcquireNodes()
                : SmallVector<DbAcquireNode *, 16>{};

  /// Step 1: Analyze each acquire's PartitionMode
  SmallVector<AcquirePartitionInfo> acquireInfos;
  SmallVector<const DbAcquirePartitionFacts *> acquireFacts;
  SmallVector<std::optional<DbAnalysis::AcquireContractSummary>, 8>
      acquireContractSummaries;
  DbAnalysis &dbAnalysis = AM->getDbAnalysis();
  if (allocNode) {
    for (DbAcquireNode *acqNode : allocAcquireNodes) {
      const DbAcquirePartitionFacts *facts =
          dbAnalysis.getAcquirePartitionFacts(acqNode->getDbAcquireOp());
      if (!facts)
        facts = &acqNode->getPartitionFacts();
      acquireFacts.push_back(facts);
      auto contractSummary =
          dbAnalysis.getAcquireContractSummary(acqNode->getDbAcquireOp());
      acquireContractSummaries.push_back(contractSummary);
      auto info = computeAcquirePartitionInfo(
          dbAnalysis, acqNode->getDbAcquireOp(),
          contractSummary ? &*contractSummary : nullptr, facts, builder);
      if (!info.isValid) {
        ARTS_DEBUG("  Acquire analysis failed for "
                   << acqNode->getDbAcquireOp());
      }
      acquireInfos.push_back(info);
    }
  }

  if (acquireInfos.empty()) {
    ARTS_DEBUG("  No acquires to analyze - keeping original");
    heuristics.recordDecision(
        "Partition-NoAcquires", false,
        "no acquires to analyze, keeping original allocation",
        allocOp.getOperation(), {});
    return allocOp;
  }

  /// Step 2: Build PartitioningContext and check for non-leading offsets
  PartitioningContext ctx;
  SmallVector<Value> blockSizesForNDBlock;
  ctx.machine = &AM->getAbstractMachine();
  bool canUseBlock = true; /// Assume block is possible until proven otherwise

  /// Avoid multinode-style block partitioning when this allocation has no
  /// internode EDT users. Keep block disabled and rely on existing
  /// fine-grained/coarse semantics inferred from DB analysis.
  bool hasInternodeUsers = hasInternodeAcquireUser(allocAcquireNodes);
  bool isMultinodeMachine = AM->getAbstractMachine().hasValidNodeCount() &&
                            AM->getAbstractMachine().getNodeCount() > 1;
  if (isMultinodeMachine && !hasInternodeUsers) {
    canUseBlock = false;
    heuristics.recordDecision("Partition-DisableBlockNoInternodeUsers", false,
                              "no internode EDT users for allocation",
                              allocOp.getOperation(), {});
    ARTS_DEBUG("  No internode EDT users - disabling block partitioning");
  }

  if (allocNode) {
    ctx.accessPatterns = summarizeAcquirePatterns(allocAcquireNodes, allocNode,
                                                  &AM->getDbAnalysis());
    ctx.isUniformAccess = ctx.accessPatterns.hasUniform;
    ARTS_DEBUG("  Access patterns: hasUniform="
               << ctx.accessPatterns.hasUniform
               << ", hasStencil=" << ctx.accessPatterns.hasStencil
               << ", hasIndexed=" << ctx.accessPatterns.hasIndexed
               << ", isMixed=" << ctx.accessPatterns.isMixed());

    if (!allocOp.getElementSizes().empty()) {
      int64_t staticFirstDim = 0;
      if (ValueAnalysis::getConstantIndex(allocOp.getElementSizes().front(),
                                          staticFirstDim)) {
        ctx.totalElements = staticFirstDim;
      }
    }

    for (size_t factIdx = 0; factIdx < acquireContractSummaries.size();
         ++factIdx) {
      const auto &contractSummary = acquireContractSummaries[factIdx];
      if (!contractSummary)
        continue;

      const ArtsMode mode = factIdx < acquireInfos.size()
                                ? acquireInfos[factIdx].acquire.getMode()
                                : ArtsMode::uninitialized;
      if (contractSummary->hasIndirectAccess()) {
        ctx.hasIndirectAccess = true;
        if (mode == ArtsMode::in || mode == ArtsMode::inout)
          ctx.hasIndirectRead = true;
        if (DbUtils::isWriterMode(mode))
          ctx.hasIndirectWrite = true;
      }
      if (contractSummary->hasDirectAccess())
        ctx.hasDirectAccess = true;
    }

    /// Note: Indirect access is handled via full-range acquires, not by
    /// disabling chunked. Full-range acquires allow indirect reads to access
    /// all chunks.
    if (ctx.hasIndirectAccess) {
      ARTS_DEBUG("  Indirect access detected - indirect acquires will use "
                 "full-range");
    }

    /// Build per-acquire capabilities for heuristic voting
    std::optional<int64_t> writerBlockSizeHint;
    std::optional<int64_t> anyBlockSizeHint;
    if (buildPerAcquireCapabilities(
            ctx, acquireInfos, allocAcquireNodes, acquireContractSummaries,
            acquireFacts, blockSizesForNDBlock, allocOp, allocNode, dbAnalysis,
            canUseBlock, writerBlockSizeHint, anyBlockSizeHint, builder))
      return allocOp;

    /// Collect DbAcquireNode* and partition offsets for heuristic decisions
    /// Get per-acquire decisions from heuristics (calls needsFullRange)
    /// Build contract summary pointer array for heuristics consumption.
    SmallVector<const DbAnalysis::AcquireContractSummary *> summaryPtrs;
    summaryPtrs.reserve(acquireContractSummaries.size());
    for (auto &opt : acquireContractSummaries)
      summaryPtrs.push_back(opt ? &*opt : nullptr);
    auto acquireDecisions =
        heuristics.chooseAcquirePolicies(acquireFacts, summaryPtrs);

    /// Apply decisions to acquireInfos
    for (size_t i = 0; i < acquireDecisions.size() && i < acquireInfos.size();
         ++i) {
      if (acquireDecisions[i].needsFullRange) {
        acquireInfos[i].needsFullRange = true;
      }
    }

    if (writerBlockSizeHint)
      ctx.blockSize = writerBlockSizeHint;
    else if (anyBlockSizeHint)
      ctx.blockSize = anyBlockSizeHint;
  }

  /// Step 3: Reconcile per-acquire capabilities into allocation-level mode.
  AcquireModeReconcileResult modeSummary =
      reconcileAcquireModes(ctx, acquireInfos, canUseBlock);
  bool modesConsistent = modeSummary.modesConsistent;
  if (!modesConsistent)
    ARTS_DEBUG("  Inconsistent partition modes across acquires");
  if (modeSummary.allBlockFullRange) {
    ARTS_DEBUG("  All block-capable acquires are full-range; deferring to "
               "heuristics");
  }

  ARTS_DEBUG("  Per-acquire voting: canEW="
             << ctx.canElementWise << ", canBlock=" << ctx.canBlock
             << ", modesConsistent=" << modesConsistent);

  /// If no structural capability found, let heuristics handle it
  if (!ctx.canElementWise && !ctx.canBlock) {
    ARTS_DEBUG("  Coarse mode - no partitioning needed");
    resetCoarseAcquireRanges(allocOp, allocNode, builder);
    heuristics.recordDecision(
        "Partition-CoarseMode", false,
        "all acquires are coarse mode, no partitioning needed",
        allocOp.getOperation(), {});
    return allocOp;
  }

  /// Set pinnedDimCount for logging (heuristics compute min/max on-the-fly)
  ctx.pinnedDimCount = ctx.maxPinnedDimCount();
  ARTS_DEBUG("  Partition dim counts: min=" << ctx.minPinnedDimCount()
                                            << ", max=" << ctx.pinnedDimCount);

  /// Get access mode from allocNode
  if (allocNode)
    ctx.accessMode = allocOp.getMode();

  /// Get memref rank
  if (auto memrefType = dyn_cast<MemRefType>(allocOp.getElementType())) {
    ctx.memrefRank = memrefType.getRank();
    ctx.elementTypeIsMemRef = true;
  } else {
    ctx.memrefRank = allocOp.getElementSizes().size();
    ctx.elementTypeIsMemRef = false;
  }

  /// Preserve tiny read-only stencil coefficient tables as coarse.
  /// These tables are effectively constants (e.g., finite-difference weights);
  /// repartitioning them per task can skew coefficient indexing and produce
  /// thread-count-dependent numerics.
  auto currentMode = getPartitionMode(allocOp);
  auto currentPattern = getDbAccessPattern(allocOp.getOperation());
  if (currentMode && *currentMode == PartitionMode::coarse && currentPattern &&
      *currentPattern == DbAccessPattern::stencil &&
      allocOp.getMode() == ArtsMode::in &&
      allocOp.getDbMode() == DbMode::read) {
    std::optional<int64_t> staticElementCount = int64_t{1};
    auto multiplyIfConstant = [&](Value v) {
      if (!staticElementCount)
        return;
      auto folded = ValueAnalysis::tryFoldConstantIndex(v);
      if (!folded || *folded < 0) {
        staticElementCount = std::nullopt;
        return;
      }
      *staticElementCount *= *folded;
    };
    for (Value v : allocOp.getSizes())
      multiplyIfConstant(v);
    for (Value v : allocOp.getElementSizes())
      multiplyIfConstant(v);

    if (staticElementCount && *staticElementCount > 0 &&
        *staticElementCount <= 8) {
      resetCoarseAcquireRanges(allocOp, allocNode, builder);
      heuristics.recordDecision(
          "Partition-KeepTinyStencilCoarse", true,
          "preserving tiny read-only stencil coefficient table as coarse",
          allocOp.getOperation(),
          {{"staticElementCount", *staticElementCount}});
      return allocOp;
    }
  }

  /// Step 4: Initial heuristics arbitration.
  PartitioningDecision decision = heuristics.choosePartitioning(ctx);

  if (decision.isCoarse()) {
    ARTS_DEBUG("  Heuristics chose coarse - keeping original");
    resetCoarseAcquireRanges(allocOp, allocNode, builder);
    heuristics.recordDecision(
        "Partition-HeuristicsCoarse", false,
        "heuristics chose coarse allocation, keeping original",
        allocOp.getOperation(),
        {{"canBlock", ctx.canBlock ? 1 : 0},
         {"canElementWise", ctx.canElementWise ? 1 : 0}});
    return allocOp;
  }

  bool hasContractBackedStencil =
      llvm::any_of(acquireContractSummaries, [](const auto &summary) {
        return summary && summary->contract.hasExplicitStencilContract();
      });
  if (decision.mode == PartitionMode::stencil && !hasContractBackedStencil) {
    unsigned outerRank = std::max(1u, decision.outerRank);
    decision = PartitioningDecision::blockND(
        ctx, outerRank, "Fallback: stencil facts without explicit contract");
    ARTS_DEBUG("  Downgrading stencil decision to block mode because no "
               "explicit stencil contract was seeded pre-DB");
    heuristics.recordDecision(
        "Partition-StencilDowngradedToBlock", true,
        "stencil mode requires an explicit pre-DB contract; using block "
        "partitioning instead",
        allocOp.getOperation(), {{"outerRank", (int64_t)outerRank}});
  }

  /// Disabled: versioned partitioning path is currently off.

  /// For element-wise partitioning, validate that partition indices match
  /// actual accesses. If not (block-wise pattern detected), fall back to
  /// N-D block mode for proper parallelism.
  ///
  /// Block-wise patterns (like Cholesky's 2D block dependencies) use:
  ///   - Partition indices as block identifiers (not element indices)
  ///   - Loop step as block size per dimension
  bool skipAcquireInfoCheck = false;
  if (decision.isFineGrained() && ctx.accessPatterns.hasIndexed) {
    bool validElementWise =
        validateElementWiseIndices(acquireInfos, allocAcquireNodes);
    if (validElementWise) {
      /// Valid element-wise: we can skip per-acquire partition info check
      skipAcquireInfoCheck = true;
    } else {
      /// Block-wise pattern detected. Extract block sizes from loop steps
      /// for each partition dimension and use N-D block partitioning.
      ARTS_DEBUG("  Block-wise pattern detected - extracting N-D block sizes");

      /// Find the acquire with partition indices to extract block sizes
      for (auto &info : acquireInfos) {
        auto partitionIndices = info.acquire.getPartitionIndices();
        if (partitionIndices.empty())
          continue;

        /// For each partition index, try to find the enclosing loop step
        auto &loopAnalysis = AM->getLoopAnalysis();
        for (Value idx : partitionIndices) {
          if (LoopNode *drivingLoop = loopAnalysis.findEnclosingLoopDrivenBy(
                  info.acquire.getOperation(), idx)) {
            Value blockSize = drivingLoop->getStep();
            if (blockSize) {
              ARTS_DEBUG("    Found block size from loop step: " << blockSize);
              blockSizesForNDBlock.push_back(blockSize);
            }
          }
        }
        if (!blockSizesForNDBlock.empty())
          break; /// Use first acquire with valid block sizes
      }

      if (!blockSizesForNDBlock.empty()) {
        /// Switch to N-D block mode with extracted block sizes
        unsigned nDims = blockSizesForNDBlock.size();
        decision = PartitioningDecision::blockND(
            ctx, nDims, "Fallback: N-D block-wise pattern");
        ARTS_DEBUG("  Falling back to N-D block mode with "
                   << nDims << " partitioned dimensions");
        heuristics.recordDecision(
            "Partition-ElementWiseFallbackToNDBlock", true,
            "block-wise pattern detected, using N-D block partitioning",
            allocOp.getOperation(), {{"numPartitionedDims", (int64_t)nDims}});
      } else {
        /// No loop-step block sizes found. Keep the block path alive and let
        /// the shared resolver synthesize a safe 1-D plan from canonical
        /// acquire facts or the default machine-derived block size.
        ARTS_DEBUG("  Cannot extract N-D block sizes - deferring to shared "
                   "block-plan resolver");
        decision = PartitioningDecision::block(
            ctx, "Fallback: block-wise pattern via shared block-plan");
        heuristics.recordDecision(
            "Partition-ElementWiseFallbackToBlock", true,
            "block-wise pattern without explicit loop-step sizes, using "
            "shared block-plan resolver",
            allocOp.getOperation(), {});
      }
    }
  }

  /// Also try N-D extraction for explicit block/stencil decisions that haven't
  /// extracted block sizes yet. This handles cases like Cholesky where the
  /// heuristics already chose block mode but we need to extract the N-D
  /// block sizes from loop steps for proper multi-dimensional partitioning.
  /// Check for both indexed and uniform patterns (block-wise access).
  bool hasExplicitBlockSizes = false;
  for (const auto &info : acquireInfos) {
    if (info.mode != PartitionMode::block &&
        info.mode != PartitionMode::stencil)
      continue;
    for (Value sz : info.partitionSizes) {
      if (!sz)
        continue;
      Value stripped = ValueAnalysis::stripNumericCasts(sz);
      if (!ValueAnalysis::isOneConstant(stripped)) {
        hasExplicitBlockSizes = true;
        break;
      }
    }
    if (hasExplicitBlockSizes)
      break;
  }

  if (decision.isBlock() &&
      (ctx.accessPatterns.hasIndexed || ctx.accessPatterns.hasUniform) &&
      blockSizesForNDBlock.empty() && !hasExplicitBlockSizes) {
    ARTS_DEBUG(
        "  Block/stencil mode with indexed/uniform patterns - extracting "
        "N-D block sizes");
    for (auto &info : acquireInfos) {
      auto partitionIndices = info.acquire.getPartitionIndices();
      if (partitionIndices.empty())
        continue;

      /// For stencil acquires with multi-entry structure, use first entry's
      /// index to find the loop step (the base index, not the offset versions)
      SmallVector<Value> indicesToCheck;
      if (info.acquire.hasMultiplePartitionEntries()) {
        /// Find the center entry (offset 0) for stencil patterns
        size_t numEntries = info.acquire.getNumPartitionEntries();
        for (size_t i = 0; i < numEntries; ++i) {
          auto entryIndices = info.acquire.getPartitionIndicesForEntry(i);
          if (!entryIndices.empty())
            indicesToCheck.push_back(entryIndices[0]);
        }
        /// Use first unique index for loop detection
        if (!indicesToCheck.empty())
          indicesToCheck.resize(1);
      } else {
        indicesToCheck.assign(partitionIndices.begin(), partitionIndices.end());
      }

      /// For each partition index, try to find the enclosing loop step
      auto &loopAnalysis = AM->getLoopAnalysis();
      for (Value idx : indicesToCheck) {
        if (LoopNode *drivingLoop = loopAnalysis.findEnclosingLoopDrivenBy(
                info.acquire.getOperation(), idx)) {
          Value blockSize = drivingLoop->getStep();
          if (blockSize) {
            ARTS_DEBUG("    Found block size from loop step: " << blockSize);
            blockSizesForNDBlock.push_back(blockSize);
          }
        }
      }
      if (!blockSizesForNDBlock.empty())
        break; /// Use first acquire with valid block sizes
    }

    if (!blockSizesForNDBlock.empty()) {
      /// For stencil mode, keep the mode but update ranks based on block sizes
      /// For non-stencil block mode, upgrade to N-D block mode
      unsigned nDims = blockSizesForNDBlock.size();
      if (decision.mode != PartitionMode::stencil) {
        decision = PartitioningDecision::blockND(ctx, nDims,
                                                 "N-D block pattern detected");
        ARTS_DEBUG("  Upgraded to N-D block mode with " << nDims
                                                        << " dimensions");
        heuristics.recordDecision(
            "Partition-BlockUpgradedToND", true,
            "block mode upgraded with N-D block sizes from loop steps",
            allocOp.getOperation(), {{"numPartitionedDims", (int64_t)nDims}});
      } else {
        /// Stencil mode: keep mode, just log block size extraction
        ARTS_DEBUG("  Stencil mode: extracted " << nDims << " block sizes");
      }
    }
  }

  bool hasMissingAcquireInfo = false;
  if (!skipAcquireInfoCheck) {
    hasMissingAcquireInfo = llvm::any_of(acquireInfos, [&](auto &info) {
      if (info.needsFullRange)
        return false;
      return !info.isValid || info.partitionOffsets.empty() ||
             info.partitionSizes.empty();
    });
  }

  ARTS_DEBUG("  Decision: mode=" << getPartitionModeName(decision.mode)
                                 << ", outerRank=" << decision.outerRank
                                 << ", innerRank=" << decision.innerRank);

  auto allocPattern = getDbAccessPattern(allocOp.getOperation());
  bool allocHasStencilPattern =
      (allocPattern && *allocPattern == DbAccessPattern::stencil) ||
      ctx.accessPatterns.hasStencil;

  if (decision.mode == PartitionMode::stencil) {
    if (allocHasStencilPattern) {
      for (auto &info : acquireInfos) {
        if (!info.acquire)
          continue;
        bool hasPartitionRange =
            !info.partitionOffsets.empty() && !info.partitionSizes.empty();
        bool blockLikeMode =
            info.mode == PartitionMode::block ||
            info.mode == PartitionMode::stencil ||
            (info.mode == PartitionMode::coarse && info.isValid);
        if (blockLikeMode && hasPartitionRange)
          info.needsFullRange = false;
      }
    }
  }

  if (hasMissingAcquireInfo) {
    if (decision.isBlock()) {
      int64_t widenedAcquires = 0;
      for (auto &info : acquireInfos) {
        if (info.needsFullRange)
          continue;
        if (info.isValid && !info.partitionOffsets.empty() &&
            !info.partitionSizes.empty())
          continue;
        info.needsFullRange = true;
        ++widenedAcquires;
      }

      ARTS_DEBUG("  Some acquires missing partition info - forcing full-range "
                 "on block allocation");
      heuristics.recordDecision(
          "Partition-MissingAcquireInfo", true,
          "some acquires missing partition info, forcing full-range on block "
          "allocation",
          allocOp.getOperation(), {{"widenedAcquires", widenedAcquires}});
    } else {
      ARTS_DEBUG("  Some acquires missing partition info - keeping original");
      heuristics.recordDecision(
          "Partition-MissingAcquireInfo", false,
          "some acquires missing partition info, cannot rewrite allocation",
          allocOp.getOperation(), {});
      return allocOp;
    }
  }

  /// Step 5a: Compute stencilInfo early (needed for inner size calculation)
  /// Must be done before computeAllocationShape() for stencil mode.
  std::optional<StencilInfo> stencilInfo;
  if (decision.mode == PartitionMode::stencil && allocNode) {
    stencilInfo = computeStencilHaloInfo(
        allocOp, allocNode, allocAcquireNodes, acquireContractSummaries,
        acquireFacts, acquireInfos, allocHasStencilPattern, dbAnalysis,
        decision, ctx, builder, loc);
  }

  /// Step 5b: Resolve block-plan sizes and reconcile partition dimensions.
  SmallVector<Value> blockSizesForPlan;
  SmallVector<unsigned> partitionedDimsForPlan;
  if (decision.isBlock()) {
    if (!resolveBlockPlanSizes(blockSizesForPlan, partitionedDimsForPlan,
                               acquireInfos, blockSizesForNDBlock, decision,
                               ctx, stencilInfo, allocOp, allocNode, heuristics,
                               builder, loc))
      return allocOp;
  }

  SmallVector<Value> newOuterSizes, newInnerSizes;
  computeAllocationShape(decision.mode, allocOp, decision, blockSizesForPlan,
                         partitionedDimsForPlan, newOuterSizes, newInnerSizes);

  if (newOuterSizes.empty()) {
    ARTS_DEBUG("  Size computation failed - keeping original");
    heuristics.recordDecision(
        "Partition-SizeComputationFailed", false,
        "failed to compute partition sizes, keeping original",
        allocOp.getOperation(), {});
    return allocOp;
  }

  /// Steps 6-7: Assemble rewrite plan and apply via DbRewriter.
  return assembleAndApplyRewritePlan(decision, acquireInfos, blockSizesForPlan,
                                     partitionedDimsForPlan, newOuterSizes,
                                     newInnerSizes, stencilInfo, allocOp,
                                     heuristics, builder);
}

/// Build per-acquire capability info and populate ctx.acquires for heuristic
/// voting. Returns true if the caller should early-return the original allocOp
/// (e.g. when an explicit contract layout must be preserved).
bool DbPartitioningPass::buildPerAcquireCapabilities(
    PartitioningContext &ctx,
    SmallVectorImpl<AcquirePartitionInfo> &acquireInfos,
    ArrayRef<DbAcquireNode *> allocAcquireNodes,
    ArrayRef<std::optional<DbAnalysis::AcquireContractSummary>>
        acquireContractSummaries,
    ArrayRef<const DbAcquirePartitionFacts *> acquireFacts,
    SmallVectorImpl<Value> &blockSizesForNDBlock, DbAllocOp allocOp,
    DbAllocNode *allocNode, DbAnalysis &dbAnalysis, bool canUseBlock,
    std::optional<int64_t> &writerBlockSizeHint,
    std::optional<int64_t> &anyBlockSizeHint, OpBuilder &builder) {
  Location loc = allocOp.getLoc();
  auto &heuristics = AM->getDbHeuristics();

  size_t idx = 0;
  bool preserveExplicitContractLayout = false;
  for (DbAcquireNode *acqNode : allocAcquireNodes) {
    if (!acqNode || idx >= acquireInfos.size())
      continue;

    auto &acqInfo = acquireInfos[idx];
    const DbAnalysis::AcquireContractSummary *contractSummary =
        idx < acquireContractSummaries.size() && acquireContractSummaries[idx]
            ? &*acquireContractSummaries[idx]
            : nullptr;
    const DbAcquirePartitionFacts *facts =
        idx < acquireFacts.size() ? acquireFacts[idx] : nullptr;

    /// Check access patterns for block capability decisions.
    bool hasIndirect =
        facts ? facts->hasIndirectAccess : acqNode->hasIndirectAccess();

    /// Read partition capability from acquire's attribute.
    /// ForLowering sets this to 'chunked' when adding offset/size hints.
    /// CreateDbs sets this based on DbControlOp analysis.
    auto acquire = acqNode->getDbAcquireOp();
    auto acquireMode = getPartitionMode(acquire.getOperation());
    bool hasBlockHints = contractSummary
                             ? contractSummary->hasBlockHints()
                             : (!acquire.getPartitionOffsets().empty() ||
                                !acquire.getPartitionSizes().empty());
    bool inferredBlock = contractSummary
                             ? contractSummary->inferredBlock()
                             : (!acqInfo.partitionOffsets.empty() &&
                                !acqInfo.partitionSizes.empty());
    if (auto blockHint = getPartitioningHint(acquire.getOperation())) {
      if (blockHint->mode == PartitionMode::block && blockHint->blockSize &&
          *blockHint->blockSize > 0) {
        if (!anyBlockSizeHint)
          anyBlockSizeHint = blockHint->blockSize;
        else
          anyBlockSizeHint =
              std::min(*anyBlockSizeHint, *blockHint->blockSize);

        if (acquire.getMode() != ArtsMode::in) {
          if (!writerBlockSizeHint)
            writerBlockSizeHint = blockHint->blockSize;
          else
            writerBlockSizeHint =
                std::min(*writerBlockSizeHint, *blockHint->blockSize);
        }
      }
    }
    bool hasDistributionContract =
        contractSummary ? contractSummary->hasDistributionContract() : false;
    bool thisAcquireCanBlock =
        (acquireMode && (*acquireMode == PartitionMode::block ||
                         *acquireMode == PartitionMode::stencil)) ||
        hasBlockHints || inferredBlock;
    /// If access is indirect and no explicit hints exist, still allow block
    /// partitioning so we can fall back to full-range acquires on a blocked
    /// allocation (improves parallelism vs coarse).
    if (!thisAcquireCanBlock &&
        (hasIndirect || (allocNode && allocNode->hasNonAffineAccesses &&
                         *allocNode->hasNonAffineAccesses))) {
      if (!ctx.totalElements || *ctx.totalElements > 1) {
        thisAcquireCanBlock = true;
        ARTS_DEBUG("  Non-affine/indirect access without hints; enabling "
                   "block capability for full-range acquires");
      }
    }
    if (!thisAcquireCanBlock &&
        ((contractSummary &&
          contractSummary->accessPattern == AccessPattern::Stencil) ||
         (!contractSummary &&
          acqNode->getAccessPattern() == AccessPattern::Stencil)))
      thisAcquireCanBlock = true;
    if (!thisAcquireCanBlock && hasExplicitStencilContract(contractSummary))
      thisAcquireCanBlock = true;
    bool thisAcquireCanElementWise =
        (acquireMode && *acquireMode == PartitionMode::fine_grained) ||
        (contractSummary && contractSummary->hasFineGrainedEntries());

    /// If the partition offset does not map to the access pattern, block
    /// partitioning would select the wrong dimension (non-leading) and is
    /// unsafe. Disable block capability in that case.
    SmallVector<Value> offsetVals = getPartitionOffsetsND(acqNode, &acqInfo);
    unsigned offsetIdx = 0;
    Value partitionOffset =
        DbUtils::pickRepresentativePartitionOffset(offsetVals, &offsetIdx);
    if (partitionOffset) {
      bool hasUnmappedEntry =
          contractSummary && contractSummary->hasUnmappedPartitionEntry();
      if (hasUnmappedEntry) {
        bool preserveFromStencilContract =
            contractSummary &&
            contractSummary->contract.supportsBlockHalo() &&
            contractSummary->contract.hasOwnerDims();
        bool writesThroughAcquire = acquire.getMode() == ArtsMode::out ||
                                    acquire.getMode() == ArtsMode::inout ||
                                    acqNode->hasStores();
        bool preserveFromExplicitContract =
            contractSummary &&
            contractSummary->preservesDistributedContractEntry();
        if (preserveFromExplicitContract || preserveFromStencilContract) {
          ARTS_DEBUG("  Partition offset not mappable by DbAcquireNode, but "
                     "preserving block capability from "
                     << (preserveFromExplicitContract ? "EDT distribution"
                                                      : "N-D stencil halo")
                     << " contract"
                     << (writesThroughAcquire ? " (write acquire)" : ""));
          bool acquireSelectsLeafDb = false;
          if (auto ptrTy = dyn_cast<MemRefType>(acquire.getPtr().getType()))
            acquireSelectsLeafDb = !isa<MemRefType>(ptrTy.getElementType());
          if (acquireSelectsLeafDb)
            preserveExplicitContractLayout = true;
        } else if (writesThroughAcquire) {
          bool hasDirectWriteAccess = contractSummary
                                          ? contractSummary->hasDirectAccess()
                                          : !acqNode->hasIndirectAccess();
          bool canPreserveWriteFullRange =
              !hasIndirect && hasDirectWriteAccess &&
              (!ctx.totalElements || *ctx.totalElements > 1);
          if (canPreserveWriteFullRange) {
            ARTS_DEBUG("  Partition offset incompatible with write access; "
                       "preserving block capability with full-range write "
                       "acquire");
            acqInfo.needsFullRange = true;
          } else {
            ARTS_DEBUG("  Partition offset incompatible with write access; "
                       "disabling block capability");
            thisAcquireCanBlock = false;
          }
        } else {
          ARTS_DEBUG("  Partition offset incompatible with read-only access "
                     "pattern; preserving block capability with full-range");
          acqInfo.needsFullRange = true;
        }
      }
    }

    /// Build AcquireInfo for heuristic voting
    AcquireInfo info;
    info.accessMode = acquire.getMode();
    info.canElementWise =
        thisAcquireCanElementWise || !acquire.getPartitionIndices().empty();
    info.canBlock = canUseBlock && thisAcquireCanBlock;
    info.hasDistributionContract = hasDistributionContract;
    info.partitionDimsFromPeers =
        contractSummary ? contractSummary->partitionDimsFromPeers() : false;
    info.explicitCoarseRequest =
        acquireMode && *acquireMode == PartitionMode::coarse;

    /// Populate unified partition infos from DbAcquireOp.
    /// Heuristics will compute uniformity and decide outerRank.
    info.partitionInfos = acquire.getPartitionInfos();

    /// Populate partition mode from AcquirePartitionInfo
    if (idx < acquireInfos.size()) {
      const auto &acqPartInfo = acquireInfos[idx];
      info.partitionMode = acqPartInfo.mode;
    }

    /// Populate access pattern and dep pattern using canonical ordering.
    info.accessPattern = dbAnalysis.resolveCanonicalAcquireAccessPattern(
        acquire, contractSummary, facts);
    info.depPattern = dbAnalysis.resolveCanonicalAcquireDepPattern(
        acquire, contractSummary, facts);

    if (info.accessPattern == AccessPattern::Unknown &&
        info.accessMode == ArtsMode::in &&
        isStencilFamilyDepPattern(info.depPattern)) {
      info.accessPattern = AccessPattern::Stencil;
    }

    if (!ctx.preferBlockND && acqInfo.partitionSizes.size() >= 2) {
      bool preferContractND = contractSummary &&
                              contractSummary->contract.supportsBlockHalo() &&
                              contractSummary->contract.hasOwnerDims();
      bool preferTiling2D = acquire.getMode() == ArtsMode::inout &&
                            DbAnalysis::isTiling2DTaskAcquire(acquire);
      if (preferContractND || preferTiling2D) {
        SmallVector<Value> candidateBlockSizes;
        unsigned targetRank =
            static_cast<unsigned>(acqInfo.partitionSizes.size());
        if (preferContractND)
          targetRank = std::min<unsigned>(
              targetRank, static_cast<unsigned>(
                              contractSummary->contract.ownerDims.size()));
        if (preferContractND) {
          if (contractSummary &&
              prefersContractNDBlock(contractSummary->contract, targetRank)) {
            auto staticBlockShape =
                contractSummary->contract.getStaticBlockShape();
            candidateBlockSizes.reserve(targetRank);
            for (unsigned dim = 0; dim < targetRank; ++dim) {
              int64_t dimSize = 0;
              Value blockSize = nullptr;
              if (dim < contractSummary->contract.blockShape.size())
                blockSize = contractSummary->contract.blockShape[dim];
              if (blockSize) {
                if (!ValueAnalysis::getConstantIndex(blockSize, dimSize) ||
                    dimSize <= 0) {
                  candidateBlockSizes.clear();
                  break;
                }
              } else if (staticBlockShape &&
                         dim < static_cast<unsigned>(
                                   staticBlockShape->size()) &&
                         (*staticBlockShape)[dim] > 0) {
                dimSize = (*staticBlockShape)[dim];
                blockSize = arts::createConstantIndex(builder, loc, dimSize);
              } else {
                candidateBlockSizes.clear();
                break;
              }
              candidateBlockSizes.push_back(blockSize);
            }
          }
        }
        if (candidateBlockSizes.empty())
          candidateBlockSizes.reserve(targetRank);
        for (unsigned dim = 0; dim < targetRank; ++dim) {
          if (dim < candidateBlockSizes.size())
            continue;
          Value blockSize = acqInfo.partitionSizes[dim];
          if (!blockSize ||
              ValueAnalysis::isZeroConstant(
                  ValueAnalysis::stripNumericCasts(blockSize))) {
            candidateBlockSizes.clear();
            break;
          }
          candidateBlockSizes.push_back(blockSize);
        }
        if (!candidateBlockSizes.empty()) {
          ctx.preferBlockND = true;
          ctx.preferredOuterRank = targetRank;
          blockSizesForNDBlock.assign(candidateBlockSizes.begin(),
                                      candidateBlockSizes.end());
          ARTS_DEBUG("    Acquire[" << idx
                                    << "]: explicit distributed contract "
                                       "prefers N-D block partitioning");
        }
      }
    }

    ctx.acquires.push_back(info);
    ARTS_DEBUG("    Acquire["
               << idx << "]: mode=" << static_cast<int>(info.accessMode)
               << ", canEW=" << info.canElementWise
               << ", canBlock=" << info.canBlock << ", acquireAttr="
               << (acquireMode ? static_cast<int>(*acquireMode) : -1));
    ++idx;
  }

  if (preserveExplicitContractLayout) {
    ARTS_DEBUG("  Preserving explicit block-contract leaf acquire layout; "
               "skipping allocation rewrite");
    heuristics.recordDecision(
        "Partition-PreserveLeafContractLayout", true,
        "leaf acquire uses explicit distributed contract offsets; preserving "
        "layout",
        allocOp.getOperation(), {});
    return true;
  }

  return false;
}

/// Compute stencil halo bounds from acquire nodes and multi-entry structure.
/// Returns the computed StencilInfo, or std::nullopt if stencil mode should
/// be downgraded (e.g. single-sided halo).
std::optional<StencilInfo> DbPartitioningPass::computeStencilHaloInfo(
    DbAllocOp allocOp, DbAllocNode *allocNode,
    ArrayRef<DbAcquireNode *> allocAcquireNodes,
    ArrayRef<std::optional<DbAnalysis::AcquireContractSummary>>
        acquireContractSummaries,
    ArrayRef<const DbAcquirePartitionFacts *> acquireFacts,
    SmallVectorImpl<AcquirePartitionInfo> &acquireInfos,
    bool allocHasStencilPattern, DbAnalysis &dbAnalysis,
    PartitioningDecision &decision, PartitioningContext &ctx,
    OpBuilder &builder, Location loc) {
  StencilInfo info;
  info.haloLeft = 0;
  info.haloRight = 0;

  /// Collect stencil bounds from acquire nodes or multi-entry structure.
  /// For multi-entry stencil acquires (not expanded), extract halo bounds
  /// from the partition indices pattern using DbUtils.
  for (DbAcquireNode *acqNode : allocAcquireNodes) {
    if (!acqNode)
      continue;
    DbAcquireOp acq = acqNode->getDbAcquireOp();
    if (!acq)
      continue;

    /// First try: get bounds from DbAcquireNode analysis
    auto stencilBounds = acqNode->getStencilBounds();
    if (stencilBounds && stencilBounds->hasHalo()) {
      info.haloLeft = std::max(info.haloLeft, stencilBounds->haloLeft());
      info.haloRight = std::max(info.haloRight, stencilBounds->haloRight());
      continue;
    }

    /// Second try: for multi-entry stencil acquires, extract from structure
    if (acq.hasMultiplePartitionEntries()) {
      int64_t minOffset = 0, maxOffset = 0;
      if (DbAnalysis::hasMultiEntryStencilPattern(acq, minOffset,
                                                  maxOffset)) {
        /// minOffset is negative (left halo), maxOffset is positive (right)
        int64_t haloL = -minOffset;
        int64_t haloR = maxOffset;
        info.haloLeft = std::max(info.haloLeft, haloL);
        info.haloRight = std::max(info.haloRight, haloR);
        ARTS_DEBUG("  Extracted stencil bounds from multi-entry: haloLeft="
                   << haloL << ", haloRight=" << haloR);
      }
    }
  }

  bool hasReadStencilAcquire = false;
  for (auto [acqNode, contractSummary, acqFacts] : llvm::zip_equal(
           allocAcquireNodes, acquireContractSummaries, acquireFacts)) {
    if (!acqNode)
      continue;
    DbAcquireOp acquire = acqNode->getDbAcquireOp();
    if (!acquire || acquire.getMode() != ArtsMode::in)
      continue;
    const DbAcquirePartitionFacts *facts =
        acqFacts ? acqFacts : &acqNode->getPartitionFacts();
    if (dbAnalysis.hasCanonicalAcquireStencilSemantics(
            acquire, contractSummary ? &*contractSummary : nullptr, facts) ||
        allocHasStencilPattern) {
      hasReadStencilAcquire = true;
      break;
    }
  }

  auto allocPattern = getDbAccessPattern(allocOp.getOperation());
  if ((info.haloLeft == 0 || info.haloRight == 0) && hasReadStencilAcquire &&
      allocHasStencilPattern) {
    if (allocPattern && *allocPattern == DbAccessPattern::stencil) {
      info.haloLeft = std::max<int64_t>(info.haloLeft, 1);
      info.haloRight = std::max<int64_t>(info.haloRight, 1);
      ARTS_DEBUG("  Applying stencil halo fallback from db_alloc access "
                 "pattern: haloLeft="
                 << info.haloLeft << ", haloRight=" << info.haloRight);
    }
  }

  /// Get total rows from first element dimension.
  /// For stencil loops, the logical owned span excludes halo rows, so use
  /// (rows - haloLeft - haloRight) when possible.
  if (!allocOp.getElementSizes().empty()) {
    if (auto staticSize =
            arts::extractBlockSizeFromHint(allocOp.getElementSizes()[0]))
      info.totalRows = arts::createConstantIndex(builder, loc, *staticSize);
    else
      info.totalRows = allocOp.getElementSizes()[0];

    if (info.totalRows && (info.haloLeft > 0 || info.haloRight > 0)) {
      if (!info.totalRows.getType().isIndex())
        info.totalRows = builder.create<arith::IndexCastOp>(
            loc, builder.getIndexType(), info.totalRows);

      Value haloTotal = arts::createConstantIndex(
          builder, loc, info.haloLeft + info.haloRight);
      Value zero = arts::createZeroIndex(builder, loc);
      Value canSubtract = builder.create<arith::CmpIOp>(
          loc, arith::CmpIPredicate::uge, info.totalRows, haloTotal);
      Value reduced =
          builder.create<arith::SubIOp>(loc, info.totalRows, haloTotal);
      info.totalRows =
          builder.create<arith::SelectOp>(loc, canSubtract, reduced, zero);
    }
  }

  ARTS_DEBUG("  Stencil mode (early): haloLeft="
             << info.haloLeft << ", haloRight=" << info.haloRight);

  /// Check: Require both halos for true ESD stencil mode.
  /// Single-sided patterns (only left or only right halo) break the
  /// 3-buffer approach which assumes data from both neighbors exists.
  if (info.haloLeft == 0 || info.haloRight == 0) {
    ARTS_DEBUG("  Single-sided halo detected (haloLeft="
               << info.haloLeft << ", haloRight=" << info.haloRight
               << ") - falling back to BLOCK mode");
    decision = PartitioningDecision::blockND(
        ctx, decision.outerRank,
        "Single-sided stencil pattern - using BLOCK instead");
    return std::nullopt;
  }

  return info;
}

/// Resolve block-plan sizes and partition dimensions, and reconcile
/// per-acquire dimension compatibility. Returns false if the caller should
/// early-return the original allocOp (block plan resolution failed).
bool DbPartitioningPass::resolveBlockPlanSizes(
    SmallVectorImpl<Value> &blockSizesForPlan,
    SmallVectorImpl<unsigned> &partitionedDimsForPlan,
    SmallVectorImpl<AcquirePartitionInfo> &acquireInfos,
    SmallVectorImpl<Value> &blockSizesForNDBlock,
    PartitioningDecision &decision, PartitioningContext &ctx,
    std::optional<StencilInfo> &stencilInfo, DbAllocOp allocOp,
    DbAllocNode *allocNode, DbHeuristics &heuristics, OpBuilder &builder,
    Location loc) {
  bool useNodesForFallback = false;
  if (AM) {
    AbstractMachine &machine = AM->getAbstractMachine();
    if (machine.hasValidNodeCount() && machine.getNodeCount() > 1) {
      /// Stencil ESD requires block-aligned worker chunk offsets.
      /// Worker chunks are produced by EDT lowering using worker-level
      /// decomposition. Keeping stencil fallback sizing worker-based avoids
      /// misalignment (e.g. node-sized blocks with worker-sized tasks) that
      /// would otherwise force DbStencilRewriter into block fallback.
      useNodesForFallback = (decision.mode != PartitionMode::stencil);
    }
  }

  DbBlockPlanInput blockPlanInput{allocOp,
                                  acquireInfos,
                                  ctx.blockSize,
                                  /*dynamicBlockSizeHint=*/Value(),
                                  blockSizesForNDBlock,
                                  &builder,
                                  loc,
                                  useNodesForFallback};
  auto blockPlanOr = resolveDbBlockPlan(blockPlanInput);
  if (failed(blockPlanOr) || blockPlanOr->blockSizes.empty()) {
    ARTS_DEBUG("  No block-plan sizes available - falling back to coarse");
    heuristics.recordDecision(
        "Partition-NoBlockSize", false,
        "block mode requested but no block-plan sizes available",
        allocOp.getOperation(), {});
    resetCoarseAcquireRanges(allocOp, allocNode, builder);
    return false;
  }

  blockSizesForPlan.assign(std::make_move_iterator(blockPlanOr->blockSizes.begin()),
                           std::make_move_iterator(blockPlanOr->blockSizes.end()));
  partitionedDimsForPlan.assign(
      std::make_move_iterator(blockPlanOr->partitionedDims.begin()),
      std::make_move_iterator(blockPlanOr->partitionedDims.end()));

  heuristics.recordDecision(
      "Partition-BlockPlanResolved", true,
      "resolved block sizes and partition dimensions via block-plan resolver",
      allocOp.getOperation(),
      {{"blockDims", static_cast<int64_t>(blockSizesForPlan.size())},
       {"partitionedDims",
        static_cast<int64_t>(partitionedDimsForPlan.size())}});

  /// Non-leading partitioning is not supported by stencil ESD. Downgrade
  /// to block mode when needed.
  if (decision.mode == PartitionMode::stencil &&
      !partitionedDimsForPlan.empty()) {
    bool nonLeading = false;
    for (unsigned i = 0; i < partitionedDimsForPlan.size(); ++i) {
      if (partitionedDimsForPlan[i] != i) {
        nonLeading = true;
        break;
      }
    }
    if (nonLeading) {
      decision = PartitioningDecision::blockND(
          ctx, partitionedDimsForPlan.size(),
          "Non-leading partition dims: disable stencil ESD");
      stencilInfo = std::nullopt;
    }
  }

  /// If an acquire's inferred partition dims disagree with the chosen plan,
  /// require full-range to preserve correctness.
  ///
  /// Allow acquires to carry extra trailing partition dimensions as long as
  /// the plan dims are a leading prefix. This is common when a 2D acquire
  /// participates in a 1D block plan (partition along rows only).
  if (!partitionedDimsForPlan.empty()) {
    for (auto &info : acquireInfos) {
      if (info.needsFullRange)
        continue;
      if (info.partitionDims.empty()) {
        info.needsFullRange = true;
        ARTS_DEBUG("  Acquire missing partition dims under concrete plan; "
                   "forcing full-range access");
        continue;
      }
      bool compatible =
          info.partitionDims.size() >= partitionedDimsForPlan.size();
      if (compatible) {
        for (unsigned i = 0; i < partitionedDimsForPlan.size(); ++i) {
          if (info.partitionDims[i] != partitionedDimsForPlan[i]) {
            compatible = false;
            break;
          }
        }
      }
      if (!compatible) {
        info.needsFullRange = true;
        ARTS_DEBUG("  Acquire partition dims mismatch with plan; "
                   "forcing full-range access");
      }
    }
  }

  return true;
}

/// Assemble the rewrite plan from partition decision and apply it via
/// DbRewriter. Returns the rewritten DbAllocOp on success.
FailureOr<DbAllocOp> DbPartitioningPass::assembleAndApplyRewritePlan(
    PartitioningDecision &decision,
    SmallVectorImpl<AcquirePartitionInfo> &acquireInfos,
    SmallVectorImpl<Value> &blockSizesForPlan,
    SmallVectorImpl<unsigned> &partitionedDimsForPlan,
    SmallVectorImpl<Value> &newOuterSizes,
    SmallVectorImpl<Value> &newInnerSizes,
    std::optional<StencilInfo> &stencilInfo, DbAllocOp allocOp,
    DbHeuristics &heuristics, OpBuilder &builder) {
  DbRewritePlan plan(decision.mode);
  plan.blockSizes.assign(blockSizesForPlan.begin(), blockSizesForPlan.end());
  plan.partitionedDims.assign(partitionedDimsForPlan.begin(),
                              partitionedDimsForPlan.end());
  plan.outerSizes.assign(newOuterSizes.begin(), newOuterSizes.end());
  plan.innerSizes.assign(newInnerSizes.begin(), newInnerSizes.end());

  ARTS_DEBUG("  Plan created: blockSizes="
             << plan.blockSizes.size() << ", outerRank=" << plan.outerRank()
             << ", innerRank=" << plan.innerRank()
             << ", numPartitionedDims=" << plan.numPartitionedDims());

  /// Mixed mode: set flag for full-range acquires
  bool hasMixedMode = llvm::any_of(
      acquireInfos, [](const auto &info) { return info.needsFullRange; });
  if (hasMixedMode && decision.isBlock()) {
    plan.isMixedMode = true;
    ARTS_DEBUG("  Mixed mode plan enabled");
  }

  /// ESD: Use early-computed stencilInfo for Stencil mode
  if (stencilInfo)
    plan.stencilInfo = *stencilInfo;
  /// Note: byte_offset/byte_size for ESD is computed in EdtLowering from
  /// element_offsets/element_sizes, using allocation's elementSizes for stride.

  /// Propagate stencil center offset to all acquires when using stencil mode.
  /// This keeps base offsets aligned for non-stencil accesses (e.g., outputs)
  /// when loop bounds are shifted (i starts at 1).
  if (decision.mode == PartitionMode::stencil) {
    std::optional<int64_t> centerOffset;
    for (auto &info : acquireInfos) {
      if (!info.acquire)
        continue;
      if (auto center = getStencilCenterOffset(info.acquire.getOperation())) {
        centerOffset = *center;
        break;
      }
    }
    if (centerOffset) {
      for (auto &info : acquireInfos) {
        if (!info.acquire)
          continue;
        if (!getStencilCenterOffset(info.acquire.getOperation()))
          setStencilCenterOffset(info.acquire.getOperation(), *centerOffset);
      }
    }
  }

  /// Build DbRewriteAcquire list from AcquirePartitionInfo
  /// Include ALL acquires - invalid ones get Coarse fallback (offset=0,
  /// size=totalSize)
  SmallVector<DbRewriteAcquire> rewriteAcquires;
  for (auto &info : acquireInfos) {
    if (!info.acquire)
      continue;

    DbRewriteAcquire rewriteInfo;
    rewriteInfo.acquire = info.acquire;
    rewriteInfo.partitionInfo.mode = decision.mode;

    if (info.acquire.hasMultiplePartitionEntries()) {
      ARTS_DEBUG("  Rewrite multi-entry acquire offset="
                 << (info.partitionOffsets.empty()
                         ? Value()
                         : info.partitionOffsets.front())
                 << " loc=" << info.acquire.getLoc());
    }

    DbAcquirePartitionView view;
    view.acquire = info.acquire;
    auto partitionIndices = info.acquire.getPartitionIndices();
    view.partitionIndices.assign(partitionIndices.begin(),
                                 partitionIndices.end());
    view.partitionOffsets.assign(info.partitionOffsets.begin(),
                                 info.partitionOffsets.end());
    view.partitionSizes.assign(info.partitionSizes.begin(),
                               info.partitionSizes.end());
    view.accessPattern = info.accessPattern;
    view.isValid = info.isValid;
    view.hasIndirectAccess = info.hasIndirectAccess;
    view.needsFullRange = info.needsFullRange;
    buildRewriteAcquire(decision.mode, view, allocOp, plan, rewriteInfo,
                        builder);

    rewriteAcquires.push_back(rewriteInfo);
  }

  auto rewriter = DbRewriter::create(allocOp, rewriteAcquires, plan);
  auto result = rewriter->apply(builder);

  /// Set partition attribute on new alloc
  if (succeeded(result)) {
    setPartitionMode(*result, decision.mode);

    AM->getMetadataManager().transferMetadata(allocOp, result.value());

    /// EXT-PART-5: Write resolved partition decisions back to the
    /// LoweringContractOp on the alloc's ptr so downstream passes can read
    /// the block shape, owner dims, and distribution version directly.
    if (!decision.isCoarse()) {
      feedbackPartitionDecisionToContract(result.value(), blockSizesForPlan,
                                          partitionedDimsForPlan, decision.mode,
                                          builder);
    }

    /// Coarse allocation: clear partition hints on acquires to avoid
    /// unnecessary per-task hint plumbing and enable invariant hoisting.
    if (decision.isCoarse()) {
      for (auto &info : rewriteAcquires) {
        if (!info.acquire)
          continue;
        info.acquire.clearPartitionHints();
      }
    }

    /// Record successful partition decision
    StringRef modeName = getPartitionModeName(decision.mode);
    ARTS_DEBUG("  Set partition attribute: " << modeName);
    heuristics.recordDecision(
        "Partition-Success", true,
        ("successfully partitioned allocation to " + modeName).str(),
        result.value().getOperation(),
        {{"outerRank", static_cast<int64_t>(decision.outerRank)},
         {"innerRank", static_cast<int64_t>(decision.innerRank)},
         {"acquireCount", static_cast<int64_t>(acquireInfos.size())}});
  } else {
    /// Record partition failure
    heuristics.recordDecision(
        "Partition-RewriterFailed", false,
        "DbRewriter failed to apply partition transformation",
        allocOp.getOperation(), {});
  }

  return result;
}

void DbPartitioningPass::analyzeStencilBounds() {
  ARTS_DEBUG_HEADER(AnalyzeStencilBounds);
  lowerStencilAcquireBounds(module, AM->getLoopAnalysis());

  ARTS_DEBUG_FOOTER(AnalyzeStencilBounds);
}

void DbPartitioningPass::invalidateAndRebuildGraph() {
  AM->invalidateAndRebuildGraphs(module);
}

namespace mlir {
namespace arts {
std::unique_ptr<Pass>
createDbPartitioningPass(mlir::arts::AnalysisManager *AM) {
  return std::make_unique<DbPartitioningPass>(AM);
}
} // namespace arts
} // namespace mlir
