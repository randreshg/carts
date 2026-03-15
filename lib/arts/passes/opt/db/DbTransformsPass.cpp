///==========================================================================///
/// File: DbTransformsPass.cpp
///
/// Post-DB transforms pass. Hosts DB-level transformations that run after
/// DbDistributedOwnership and before the final DbModeTightening mode
/// tightening.
///
/// Current transforms:
///   DT-1: Contract persistence -- persist refined AcquireContractSummary
///         back to IR via upsertLoweringContract().
///   DT-2: Stencil halo consolidation -- unify halo bounds from graph
///         analysis, raw IR attrs, and contract into unified min/max offsets.
///   DT-3: ESD annotation -- for block/stencil partitioned acquires with
///         static partition info, annotate esdByteOffset/esdByteSize in the
///         contract so lowering can emit arts_add_dependence_at.
///   DT-7: Block window caching -- for block-partitioned acquires with
///         static partition and block size info, cache startBlock and
///         blockCount in the contract so lowering skips element-to-block
///         mapping recomputation.
/// Placeholder transform slots:
///   DT-4: GUID range allocation
///   DT-5: DB replication (artsAddDbDuplicate for read-heavy DBs)
///   DT-6: Atomic reduction recognition (artsAtomicAddInArrayDb)
///
///==========================================================================///

/// Dialects
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/Pass/Pass.h"
/// Arts
#include "arts/Dialect.h"
#include "arts/analysis/AnalysisManager.h"
#include "arts/analysis/db/DbAnalysis.h"
#include "arts/analysis/graphs/db/DbGraph.h"
#include "arts/analysis/graphs/db/DbNode.h"
#include "arts/analysis/value/ValueAnalysis.h"
#include "arts/passes/PassDetails.h"
#include "arts/passes/Passes.h"
#include "arts/transforms/db/transforms/GUIDRangeDetection.h"
#include "arts/utils/DbUtils.h"
#include "arts/utils/LoweringContractUtils.h"
#include "arts/utils/OperationAttributes.h"
#include "arts/utils/StencilAttributes.h"
#include "arts/utils/Utils.h"
/// Debug
#include "arts/utils/Debug.h"
ARTS_DEBUG_SETUP(db_transforms);

using namespace mlir;
using namespace mlir::func;
using namespace mlir::arts;

namespace {
struct DbTransformsPass : public arts::DbTransformsBase<DbTransformsPass> {
  DbTransformsPass(mlir::arts::AnalysisManager *AM) : AM(AM) {
    assert(AM && "AnalysisManager must be provided externally");
  }

  void runOnOperation() override;

private:
  mlir::arts::AnalysisManager *AM = nullptr;

  /// DT-1: Contract persistence
  unsigned persistContracts();

  /// DT-2: Stencil halo consolidation
  unsigned consolidateStencilHalos();

  /// DT-3: ESD annotation
  unsigned annotateESD();

  /// DT-7: Block window caching
  unsigned cacheBlockWindows();
};
} // namespace

void DbTransformsPass::runOnOperation() {
  ARTS_INFO_HEADER(DbTransformsPass);

  ///===------------------------------------------------------------------===///
  /// DT-1: Contract persistence
  ///
  /// Walk all DbAcquireOps, query DbAnalysis for the refined
  /// AcquireContractSummary, and when refinedByDbAnalysis is true persist
  /// the contract back to IR via upsertLoweringContract(). This ensures
  /// downstream lowering passes see the refined depPattern, ownerDims, etc.
  ///===------------------------------------------------------------------===///
  unsigned dt1Count = persistContracts();
  if (dt1Count > 0)
    ARTS_INFO("DT-1: persisted " << dt1Count << " refined contracts");

  ///===------------------------------------------------------------------===///
  /// DT-2: Stencil halo consolidation
  ///
  /// Unify halo bounds from graph analysis, raw IR attrs, and the existing
  /// LoweringContractOp into the contract's minOffsets/maxOffsets. After
  /// consolidation, remove redundant raw stencil attrs from the acquire op.
  ///===------------------------------------------------------------------===///
  unsigned dt2Count = consolidateStencilHalos();
  if (dt2Count > 0)
    ARTS_INFO("DT-2: consolidated stencil halos on " << dt2Count
                                                     << " acquires");

  ///===------------------------------------------------------------------===///
  /// DT-3: ESD annotation
  ///
  /// For each acquire in stencil/block partitioned DBs, analyze EDT's
  /// actual access footprint. If it touches only a subset, annotate
  /// contract with esdByteOffset/esdByteSize so lowering emits
  /// arts_add_dependence_at.
  ///===------------------------------------------------------------------===///
  unsigned dt3Count = annotateESD();
  if (dt3Count > 0)
    ARTS_INFO("DT-3: annotated ESD on " << dt3Count << " acquires");

  ///===------------------------------------------------------------------===///
  /// DT-4: GUID range allocation
  ///
  /// Detect loops creating N GUIDs and mark them for batch allocation
  /// via arts_guid_reserve_range(N). The marking pass stamps IR with
  /// attributes that ConvertArtsToLLVM can consume.
  ///===------------------------------------------------------------------===///
  {
    ModuleOp module = getOperation();
    bool dt4Found = detectGUIDRangeCandidates(module);
    if (dt4Found)
      ARTS_INFO("DT-4: marked GUID range allocation candidates");
  }

  ///===------------------------------------------------------------------===///
  /// DT-5: DB replication
  ///
  /// Identify DBs with high read count from multiple nodes, emit
  /// artsAddDbDuplicate() to replicate to consumer nodes.
  /// Placeholder -- no transform applied yet.
  ///===------------------------------------------------------------------===///

  ///===------------------------------------------------------------------===///
  /// DT-6: Atomic reduction recognition
  ///
  /// Detect scalar reductions in EDT bodies, emit
  /// artsAtomicAddInArrayDb() instead of per-worker accumulator +
  /// finalization EDT.
  /// Placeholder -- no transform applied yet.
  ///
  /// TODO(DT-6): Placeholder — no transform implemented. Detect scalar
  /// reductions in EDT bodies and emit artsAtomicAddInArrayDb() instead
  /// of per-worker accumulator + finalization EDT.
  ///===------------------------------------------------------------------===///

  ///===------------------------------------------------------------------===///
  /// DT-7: Block window caching
  ///
  /// Cache computed block window (startBlock, blockCount) in contract
  /// so DbLowering doesn't recompute the element-to-block mapping.
  ///===------------------------------------------------------------------===///
  unsigned dt7Count = cacheBlockWindows();
  if (dt7Count > 0)
    ARTS_INFO("DT-7: cached block windows on " << dt7Count << " acquires");

  ARTS_INFO_FOOTER(DbTransformsPass);
}

///===----------------------------------------------------------------------===///
/// DT-1: Contract persistence
///===----------------------------------------------------------------------===///
unsigned DbTransformsPass::persistContracts() {
  ModuleOp module = getOperation();
  unsigned count = 0;

  module.walk([&](func::FuncOp func) {
    DbAnalysis &dbAnalysis = AM->getDbAnalysis();
    SmallVector<DbAcquireOp, 16> acquires;
    func.walk([&](DbAcquireOp acquire) { acquires.push_back(acquire); });

    for (DbAcquireOp acquire : acquires) {
      auto contractSummary = dbAnalysis.getAcquireContractSummary(acquire);
      if (!contractSummary)
        continue;

      if (!contractSummary->facts)
        continue;

      LoweringContractInfo persistedContract = contractSummary->contract;
      const bool needsRefinedMarker = !persistedContract.postDbRefined;
      if (needsRefinedMarker)
        persistedContract.postDbRefined = true;

      if (!contractSummary->refinedByDbAnalysis && !needsRefinedMarker)
        continue;

      /// Persist post-DB contract payload on the acquire pointer as the
      /// canonical source consumed by downstream passes.
      OpBuilder builder(acquire.getContext());
      builder.setInsertionPointAfter(acquire.getOperation());
      upsertLoweringContract(builder, acquire.getLoc(), acquire.getPtr(),
                             persistedContract);
      ++count;

      ARTS_DEBUG("DT-1: persisted contract for acquire " << acquire);
    }
  });

  return count;
}

///===----------------------------------------------------------------------===///
/// DT-2: Stencil halo consolidation
///===----------------------------------------------------------------------===///
unsigned DbTransformsPass::consolidateStencilHalos() {
  ModuleOp module = getOperation();
  unsigned count = 0;

  module.walk([&](func::FuncOp func) {
    DbAnalysis &dbAnalysis = AM->getDbAnalysis();
    SmallVector<DbAcquireOp, 16> acquires;
    func.walk([&](DbAcquireOp acquire) { acquires.push_back(acquire); });

    for (DbAcquireOp acquire : acquires) {
      /// -----------------------------------------------------------
      /// Step 0: Determine if this acquire is on a stencil-family DB.
      /// Check via the contract first, then fall back to raw attrs.
      /// -----------------------------------------------------------
      auto contractOpt = getLoweringContract(acquire.getPtr());
      auto rawDepPattern = getDepPattern(acquire.getOperation());

      bool isStencilFamily = false;
      if (contractOpt && contractOpt->depPattern)
        isStencilFamily = isStencilFamilyDepPattern(*contractOpt->depPattern);
      if (!isStencilFamily && rawDepPattern)
        isStencilFamily = isStencilFamilyDepPattern(*rawDepPattern);

      /// Also check via the AcquireContractSummary from DbAnalysis which
      /// may have inferred a stencil pattern from graph-backed facts.
      if (!isStencilFamily) {
        auto contractSummary = dbAnalysis.getAcquireContractSummary(acquire);
        if (contractSummary &&
            contractSummary->contract.hasExplicitStencilContract())
          isStencilFamily = true;
      }

      if (!isStencilFamily)
        continue;

      ARTS_DEBUG("DT-2: processing stencil acquire " << acquire);

      /// -----------------------------------------------------------
      /// Step 1: Gather halo bounds from all three sources.
      ///
      /// Source A: Graph analysis (PartitionBoundsAnalyzer's StencilBounds)
      /// Source B: Raw IR attrs (stencil_min_offsets / stencil_max_offsets)
      /// Source C: Existing LoweringContractOp's minOffsets/maxOffsets
      ///
      /// Each source provides a per-dimension min/max vector. We take
      /// the conservative union: min of mins, max of maxes.
      /// -----------------------------------------------------------

      /// Determine spatial rank from whichever source has the most dims.
      unsigned rank = 0;

      /// Source A: graph analysis StencilBounds (1-D scalar bounds)
      std::optional<StencilBounds> graphBounds;
      DbAcquireNode *acqNode = dbAnalysis.getDbAcquireNode(acquire);
      if (acqNode)
        graphBounds = acqNode->getStencilBounds();

      /// Source B: raw IR attrs
      auto rawMinOffsets = getStencilMinOffsets(acquire.getOperation());
      auto rawMaxOffsets = getStencilMaxOffsets(acquire.getOperation());
      if (rawMinOffsets)
        rank = std::max(rank, static_cast<unsigned>(rawMinOffsets->size()));
      if (rawMaxOffsets)
        rank = std::max(rank, static_cast<unsigned>(rawMaxOffsets->size()));

      /// Source C: existing contract SSA values (read as static constants)
      std::optional<SmallVector<int64_t, 4>> contractStaticMins;
      std::optional<SmallVector<int64_t, 4>> contractStaticMaxs;
      if (contractOpt) {
        contractStaticMins = contractOpt->getStaticMinOffsets();
        contractStaticMaxs = contractOpt->getStaticMaxOffsets();
        if (contractStaticMins)
          rank =
              std::max(rank, static_cast<unsigned>(contractStaticMins->size()));
        if (contractStaticMaxs)
          rank =
              std::max(rank, static_cast<unsigned>(contractStaticMaxs->size()));
      }

      /// Graph analysis is 1-D; promote to rank if we have a rank > 0.
      if (graphBounds && graphBounds->valid && rank == 0)
        rank = 1;

      /// If we have no dimensional information at all, skip.
      if (rank == 0) {
        ARTS_DEBUG("DT-2: no halo bounds available, skipping");
        continue;
      }

      /// -----------------------------------------------------------
      /// Step 2: Compute conservative union across all sources.
      ///
      /// For each dimension d:
      ///   unified_min[d] = min(sourceA_min[d], sourceB_min[d], sourceC_min[d])
      ///   unified_max[d] = max(sourceA_max[d], sourceB_max[d], sourceC_max[d])
      /// -----------------------------------------------------------
      SmallVector<int64_t, 4> unifiedMin(rank, 0);
      SmallVector<int64_t, 4> unifiedMax(rank, 0);

      /// Merge Source A: graph analysis (1-D scalar -> dimension 0)
      if (graphBounds && graphBounds->valid) {
        unifiedMin[0] = std::min(unifiedMin[0], graphBounds->minOffset);
        unifiedMax[0] = std::max(unifiedMax[0], graphBounds->maxOffset);
        ARTS_DEBUG("DT-2:   graph bounds: min=" << graphBounds->minOffset
                                                << " max="
                                                << graphBounds->maxOffset);
      }

      /// Merge Source B: raw IR attrs
      if (rawMinOffsets) {
        for (unsigned d = 0; d < rawMinOffsets->size() && d < rank; ++d)
          unifiedMin[d] = std::min(unifiedMin[d], (*rawMinOffsets)[d]);
        ARTS_DEBUG(
            "DT-2:   raw min offsets present, size=" << rawMinOffsets->size());
      }
      if (rawMaxOffsets) {
        for (unsigned d = 0; d < rawMaxOffsets->size() && d < rank; ++d)
          unifiedMax[d] = std::max(unifiedMax[d], (*rawMaxOffsets)[d]);
        ARTS_DEBUG(
            "DT-2:   raw max offsets present, size=" << rawMaxOffsets->size());
      }

      /// Merge Source C: contract SSA values
      if (contractStaticMins) {
        for (unsigned d = 0; d < contractStaticMins->size() && d < rank; ++d)
          unifiedMin[d] = std::min(unifiedMin[d], (*contractStaticMins)[d]);
        ARTS_DEBUG("DT-2:   contract min offsets present, size="
                   << contractStaticMins->size());
      }
      if (contractStaticMaxs) {
        for (unsigned d = 0; d < contractStaticMaxs->size() && d < rank; ++d)
          unifiedMax[d] = std::max(unifiedMax[d], (*contractStaticMaxs)[d]);
        ARTS_DEBUG("DT-2:   contract max offsets present, size="
                   << contractStaticMaxs->size());
      }

      /// Check if unified bounds are all zeros (no halo at all).
      bool allZero = true;
      for (unsigned d = 0; d < rank; ++d) {
        if (unifiedMin[d] != 0 || unifiedMax[d] != 0) {
          allZero = false;
          break;
        }
      }
      if (allZero) {
        ARTS_DEBUG("DT-2: unified bounds are all zero, skipping");
        continue;
      }

      /// -----------------------------------------------------------
      /// Step 3: Write unified bounds into the contract via
      ///         upsertLoweringContract().
      /// -----------------------------------------------------------
      LoweringContractInfo newContract =
          contractOpt.value_or(LoweringContractInfo{});

      /// If the contract had no depPattern, try to populate it from raw attrs.
      if (!newContract.depPattern && rawDepPattern)
        newContract.depPattern = *rawDepPattern;

      /// Materialize the unified offset vectors as SSA index constants.
      OpBuilder builder(acquire.getContext());
      builder.setInsertionPointAfter(acquire.getOperation());
      Location loc = acquire.getLoc();

      SmallVector<Value, 4> minOffsetValues;
      SmallVector<Value, 4> maxOffsetValues;
      minOffsetValues.reserve(rank);
      maxOffsetValues.reserve(rank);
      for (unsigned d = 0; d < rank; ++d) {
        minOffsetValues.push_back(
            createConstantIndex(builder, loc, unifiedMin[d]));
        maxOffsetValues.push_back(
            createConstantIndex(builder, loc, unifiedMax[d]));
      }

      newContract.minOffsets.assign(minOffsetValues.begin(),
                                    minOffsetValues.end());
      newContract.maxOffsets.assign(maxOffsetValues.begin(),
                                    maxOffsetValues.end());

      upsertLoweringContract(builder, loc, acquire.getPtr(), newContract);

      /// -----------------------------------------------------------
      /// Step 4: Remove redundant raw stencil attrs from the acquire.
      ///         These are now consolidated into the contract.
      /// -----------------------------------------------------------
      Operation *acquireOp = acquire.getOperation();
      acquireOp->removeAttr(AttrNames::Operation::Stencil::FootprintMinOffsets);
      acquireOp->removeAttr(AttrNames::Operation::Stencil::FootprintMaxOffsets);

      ++count;

      ARTS_DEBUG("DT-2: consolidated halo for acquire "
                 << acquire << " -> unified min/max with rank=" << rank);
    }
  });

  return count;
}

///===----------------------------------------------------------------------===///
/// DT-3: ESD annotation
///===----------------------------------------------------------------------===///
unsigned DbTransformsPass::annotateESD() {
  ModuleOp module = getOperation();
  unsigned count = 0;

  module.walk([&](func::FuncOp func) {
    SmallVector<DbAcquireOp, 16> acquires;
    func.walk([&](DbAcquireOp acquire) { acquires.push_back(acquire); });

    for (DbAcquireOp acquire : acquires) {
      /// -----------------------------------------------------------
      /// Step 0: Only process block or stencil partitioned acquires.
      /// Coarse acquires touch the full DB and don't benefit from ESD.
      /// -----------------------------------------------------------
      auto partMode = acquire.getPartitionMode();
      if (!partMode)
        continue;
      if (*partMode != PartitionMode::block &&
          *partMode != PartitionMode::stencil)
        continue;

      /// Skip acquires that already have ESD annotation.
      auto existingContract = getLoweringContract(acquire.getPtr());
      if (existingContract && existingContract->esdByteOffset &&
          existingContract->esdByteSize)
        continue;

      ARTS_DEBUG("DT-3: evaluating acquire " << acquire);

      /// -----------------------------------------------------------
      /// Step 1: Resolve the parent allocation to get element type
      /// and element sizes for byte computation.
      /// -----------------------------------------------------------
      Operation *allocOp =
          DbUtils::getUnderlyingDbAlloc(acquire.getSourcePtr());
      auto alloc = dyn_cast_or_null<DbAllocOp>(allocOp);
      if (!alloc) {
        ARTS_DEBUG("DT-3: no underlying alloc, skipping");
        continue;
      }

      /// Determine scalar element byte size from the allocation's element type.
      Type elementType = alloc.getElementType();
      if (auto memrefType = dyn_cast<MemRefType>(elementType))
        elementType = memrefType.getElementType();
      if (!elementType.isIntOrFloat()) {
        ARTS_DEBUG("DT-3: non-scalar element type, skipping");
        continue;
      }
      int64_t scalarBytes = (elementType.getIntOrFloatBitWidth() + 7) / 8;
      if (scalarBytes <= 0) {
        ARTS_DEBUG("DT-3: invalid scalar byte size, skipping");
        continue;
      }

      /// Get element sizes from the allocation (inner dimensions of each DB
      /// chunk). These define the stride structure for linearization.
      auto allocElementSizes = alloc.getElementSizes();
      if (allocElementSizes.empty()) {
        ARTS_DEBUG("DT-3: no element sizes on alloc, skipping");
        continue;
      }

      /// Resolve all element sizes to static constants. If any is dynamic,
      /// we cannot statically compute byte offset/size.
      SmallVector<int64_t, 4> staticElementSizes;
      staticElementSizes.reserve(allocElementSizes.size());
      bool hasDynamicElementSize = false;
      for (Value sz : allocElementSizes) {
        auto constVal = ValueAnalysis::getConstantValue(sz);
        if (!constVal) {
          ARTS_DEBUG("DT-3: dynamic element size, skipping");
          hasDynamicElementSize = true;
          break;
        }
        staticElementSizes.push_back(*constVal);
      }
      if (hasDynamicElementSize)
        continue;

      /// -----------------------------------------------------------
      /// Step 2: Determine the access footprint based on partition
      /// mode.
      /// -----------------------------------------------------------
      int64_t esdByteOffset = 0;
      int64_t esdByteSize = 0;

      if (*partMode == PartitionMode::block) {
        /// For block partitioned acquires, the partition offsets/sizes
        /// describe the element-space sub-region this EDT accesses.
        auto partOffsets = acquire.getPartitionOffsets();
        auto partSizes = acquire.getPartitionSizes();
        if (partOffsets.empty() || partSizes.empty()) {
          ARTS_DEBUG("DT-3: no partition offsets/sizes, skipping");
          continue;
        }

        /// Resolve partition offsets to static constants.
        SmallVector<int64_t, 4> staticOffsets;
        bool hasDynamicPartitionOffset = false;
        for (Value v : partOffsets) {
          auto c = ValueAnalysis::getConstantValue(v);
          if (!c) {
            ARTS_DEBUG("DT-3: dynamic partition offset, skipping");
            hasDynamicPartitionOffset = true;
            break;
          }
          staticOffsets.push_back(*c);
        }
        if (hasDynamicPartitionOffset)
          continue;

        /// Resolve partition sizes to static constants.
        SmallVector<int64_t, 4> staticSizes;
        bool hasDynamicPartitionSize = false;
        for (Value v : partSizes) {
          auto c = ValueAnalysis::getConstantValue(v);
          if (!c) {
            ARTS_DEBUG("DT-3: dynamic partition size, skipping");
            hasDynamicPartitionSize = true;
            break;
          }
          staticSizes.push_back(*c);
        }
        if (hasDynamicPartitionSize)
          continue;

        /// Linearize the offset: for N-D element sizes [d0, d1, ...],
        /// linearOffset = off[0]*d1*d2*... + off[1]*d2*... + ... + off[N-1]
        /// byte_offset = linearOffset * scalarBytes
        unsigned rank =
            std::min<unsigned>(staticOffsets.size(), staticElementSizes.size());
        if (rank == 0) {
          ARTS_DEBUG("DT-3: zero rank, skipping");
          continue;
        }

        int64_t linearOffset = 0;
        for (unsigned i = 0; i < rank; ++i) {
          int64_t stride = 1;
          for (unsigned j = i + 1; j < staticElementSizes.size(); ++j)
            stride *= staticElementSizes[j];
          linearOffset += staticOffsets[i] * stride;
        }
        esdByteOffset = linearOffset * scalarBytes;

        /// Compute total elements in the partition slice.
        unsigned sizeRank =
            std::min<unsigned>(staticSizes.size(), staticElementSizes.size());
        int64_t totalElements = 1;
        for (unsigned i = 0; i < sizeRank; ++i)
          totalElements *= staticSizes[i];
        esdByteSize = totalElements * scalarBytes;

      } else {
        /// PartitionMode::stencil
        /// For stencil acquires, use the partition offsets/sizes plus halo
        /// bounds from the contract (consolidated by DT-2).
        auto partOffsets = acquire.getPartitionOffsets();
        auto partSizes = acquire.getPartitionSizes();
        if (partOffsets.empty() || partSizes.empty()) {
          ARTS_DEBUG("DT-3: stencil acquire without partition info, skipping");
          continue;
        }

        /// Resolve partition offsets and sizes.
        SmallVector<int64_t, 4> staticOffsets, staticSizes;
        bool hasDynamicPartitionInfo = false;
        for (Value v : partOffsets) {
          auto c = ValueAnalysis::getConstantValue(v);
          if (!c) {
            hasDynamicPartitionInfo = true;
            break;
          }
          staticOffsets.push_back(*c);
        }
        if (hasDynamicPartitionInfo)
          continue;
        for (Value v : partSizes) {
          auto c = ValueAnalysis::getConstantValue(v);
          if (!c) {
            hasDynamicPartitionInfo = true;
            break;
          }
          staticSizes.push_back(*c);
        }
        if (hasDynamicPartitionInfo)
          continue;

        /// Apply halo expansion from the contract if available.
        /// The halo extends the partition in each dimension:
        ///   adjustedOffset[d] = partOffset[d] + minOffset[d]  (minOffset <= 0)
        ///   adjustedSize[d] = partSize[d] + maxOffset[d] - minOffset[d]
        SmallVector<int64_t, 4> adjustedOffsets = staticOffsets;
        SmallVector<int64_t, 4> adjustedSizes = staticSizes;

        auto contractOpt = getLoweringContract(acquire.getPtr());
        if (contractOpt) {
          auto staticMins = contractOpt->getStaticMinOffsets();
          auto staticMaxs = contractOpt->getStaticMaxOffsets();
          unsigned haloDims = adjustedOffsets.size();
          if (staticMins && staticMaxs)
            haloDims = std::min<unsigned>(
                haloDims,
                std::min<unsigned>(staticMins->size(), staticMaxs->size()));

          if (staticMins) {
            for (unsigned d = 0; d < haloDims && d < staticMins->size(); ++d) {
              /// minOffset is negative for left halo
              adjustedOffsets[d] += (*staticMins)[d];
              if (adjustedOffsets[d] < 0)
                adjustedOffsets[d] = 0; // clamp to zero
            }
          }
          if (staticMins && staticMaxs) {
            for (unsigned d = 0; d < haloDims && d < staticMins->size() &&
                                 d < staticMaxs->size();
                 ++d) {
              /// Total halo expansion = maxOffset - minOffset
              adjustedSizes[d] += (*staticMaxs)[d] - (*staticMins)[d];
            }
          }
        }

        /// Linearize the adjusted offset.
        unsigned rank = std::min<unsigned>(adjustedOffsets.size(),
                                           staticElementSizes.size());
        if (rank == 0)
          continue;

        int64_t linearOffset = 0;
        for (unsigned i = 0; i < rank; ++i) {
          int64_t stride = 1;
          for (unsigned j = i + 1; j < staticElementSizes.size(); ++j)
            stride *= staticElementSizes[j];
          linearOffset += adjustedOffsets[i] * stride;
        }
        esdByteOffset = linearOffset * scalarBytes;

        /// Compute total elements for the halo-expanded region.
        unsigned sizeRank =
            std::min<unsigned>(adjustedSizes.size(), staticElementSizes.size());
        int64_t totalElements = 1;
        for (unsigned i = 0; i < sizeRank; ++i)
          totalElements *= adjustedSizes[i];
        esdByteSize = totalElements * scalarBytes;
      }

      /// -----------------------------------------------------------
      /// Step 3: Validate and write ESD annotation.
      /// -----------------------------------------------------------
      if (esdByteSize <= 0) {
        ARTS_DEBUG("DT-3: computed zero or negative byte size, skipping");
        continue;
      }
      if (esdByteOffset < 0) {
        ARTS_DEBUG("DT-3: computed negative byte offset, skipping");
        continue;
      }

      /// Build the updated contract with ESD fields.
      LoweringContractInfo newContract =
          existingContract.value_or(LoweringContractInfo{});

      /// Populate depPattern from raw attrs if not already in the contract.
      if (!newContract.depPattern) {
        auto rawDepPattern = getDepPattern(acquire.getOperation());
        if (rawDepPattern)
          newContract.depPattern = *rawDepPattern;
      }

      newContract.esdByteOffset = esdByteOffset;
      newContract.esdByteSize = esdByteSize;

      OpBuilder builder(acquire.getContext());
      builder.setInsertionPointAfter(acquire.getOperation());
      upsertLoweringContract(builder, acquire.getLoc(), acquire.getPtr(),
                             newContract);
      ++count;

      ARTS_DEBUG("DT-3: annotated ESD for acquire "
                 << acquire << " -> offset=" << esdByteOffset
                 << " size=" << esdByteSize);
    }
  });

  return count;
}

///===----------------------------------------------------------------------===///
/// DT-7: Block window caching
///===----------------------------------------------------------------------===///
unsigned DbTransformsPass::cacheBlockWindows() {
  ModuleOp module = getOperation();
  unsigned count = 0;

  module.walk([&](func::FuncOp func) {
    SmallVector<DbAcquireOp, 16> acquires;
    func.walk([&](DbAcquireOp acquire) { acquires.push_back(acquire); });

    for (DbAcquireOp acquire : acquires) {
      /// -----------------------------------------------------------
      /// Step 0: Only process block-partitioned acquires.
      /// Stencil and coarse acquires don't use block window caching.
      /// -----------------------------------------------------------
      auto partMode = acquire.getPartitionMode();
      if (!partMode || *partMode != PartitionMode::block)
        continue;

      /// Skip acquires that already have cached block window annotation.
      auto existingContract = getLoweringContract(acquire.getPtr());
      if (existingContract && existingContract->cachedStartBlock &&
          existingContract->cachedBlockCount)
        continue;

      ARTS_DEBUG("DT-7: evaluating acquire " << acquire);

      /// -----------------------------------------------------------
      /// Step 1: Resolve partition offsets and sizes to static
      /// constants. If any value is dynamic, skip conservatively.
      /// -----------------------------------------------------------
      auto partOffsets = acquire.getPartitionOffsets();
      auto partSizes = acquire.getPartitionSizes();
      if (partOffsets.empty() || partSizes.empty()) {
        ARTS_DEBUG("DT-7: no partition offsets/sizes, skipping");
        continue;
      }

      SmallVector<int64_t, 4> staticOffsets;
      staticOffsets.reserve(partOffsets.size());
      bool hasDynamicPartitionOffset = false;
      for (Value v : partOffsets) {
        auto c = ValueAnalysis::getConstantValue(v);
        if (!c) {
          ARTS_DEBUG("DT-7: dynamic partition offset, skipping");
          hasDynamicPartitionOffset = true;
          break;
        }
        staticOffsets.push_back(*c);
      }
      if (hasDynamicPartitionOffset)
        continue;

      SmallVector<int64_t, 4> staticSizes;
      staticSizes.reserve(partSizes.size());
      bool hasDynamicPartitionSize = false;
      for (Value v : partSizes) {
        auto c = ValueAnalysis::getConstantValue(v);
        if (!c) {
          ARTS_DEBUG("DT-7: dynamic partition size, skipping");
          hasDynamicPartitionSize = true;
          break;
        }
        staticSizes.push_back(*c);
      }
      if (hasDynamicPartitionSize)
        continue;

      /// -----------------------------------------------------------
      /// Step 2: Resolve block sizes. Try the contract's blockShape
      /// first, then fall back to fine-grained partitioning info
      /// from the parent allocation's element sizes.
      /// -----------------------------------------------------------
      SmallVector<int64_t, 4> staticBlockSizes;

      /// Source A: blockShape from the contract (preferred).
      if (existingContract && !existingContract->blockShape.empty()) {
        staticBlockSizes.reserve(existingContract->blockShape.size());
        for (Value v : existingContract->blockShape) {
          auto c = ValueAnalysis::getConstantValue(v);
          if (!c) {
            staticBlockSizes.clear();
            break;
          }
          staticBlockSizes.push_back(*c);
        }
        if (!staticBlockSizes.empty())
          ARTS_DEBUG("DT-7: using contract blockShape, rank="
                     << staticBlockSizes.size());
      }

      /// Source B: fall back to the alloc's element sizes as the block
      /// size (1-D block partitioning uses element_sizes[0] as block).
      if (staticBlockSizes.empty()) {
        Operation *allocOp =
            DbUtils::getUnderlyingDbAlloc(acquire.getSourcePtr());
        auto alloc = dyn_cast_or_null<DbAllocOp>(allocOp);
        if (!alloc) {
          ARTS_DEBUG("DT-7: no underlying alloc, skipping");
          continue;
        }

        auto allocElementSizes = alloc.getElementSizes();
        if (allocElementSizes.empty()) {
          ARTS_DEBUG("DT-7: no element sizes on alloc, skipping");
          continue;
        }

        staticBlockSizes.reserve(allocElementSizes.size());
        for (Value sz : allocElementSizes) {
          auto c = ValueAnalysis::getConstantValue(sz);
          if (!c) {
            staticBlockSizes.clear();
            break;
          }
          staticBlockSizes.push_back(*c);
        }
        if (!staticBlockSizes.empty())
          ARTS_DEBUG("DT-7: using alloc element sizes as block sizes, rank="
                     << staticBlockSizes.size());
      }

      if (staticBlockSizes.empty()) {
        ARTS_DEBUG("DT-7: could not resolve block sizes, skipping");
        continue;
      }

      /// -----------------------------------------------------------
      /// Step 3: Compute startBlock and blockCount for N-D.
      ///
      /// For N-D block partitioning, we linearize block coordinates:
      ///   blockCoord[d] = partitionOffset[d] / blockSize[d]
      ///   blocksInDim[d] = ceil(partitionSize[d] / blockSize[d])
      ///
      /// Then linearize into a single startBlock and blockCount:
      ///   startBlock = blockCoord[0]*blocksPerDim[1]*... + blockCoord[1]*... +
      ///   ... blockCount = blocksInDim[0] * blocksInDim[1] * ...
      ///
      /// For 1-D this reduces to:
      ///   startBlock = partitionOffset[0] / blockSize[0]
      ///   blockCount = ceil(partitionSize[0] / blockSize[0])
      /// -----------------------------------------------------------
      unsigned rank = std::min<unsigned>(
          std::min<unsigned>(staticOffsets.size(), staticSizes.size()),
          staticBlockSizes.size());
      if (rank == 0) {
        ARTS_DEBUG("DT-7: zero rank, skipping");
        continue;
      }

      /// Validate all block sizes are positive.
      bool hasInvalidBlockSize = false;
      for (unsigned d = 0; d < rank; ++d) {
        if (staticBlockSizes[d] <= 0) {
          ARTS_DEBUG("DT-7: non-positive block size at dim " << d
                                                             << ", skipping");
          hasInvalidBlockSize = true;
          break;
        }
      }
      if (hasInvalidBlockSize)
        continue;

      /// Compute per-dimension block coordinates and block counts.
      SmallVector<int64_t, 4> blockCoords(rank);
      SmallVector<int64_t, 4> blocksPerDim(rank);
      for (unsigned d = 0; d < rank; ++d) {
        blockCoords[d] = staticOffsets[d] / staticBlockSizes[d];
        /// ceil division: (size + blockSize - 1) / blockSize
        blocksPerDim[d] =
            (staticSizes[d] + staticBlockSizes[d] - 1) / staticBlockSizes[d];
        if (blocksPerDim[d] <= 0)
          blocksPerDim[d] = 1;
      }

      /// Linearize start block index across dimensions.
      int64_t startBlock = 0;
      for (unsigned d = 0; d < rank; ++d) {
        int64_t stride = 1;
        for (unsigned j = d + 1; j < rank; ++j)
          stride *= blocksPerDim[j];
        startBlock += blockCoords[d] * stride;
      }

      /// Total block count is the product of per-dimension block counts.
      int64_t blockCount = 1;
      for (unsigned d = 0; d < rank; ++d)
        blockCount *= blocksPerDim[d];

      /// -----------------------------------------------------------
      /// Step 4: Validate and write cached block window.
      /// -----------------------------------------------------------
      if (startBlock < 0) {
        ARTS_DEBUG("DT-7: computed negative start block, skipping");
        continue;
      }
      if (blockCount <= 0) {
        ARTS_DEBUG("DT-7: computed non-positive block count, skipping");
        continue;
      }

      LoweringContractInfo newContract =
          existingContract.value_or(LoweringContractInfo{});

      /// Populate depPattern from raw attrs if not already in the contract.
      if (!newContract.depPattern) {
        auto rawDepPattern = getDepPattern(acquire.getOperation());
        if (rawDepPattern)
          newContract.depPattern = *rawDepPattern;
      }

      newContract.cachedStartBlock = startBlock;
      newContract.cachedBlockCount = blockCount;

      OpBuilder builder(acquire.getContext());
      builder.setInsertionPointAfter(acquire.getOperation());
      upsertLoweringContract(builder, acquire.getLoc(), acquire.getPtr(),
                             newContract);
      ++count;

      ARTS_DEBUG("DT-7: cached block window for acquire "
                 << acquire << " -> startBlock=" << startBlock
                 << " blockCount=" << blockCount);
    }
  });

  return count;
}

///===----------------------------------------------------------------------===///
/// Pass creation
///===----------------------------------------------------------------------===///
namespace mlir {
namespace arts {
std::unique_ptr<Pass> createDbTransformsPass(mlir::arts::AnalysisManager *AM) {
  return std::make_unique<DbTransformsPass>(AM);
}
} // namespace arts
} // namespace mlir
