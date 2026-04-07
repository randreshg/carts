///==========================================================================///
/// File: DbPartitioningSupport.cpp
///
/// Shared local helpers for the split DbPartitioning implementation.
///==========================================================================///

#include "arts/passes/opt/db/DbPartitioningInternal.h"

using namespace mlir;
using namespace mlir::func;
using namespace mlir::arith;
using namespace mlir::polygeist;
using namespace mlir::arts;

ARTS_DEBUG_SETUP(db_partitioning);

/// Statistics are defined in DbPartitioning.cpp — reference them here.
extern llvm::Statistic numAcquiresForcedFullRange;
extern llvm::Statistic numStencilBoundsGuardsInserted;

namespace mlir::arts::db_partitioning {

///===----------------------------------------------------------------------===//
/// Tracing
///===----------------------------------------------------------------------===//

bool dbPartitionTraceEnabled() {
  static const bool enabled =
      std::getenv("CARTS_TRACE_DB_PARTITION") != nullptr;
  return enabled;
}

void dbPartitionTraceImpl(llvm::function_ref<void(llvm::raw_ostream &)> fn) {
  if (!dbPartitionTraceEnabled())
    return;
  ARTS_DEBUG_REGION(ARTS_DBGS() << "[db_partition_trace] "; fn(ARTS_DBGS());
                    ARTS_DBGS() << "\n";);
}

///===----------------------------------------------------------------------===//
/// Helper Functions
///===----------------------------------------------------------------------===//

bool markAcquireNeedsFullRange(AcquirePartitionInfo &info) {
  if (info.needsFullRange)
    return false;
  info.needsFullRange = true;
  ++numAcquiresForcedFullRange;
  return true;
}

/// DbBlockPlanResolver/DbPartitionPlanner model leading-dimension block plans
/// in two equivalent forms:
///   1. explicit  : partitionedDims = [0, 1, ...]
///   2. implicit  : partitionedDims = [] and blockSizes map to leading dims
///
/// Mixed block+stencil lowering should accept either form. Rejecting the
/// implicit form forces otherwise-stencil-compatible block plans down the pure
/// block rewrite path even though the planner and shape computation can handle
/// them already.
bool isLeadingPrefixPartitionPlan(ArrayRef<unsigned> partitionedDims,
                                  size_t numPartitionedDims,
                                  size_t elementRank) {
  if (numPartitionedDims == 0 || numPartitionedDims > elementRank)
    return false;

  if (partitionedDims.empty())
    return true;

  if (partitionedDims.size() != numPartitionedDims)
    return false;

  for (unsigned i = 0; i < partitionedDims.size(); ++i) {
    if (partitionedDims[i] != i)
      return false;
  }
  return true;
}

bool blockSizeCoversFullExtent(Value blockSize, Value extent) {
  blockSize = ValueAnalysis::stripNumericCasts(blockSize);
  extent = ValueAnalysis::stripNumericCasts(extent);
  if (!blockSize || !extent)
    return false;

  if (ValueAnalysis::sameValue(blockSize, extent) ||
      ValueAnalysis::areValuesEquivalent(blockSize, extent))
    return true;

  auto blockConst = ValueAnalysis::tryFoldConstantIndex(blockSize);
  auto extentConst = ValueAnalysis::tryFoldConstantIndex(extent);
  return blockConst && extentConst && *blockConst >= *extentConst;
}

std::optional<std::pair<StencilBounds, unsigned>>
getSingleOwnerContractStencilBounds(
    const DbAnalysis::AcquireContractSummary *summary) {
  if (!summary)
    return std::nullopt;

  const LoweringContractInfo &contract = summary->contract;
  if (!contract.supportsBlockHalo() || contract.spatial.ownerDims.size() != 1)
    return std::nullopt;

  int64_t ownerDim = contract.spatial.ownerDims.front();
  if (ownerDim < 0)
    return std::nullopt;

  auto minOffsets = contract.getStaticMinOffsets();
  auto maxOffsets = contract.getStaticMaxOffsets();
  if (!minOffsets || !maxOffsets || minOffsets->size() != 1 ||
      maxOffsets->size() != 1)
    return std::nullopt;

  StencilBounds bounds = StencilBounds::create(
      (*minOffsets)[0], (*maxOffsets)[0], /*stencil=*/true, /*isValid=*/true);
  if (!bounds.hasHalo())
    return std::nullopt;

  return std::make_pair(bounds, static_cast<unsigned>(ownerDim));
}

/// Block plans can preserve the semantic owner rank from a stencil/uniform
/// contract even when a trailing physical partition dim spans the full extent
/// of the underlying memref. Carrying that singleton outer dim into the DB
/// layout only adds an extra level of db_ref indexing and can prevent later
/// 1-D-owner stencil rewrites from recognizing the simpler physical shape.
///
/// Keep the semantic owner rank in lowering contracts, but collapse trailing
/// physical partition dims whose block size already covers the full extent.
void collapseTrailingFullExtentPartitionDims(
    DbAllocOp allocOp, SmallVectorImpl<Value> &blockSizes,
    SmallVectorImpl<unsigned> &partitionedDims) {
  auto elementSizes = allocOp.getElementSizes();
  if (blockSizes.size() <= 1)
    return;

  while (blockSizes.size() > 1) {
    unsigned dim = partitionedDims.empty() ? blockSizes.size() - 1
                                           : partitionedDims.back();
    if (dim >= elementSizes.size())
      break;
    if (!blockSizeCoversFullExtent(blockSizes.back(), elementSizes[dim]))
      break;

    ARTS_DEBUG("  Collapsing trailing full-extent partition dim " << dim
                                                                  << " from "
                                                                     "block "
                                                                     "plan");
    blockSizes.pop_back();
    if (!partitionedDims.empty())
      partitionedDims.pop_back();
    /// Implicit leading-dimension plans keep `partitionedDims` empty. Popping
    /// only the trailing block size is sufficient because downstream shape
    /// computation interprets the remaining entries as the surviving leading
    /// partitioned dims.
    if (partitionedDims.empty())
      continue;
  }
}

std::optional<int64_t> computeStaticElementCount(DbAllocOp allocOp) {
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
  return staticElementCount;
}

/// Validate that element-wise partition indices match actual EDT accesses.
/// Returns false if this is a block-wise pattern (indices are block corners,
/// but accesses span a range) - indicating we should fall back to block mode.
///
/// Detection criteria for block-wise pattern:
/// 1. Partition hints have indices[] (looks like element-wise)
/// 2. BUT the enclosing loop steps by > 1 (BLOCK_SIZE)
/// 3. OR partition indices don't appear in EDT body accesses
bool validateElementWiseIndices(ArrayRef<AcquirePartitionInfo> acquireInfos,
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
/// Use front() on the result for 1-D callers.
SmallVector<Value> getPartitionOffsetsND(DbAcquireNode *acqNode,
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

bool hasInternodeAcquireUser(ArrayRef<DbAcquireNode *> acquireNodes) {
  for (DbAcquireNode *acqNode : acquireNodes) {
    if (!acqNode)
      continue;
    EdtOp edt = acqNode->getEdtUser();
    if (edt && edt.getConcurrency() == EdtConcurrency::internode)
      return true;
  }
  return false;
}

AcquirePatternSummary
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

    /// If proof.partition_access_mapping is proven on the source contract,
    /// trust the contract's pattern directly without graph re-derivation.
    Value sourcePtr = acquire.getSourcePtr();
    LoweringContractOp contractOp = getLoweringContractOp(sourcePtr);
    if (contractOp) {
      OwnershipProof proof = readOwnershipProof(contractOp.getOperation());
      if (proof.partitionAccessMapping) {
        sawSummary = true;
        auto depPattern = contractOp.getDepPattern();
        if (depPattern && isStencilFamilyDepPattern(*depPattern))
          summary.hasStencil = true;
        else if (depPattern && isUniformFamilyDepPattern(*depPattern))
          summary.hasUniform = true;
        dbPartitionTraceImpl([&](llvm::raw_ostream &os) {
          os << "Proof-trusted access mapping for acquire "
             << acquire.getOperation()->getLoc();
        });
        continue;
      }
    }

    auto contractSummary = dbAnalysis->getAcquireContractSummary(acquire);
    if (!contractSummary)
      continue;
    sawSummary = true;

    AccessPattern accessPattern = contractSummary->accessPattern;
    if (accessPattern == AccessPattern::Unknown)
      accessPattern = contractSummary->getDerivedAccessPattern();

    switch (accessPattern) {
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
    if (contractSummary->hasExplicitStencilLayout())
      summary.hasStencil = true;
    if (contractSummary->usesMatmulSemantics())
      summary.hasUniform = true;
  }

  if (sawSummary)
    return summary;
  return allocNode ? allocNode->summarizeAcquirePatterns()
                   : AcquirePatternSummary{};
}

bool isAcquireInfoConsistent(const AcquirePartitionInfo &lhs,
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

AcquireModeReconcileResult
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
        markAcquireNeedsFullRange(info);
    }
  } else if (hasCoarse) {
    ctx.canElementWise = false;
  }

  return result;
}

bool isLowerBoundGuaranteedByControlFlow(Operation *op, Value loopIV) {
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

void applyBoundsValid(DbAcquireOp acquireOp, ArrayRef<int64_t> boundsCheckFlags,
                      Value loopIV) {
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
  ++numStencilBoundsGuardsInserted;
  acquireOp.erase();
}

void lowerStencilAcquireBounds(ModuleOp module, LoopAnalysis &loopAnalysis) {
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
AcquirePartitionInfo
computeAcquirePartitionInfo(DbAnalysis &dbAnalysis, DbAcquireOp acquire,
                            const DbAnalysis::AcquireContractSummary *summary,
                            OpBuilder &builder) {
  AcquirePartitionInfo info;
  info.acquire = acquire;
  DbAnalysis::AcquirePartitionSummary partitionSummary;
  partitionSummary =
      dbAnalysis.analyzeAcquirePartition(acquire, builder, summary);
  info.mode = partitionSummary.mode;
  info.partitionOffsets.assign(partitionSummary.partitionOffsets.begin(),
                               partitionSummary.partitionOffsets.end());
  info.partitionSizes.assign(partitionSummary.partitionSizes.begin(),
                             partitionSummary.partitionSizes.end());
  info.partitionDims.assign(partitionSummary.partitionDims.begin(),
                            partitionSummary.partitionDims.end());
  info.accessPattern =
      dbAnalysis.resolveCanonicalAcquireAccessPattern(acquire, summary);
  info.isValid = partitionSummary.isValid;
  info.hasIndirectAccess = summary ? summary->hasIndirectAccess()
                                   : partitionSummary.hasIndirectAccess;
  info.hasDistributionContract = summary
                                     ? summary->hasDistributionContract()
                                     : partitionSummary.hasDistributionContract;
  info.preservesDepMode = static_cast<bool>(acquire.getPreserveAccessMode());

  /// ownerDims describe which physical memref dims participate in the layout.
  /// They must remain authoritative even when the acquire does not opt into
  /// halo transport, otherwise non-leading stencil contracts collapse back to
  /// implicit leading-dim plans during block-plan resolution.
  if (summary && !summary->contract.spatial.ownerDims.empty()) {
    unsigned contractRank = 0;
    for (int64_t dim : summary->contract.spatial.ownerDims) {
      if (dim >= 0)
        contractRank =
            std::max<unsigned>(contractRank, static_cast<unsigned>(dim) + 1);
    }
    if (contractRank == 0)
      contractRank =
          static_cast<unsigned>(summary->contract.spatial.ownerDims.size());

    SmallVector<unsigned, 4> contractDims =
        resolveContractOwnerDims(summary->contract, contractRank);
    if (!contractDims.empty()) {
      if (!info.partitionOffsets.empty() &&
          contractDims.size() > info.partitionOffsets.size())
        contractDims.resize(info.partitionOffsets.size());
      info.partitionDims.assign(contractDims.begin(), contractDims.end());
    }

    if (info.mode == PartitionMode::coarse &&
        (summary->hasBlockHints() || summary->inferredBlock()))
      info.mode = summary->preferredBlockPartitionMode();
  }

  return info;
}

unsigned deriveAuthoritativeOwnerBlockRank(
    DbAllocOp allocOp, ArrayRef<AcquirePartitionInfo> acquireInfos,
    ArrayRef<std::optional<DbAnalysis::AcquireContractSummary>>
        acquireContractSummaries,
    ArrayRef<AcquireInfo> heuristicInfos) {
  unsigned maxSupportedRank = allocOp.getElementSizes().size();
  if (maxSupportedRank == 0)
    return 0;

  SmallVector<bool> seenOwnerDims(maxSupportedRank, false);
  unsigned distinctOwnerDims = 0;

  for (size_t acquireIdx = 0; acquireIdx < acquireInfos.size() &&
                              acquireIdx < acquireContractSummaries.size() &&
                              acquireIdx < heuristicInfos.size();
       ++acquireIdx) {
    const auto &acqInfo = acquireInfos[acquireIdx];
    const auto &summary = acquireContractSummaries[acquireIdx];
    const auto &heuristicInfo = heuristicInfos[acquireIdx];

    if (!summary || acqInfo.needsFullRange || !heuristicInfo.canBlock ||
        summary->partitionDimsFromPeers())
      continue;

    ArrayRef<unsigned> authoritativeDims = acqInfo.partitionDims;
    if (authoritativeDims.empty())
      authoritativeDims = summary->partitionDims;
    if (authoritativeDims.empty())
      continue;

    unsigned explicitRank = std::min<unsigned>(acqInfo.partitionOffsets.size(),
                                               acqInfo.partitionSizes.size());
    if (explicitRank > authoritativeDims.size())
      continue;

    for (unsigned dim : authoritativeDims) {
      if (dim >= maxSupportedRank)
        continue;
      if (!seenOwnerDims[dim]) {
        seenOwnerDims[dim] = true;
        ++distinctOwnerDims;
      }
    }
  }

  return distinctOwnerDims;
}

void resetCoarseAcquireRanges(DbAllocOp allocOp, DbAllocNode *allocNode,
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
    /// A coarse fallback invalidates any previously materialized element-space
    /// slice. Leaving element_offsets/element_sizes behind would later
    /// re-create byte-sliced dependencies for an acquire whose DB-space
    /// contract is now whole-range.
    acqOp.getElementOffsetsMutable().clear();
    acqOp.getElementSizesMutable().clear();
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
/// ownerDims, and post-DB refinement marker, then upserts the updated
/// contract.
/// Returns true if a contract was updated.
bool feedbackPartitionDecisionToContract(DbAllocOp newAllocOp,
                                         ArrayRef<Value> blockSizes,
                                         ArrayRef<unsigned> partitionedDims,
                                         PartitionMode mode,
                                         OpBuilder &builder) {
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

  SmallVector<unsigned, 4> effectivePartitionDims;
  if (!partitionedDims.empty()) {
    effectivePartitionDims.assign(partitionedDims.begin(),
                                  partitionedDims.end());
  } else if (blockSizes.size() > 1) {
    effectivePartitionDims.reserve(blockSizes.size());
    for (unsigned dim = 0; dim < blockSizes.size(); ++dim)
      effectivePartitionDims.push_back(dim);
  }

  /// Populate ownerDims from the resolved plan. Implicit leading-prefix N-D
  /// plans are canonical block layouts too; materialize them explicitly in the
  /// contract so downstream consumers do not have to rediscover them from the
  /// rewritten allocation shape alone.
  if (!effectivePartitionDims.empty() &&
      (!info.hasExplicitStencilContract() || info.spatial.ownerDims.empty())) {
    info.spatial.ownerDims.clear();
    info.spatial.ownerDims.reserve(effectivePartitionDims.size());
    for (unsigned dim : effectivePartitionDims)
      info.spatial.ownerDims.push_back(static_cast<int64_t>(dim));
  }

  /// Populate resolved block shape from the block-plan resolver output.
  if (!blockSizes.empty() &&
      blockSizes.size() == info.spatial.ownerDims.size()) {
    info.spatial.blockShape.assign(blockSizes.begin(), blockSizes.end());
  }

  /// Mark as post-DB refined so consumers can distinguish contracts that
  /// survived partitioning from pre-partitioning seeds.
  info.analysis.postDbRefined = true;

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
             << ", ownerDims=" << partitionedDims.size() << ")");
  return true;
}

///===----------------------------------------------------------------------===//
/// Multi-entry Acquire Expansion Helpers
///===----------------------------------------------------------------------===//

SmallVector<DbAcquireOp> createExpandedAcquires(DbAcquireOp original,
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

SmallVector<Value> rebuildEdtDeps(EdtOp edt, DbAcquireOp original,
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

SmallVector<BlockArgument> insertExpandedBlockArgs(EdtOp edt,
                                                   BlockArgument originalArg,
                                                   size_t numEntries,
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

SmallVector<DbRefOp> collectDbRefUsers(BlockArgument arg) {
  SmallVector<DbRefOp> refs;
  for (Operation *user : arg.getUsers())
    if (auto dbRef = dyn_cast<DbRefOp>(user))
      refs.push_back(dbRef);
  return refs;
}

SmallVector<Value> collectAccessIndices(DbRefOp dbRef) {
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

SmallVector<Value> collectAccessIndicesFromUser(Operation *refUser) {
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

SmallVector<Value> getEntryAnchors(DbAcquireOp original, size_t entry) {
  SmallVector<Value> anchors = original.getPartitionIndicesForEntry(entry);
  if (!anchors.empty())
    return anchors;
  return original.getPartitionOffsetsForEntry(entry);
}

int indexMatchStrength(Value idx, Value anchor) {
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

int scoreEntry(ArrayRef<Value> indices, ArrayRef<Value> anchors) {
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

EntryMatch matchDbRefEntry(DbRefOp dbRef, ArrayRef<Value> accessIndices,
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

} // namespace mlir::arts::db_partitioning
