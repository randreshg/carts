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
#define GEN_PASS_DEF_DBTRANSFORMS
#include "arts/Dialect.h"
#include "arts/passes/Passes.h"
#include "arts/passes/Passes.h.inc"
#include "arts/transforms/db/transforms/GUIDRangeDetection.h"
#include "arts/utils/DbUtils.h"
#include "arts/utils/LoweringContractUtils.h"
#include "arts/utils/OperationAttributes.h"
#include "arts/utils/StencilAttributes.h"
#include "arts/utils/Utils.h"
#include "mlir/Pass/Pass.h"
/// Debug
#include "arts/utils/Debug.h"
ARTS_DEBUG_SETUP(db_transforms);

using namespace mlir;
using namespace mlir::func;
using namespace mlir::arts;

namespace {
struct DbTransformsPass : public impl::DbTransformsBase<DbTransformsPass> {
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
