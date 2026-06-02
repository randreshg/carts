///==========================================================================///
/// File: DbModeTightening.cpp
/// Pass for DB mode tightening and storage-type inference.
/// Partitioning facts come from DbAnalysis; this pass only reconciles DB modes
/// and storage policy against those facts and observed uses.
///
/// Example:
///   Before:
///     arts.db_acquire[<inout>] ...   // conservative mode
///
///   After:
///     arts.db_acquire[<in>] ...      // mode tightened when write not observed
///==========================================================================///

/// Dialects
#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/Dominance.h"
/// Arts
#include "carts/dialect/arts/Analysis/AnalysisManager.h"
#include "carts/dialect/arts/Analysis/db/DbAnalysis.h"
#include "carts/dialect/arts/Analysis/db/OwnershipProof.h"
#include "carts/dialect/arts/Analysis/loop/LoopAnalysis.h"
#include "carts/dialect/arts/IR/ArtsDialect.h"
#include "carts/passes/Passes.h"
#include "mlir/Pass/Pass.h"
/// Debug
#include "carts/dialect/arts/Utils/BlockedAccessUtils.h"
#include "carts/dialect/arts/Utils/DbUtils.h"
#include "carts/dialect/arts/Utils/LoweringContractUtils.h"
#include "carts/dialect/arts/Utils/OperationAttributes.h"
#include "carts/dialect/arts/Utils/PartitionPredicates.h"
#include "carts/dialect/arts/Utils/ValueAnalysisUtils.h"
#include "carts/utils/Debug.h"
#include "carts/utils/Utils.h"
#include "carts/utils/ValueAnalysis.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SetVector.h"

using namespace mlir;
using namespace mlir::func;
using namespace mlir::carts;
using namespace mlir::carts::arts;

#define GEN_PASS_DEF_DBMODETIGHTENING
#include "carts/passes/Passes.h.inc"

ARTS_DEBUG_SETUP(db_mode_tightening);

#include "llvm/ADT/Statistic.h"
static llvm::Statistic numAcquiresTightenedToRead{
    "db_mode_tightening", "NumAcquiresTightenedToRead",
    "Number of acquires tightened to read mode"};
static llvm::Statistic numAcquiresTightenedToOut{
    "db_mode_tightening", "NumAcquiresTightenedToOut",
    "Number of acquires tightened to out mode"};
static llvm::Statistic numAllocModesAdjusted{
    "db_mode_tightening", "NumAllocModesAdjusted",
    "Number of alloc modes adjusted to match acquire patterns"};
static llvm::Statistic numAllocDbModesAdjusted{
    "db_mode_tightening", "NumAllocDbModesAdjusted",
    "Number of alloc DB modes adjusted to match acquire patterns"};
static llvm::Statistic numDbsMarkedLocalOnly{
    "db_mode_tightening", "NumDbsMarkedLocalOnly",
    "Number of DBs annotated as local-only"};
static llvm::Statistic numDbsMarkedReadOnlyAfterInit{
    "db_mode_tightening", "NumDbsMarkedReadOnlyAfterInit",
    "Number of DBs annotated as read-only-after-init"};

namespace {

static bool isProvablyZeroLoopLowerBound(Value lb) {
  lb = ValueAnalysis::stripNumericCasts(lb);
  if (ValueAnalysis::isZeroConstant(lb))
    return true;

  auto select = lb.getDefiningOp<arith::SelectOp>();
  if (!select)
    return false;

  Value trueVal = ValueAnalysis::stripNumericCasts(select.getTrueValue());
  Value falseVal = ValueAnalysis::stripNumericCasts(select.getFalseValue());
  auto cmp = ValueAnalysis::stripNumericCasts(select.getCondition())
                 .getDefiningOp<arith::CmpIOp>();
  if (!cmp)
    return false;

  Value lhs = ValueAnalysis::stripNumericCasts(cmp.getLhs());
  Value rhs = ValueAnalysis::stripNumericCasts(cmp.getRhs());
  auto pred = cmp.getPredicate();

  auto matchesZeroClamp = [&](Value zeroArm, Value otherArm) {
    if (!ValueAnalysis::isZeroConstant(zeroArm) ||
        !isKnownNonPositive(otherArm))
      return false;
    return ((pred == arith::CmpIPredicate::slt ||
             pred == arith::CmpIPredicate::ult) &&
            ValueAnalysis::sameValue(lhs, otherArm) &&
            ValueAnalysis::isZeroConstant(rhs)) ||
           ((pred == arith::CmpIPredicate::sgt ||
             pred == arith::CmpIPredicate::ugt) &&
            ValueAnalysis::sameValue(rhs, otherArm) &&
            ValueAnalysis::isZeroConstant(lhs));
  };

  return matchesZeroClamp(trueVal, falseVal) ||
         matchesZeroClamp(falseVal, trueVal);
}

static bool isLoopFullRange(LoopNode *loop, Value dimSize) {
  if (!loop || !dimSize)
    return false;

  Value lb = loop->getLowerBound();
  Value step = loop->getStep();
  Value ub = loop->getUpperBound();
  if (!lb || !step || !ub)
    return false;

  if (!isProvablyZeroLoopLowerBound(lb))
    return false;
  if (!ValueAnalysis::isOneConstant(ValueAnalysis::stripNumericCasts(step)))
    return false;
  return areEquivalentOwnedSliceExtents(ub, dimSize);
}

static bool isIndexFullCoverage(Value idx, Value dimSize,
                                ArrayRef<LoopNode *> loops) {
  if (!idx || !dimSize)
    return false;

  auto dimConstOpt =
      arts::tryFoldConstantIndex(ValueAnalysis::stripNumericCasts(dimSize));
  if (dimConstOpt && *dimConstOpt == 1)
    return true;

  idx = ValueAnalysis::stripNumericCasts(idx);

  /// Constant index only covers full range if size == 1.
  auto idxConstOpt = arts::tryFoldConstantIndex(idx);
  if (idxConstOpt)
    /// A constant index alone cannot cover a non-unit dimension.
    return false;

  LoopNode *best = nullptr;
  LoopNode::IVExpr bestExpr;
  int bestDepth = -1;

  for (LoopNode *loop : loops) {
    if (!loop || !loop->dependsOnInductionVarNormalized(idx))
      continue;
    auto expr = loop->analyzeIndexExpr(idx);
    if (!expr.dependsOnIV)
      continue;
    int depth = loop->getNestingDepth();
    if (!best || depth > bestDepth) {
      best = loop;
      bestExpr = expr;
      bestDepth = depth;
    }
  }

  if (!best || !bestExpr.multiplier || !bestExpr.offset)
    return false;
  if (*bestExpr.multiplier != 1)
    return false;
  if (*bestExpr.offset != 0)
    return false;

  return isLoopFullRange(best, dimSize);
}

static SmallVector<Value> getEffectiveSliceDimSizes(DbAnalysis &dbAnalysis,
                                                    DbAcquireOp acquire,
                                                    DbAllocOp allocOp) {
  return dbAnalysis.getEffectiveAcquireSliceDimSizes(acquire, allocOp);
}

static bool writesFullAllocation(DbAnalysis &dbAnalysis, DbAcquireOp acquire,
                                 DbAllocOp allocOp,
                                 LoopAnalysis &loopAnalysis) {
  if (!acquire || !allocOp)
    return true;

  std::optional<DbAnalysis::AcquireAccessSummary> accessSummary =
      dbAnalysis.getAcquireAccessSummary(acquire);
  if (!accessSummary)
    return true;
  if (!accessSummary->hasStores)
    return true;

  EdtOp edt = accessSummary->edtUser;
  if (!edt)
    return true;

  SmallVector<LoopNode *> loops;
  loopAnalysis.collectLoopsInOperation(edt, loops);

  DbAnalysis::AcquireAccessOperationMap dbRefToMemOps;
  dbAnalysis.collectAcquireAccessOperations(acquire, dbRefToMemOps);
  if (dbRefToMemOps.empty())
    return true;

  SmallVector<Value> dimSizes =
      getEffectiveSliceDimSizes(dbAnalysis, acquire, allocOp);
  if (dimSizes.empty())
    return true;

  for (auto &entry : dbRefToMemOps) {
    DbRefOp dbRef = entry.first;
    for (Operation *memOp : entry.second) {
      if (!isa<memref::StoreOp>(memOp))
        continue;
      SmallVector<Value> indexChain =
          DbUtils::collectFullIndexChain(dbRef, memOp);
      if (indexChain.empty())
        return false;

      unsigned memrefStart = dbRef.getIndices().size();
      if (indexChain.size() <= memrefStart)
        return false;

      unsigned memrefRank = indexChain.size() - memrefStart;
      unsigned sizeRank = dimSizes.size();
      unsigned checkRank = std::min(memrefRank, sizeRank);

      for (unsigned d = 0; d < checkRank; ++d) {
        Value idx = indexChain[memrefStart + d];
        Value dimSize = dimSizes[d];
        if (!isIndexFullCoverage(idx, dimSize, loops))
          return false;
      }

      if (memrefRank != sizeRank)
        return false;
    }
  }

  return true;
}

static bool canPreserveProofTrustedPartitionedWrite(DbAcquireOp acquire) {
  if (!acquire)
    return false;

  auto partitionMode =
      acquire.getPartitionMode().value_or(PartitionMode::coarse);
  if (partitionMode == PartitionMode::coarse)
    return false;

  if (auto info = resolveAcquireContract(acquire))
    if (info->hasExplicitStencilContract() && info->supportsBlockHalo())
      return false;

  LoweringContractOp contract = getLoweringContractOp(acquire.getPtr());
  if (!contract)
    contract = getLoweringContractOp(acquire.getSourcePtr());
  if (!contract)
    return false;

  OwnershipProof proof = readOwnershipProof(contract.getOperation());
  if (!(proof.partitionAccessMapping && proof.depSliceSoundness))
    proof = computeOwnershipProof(contract);
  return proof.partitionAccessMapping && proof.depSliceSoundness;
}

static bool getBoolAttr(Operation *op, llvm::StringLiteral name) {
  if (!op)
    return false;
  if (auto attr = op->getAttrOfType<BoolAttr>(name))
    return attr.getValue();
  return false;
}

static RuntimeDbMode getOrderedRuntimeDbMode(ArtsMode mode) {
  return mode == ArtsMode::in ? RuntimeDbMode::ro : RuntimeDbMode::ew;
}

static bool hasTrustedPartitionedWriteContract(DbAcquireOp acquire) {
  if (!acquire)
    return false;

  auto mode = acquire.getPartitionMode().value_or(PartitionMode::coarse);
  if (!usesBlockLayout(mode))
    return false;

  bool hasPartitionWindow =
      (!acquire.getPartitionOffsets().empty() &&
       !acquire.getPartitionSizes().empty()) ||
      (!acquire.getOffsets().empty() && !acquire.getSizes().empty());
  if (!hasPartitionWindow)
    return false;

  if (auto contract = resolveAcquireContract(acquire))
    if (contract->hasExplicitStencilContract() &&
        contract->supportsBlockHalo() && contract->hasOwnerDims())
      return true;

  LoweringContractOp contractOp = getLoweringContractOp(acquire.getPtr());
  if (!contractOp)
    contractOp = getLoweringContractOp(acquire.getSourcePtr());
  if (!contractOp)
    return false;

  Operation *contract = contractOp.getOperation();
  return getBoolAttr(
             contract,
             ::mlir::carts::arts::AttrNames::Proof::OwnerDimReachability) &&
         getBoolAttr(
             contract,
             ::mlir::carts::arts::AttrNames::Proof::PartitionAccessMapping) &&
         getBoolAttr(contract,
                     ::mlir::carts::arts::AttrNames::Proof::HaloLegality);
}

static bool canUseUnorderedLocalWrite(DbAcquireOp acquire, EdtOp edtOp,
                                      ModuleOp module) {
  if (!acquire || !edtOp)
    return false;
  if (acquire.getMode() != ArtsMode::out &&
      acquire.getMode() != ArtsMode::inout)
    return false;
  if (edtOp.getConcurrency() != EdtConcurrency::intranode)
    return false;
  auto totalNodes = arts::getRuntimeTotalNodes(module);
  if (!totalNodes || *totalNodes != 1)
    return false;
  return hasTrustedPartitionedWriteContract(acquire);
}

static bool canUsePlannedCoarseUnorderedOutWrite(DbAcquireOp acquire,
                                                 EdtOp edtOp, ModuleOp module,
                                                 bool payloadMayRead) {
  if (!acquire || !edtOp)
    return false;
  if (acquire.getMode() != ArtsMode::out &&
      acquire.getMode() != ArtsMode::inout)
    return false;
  if (edtOp.getConcurrency() != EdtConcurrency::intranode)
    return false;
  auto totalNodes = arts::getRuntimeTotalNodes(module);
  if (!totalNodes || *totalNodes != 1)
    return false;
  if (acquire.getPartitionMode().value_or(PartitionMode::coarse) !=
      PartitionMode::coarse)
    return false;
  auto alloc = dyn_cast_or_null<DbAllocOp>(
      DbUtils::getUnderlyingDbAlloc(acquire.getSourcePtr()));
  if (!alloc || !alloc.getLocalOnly().value_or(false))
    return false;
  if (!edtOp.getPlanLogicalWorkerSliceAttr() ||
      !edtOp.getPlanIterationTopologyAttr())
    return false;

  if (acquire.getMode() == ArtsMode::inout && payloadMayRead)
    return false;

  return true;
}

static bool isInPlaceSafeUnorderedPattern(ArtsDepPattern pattern) {
  switch (pattern) {
  case ArtsDepPattern::matmul:
  case ArtsDepPattern::uniform:
  case ArtsDepPattern::elementwise_pipeline:
    return true;
  case ArtsDepPattern::unknown:
  case ArtsDepPattern::stencil:
  case ArtsDepPattern::triangular:
  case ArtsDepPattern::wavefront_2d:
  case ArtsDepPattern::alternating_buffer_stencil:
  case ArtsDepPattern::stencil_tiling_nd:
  case ArtsDepPattern::cross_dim_stencil_3d:
  case ArtsDepPattern::higher_order_stencil:
  case ArtsDepPattern::reduction:
    return false;
  }
  return false;
}

static std::optional<unsigned> getDependencyIndex(EdtOp edtOp,
                                                  DbAcquireOp acquire) {
  if (!edtOp || !acquire)
    return std::nullopt;
  for (auto [idx, dep] : llvm::enumerate(edtOp.getDependencies()))
    if (dep.getDefiningOp<DbAcquireOp>() == acquire)
      return idx;
  return std::nullopt;
}

static bool canUseInPlaceSafeCoarseUnorderedWrite(DbAcquireOp acquire,
                                                  EdtOp edtOp, ModuleOp module,
                                                  unsigned depIndex) {
  if (!acquire || !edtOp)
    return false;
  if (depIndex != 0)
    return false;
  if (acquire.getMode() != ArtsMode::out &&
      acquire.getMode() != ArtsMode::inout)
    return false;
  if (edtOp.getConcurrency() != EdtConcurrency::intranode)
    return false;
  auto totalNodes = arts::getRuntimeTotalNodes(module);
  if (!totalNodes || *totalNodes != 1)
    return false;
  if (acquire.getPartitionMode().value_or(PartitionMode::coarse) !=
      PartitionMode::coarse)
    return false;
  auto alloc = dyn_cast_or_null<DbAllocOp>(
      DbUtils::getUnderlyingDbAlloc(acquire.getSourcePtr()));
  if (!alloc || !alloc.getLocalOnly().value_or(false))
    return false;
  auto depPattern = getDepPattern(edtOp.getOperation());
  if (!depPattern || !isInPlaceSafeUnorderedPattern(*depPattern))
    return false;
  if (!edtOp.getInPlaceSafeAttr())
    return false;
  return true;
}

static RuntimeDbMode selectRuntimeDbModeVerdict(DbAnalysis &dbAnalysis,
                                                DbAcquireOp acquire,
                                                ModuleOp module) {
  if (!acquire)
    return RuntimeDbMode::ew;

  RuntimeDbMode orderedMode = getOrderedRuntimeDbMode(acquire.getMode());
  if (orderedMode == RuntimeDbMode::ro)
    return orderedMode;

  std::optional<DbAnalysis::AcquireAccessSummary> accessSummary =
      dbAnalysis.getAcquireAccessSummary(acquire);
  EdtOp edtOp = accessSummary ? accessSummary->edtUser : EdtOp();
  bool payloadMayRead = !accessSummary || accessSummary->hasLoads;
  std::optional<unsigned> depIndex = getDependencyIndex(edtOp, acquire);

  if (canUseUnorderedLocalWrite(acquire, edtOp, module) ||
      canUsePlannedCoarseUnorderedOutWrite(acquire, edtOp, module,
                                           payloadMayRead) ||
      (depIndex && canUseInPlaceSafeCoarseUnorderedWrite(acquire, edtOp, module,
                                                         *depIndex)))
    return RuntimeDbMode::rw;

  return orderedMode;
}

struct MemAccessSite {
  Value source;
  SmallVector<Value, 4> fullIndexChain;
};

static std::optional<MemAccessSite> getMemAccessSite(DbRefOp dbRef,
                                                     Operation *op) {
  if (!dbRef || !op)
    return std::nullopt;
  SmallVector<Value> fullIndexChain = DbUtils::collectFullIndexChain(dbRef, op);
  if (fullIndexChain.empty())
    return std::nullopt;
  if (auto load = dyn_cast<memref::LoadOp>(op))
    return MemAccessSite{
        dbRef.getSource(),
        SmallVector<Value, 4>(fullIndexChain.begin(), fullIndexChain.end())};
  if (auto store = dyn_cast<memref::StoreOp>(op))
    return MemAccessSite{
        dbRef.getSource(),
        SmallVector<Value, 4>(fullIndexChain.begin(), fullIndexChain.end())};
  if (auto load = dyn_cast<affine::AffineLoadOp>(op))
    return MemAccessSite{
        dbRef.getSource(),
        SmallVector<Value, 4>(fullIndexChain.begin(), fullIndexChain.end())};
  if (auto store = dyn_cast<affine::AffineStoreOp>(op))
    return MemAccessSite{
        dbRef.getSource(),
        SmallVector<Value, 4>(fullIndexChain.begin(), fullIndexChain.end())};
  return std::nullopt;
}

static bool sameMemAccessSite(const MemAccessSite &lhs,
                              const MemAccessSite &rhs) {
  if (!ValueAnalysis::sameValue(lhs.source, rhs.source) ||
      lhs.fullIndexChain.size() != rhs.fullIndexChain.size())
    return false;
  for (auto [lhsIdx, rhsIdx] :
       llvm::zip(lhs.fullIndexChain, rhs.fullIndexChain)) {
    if (!ValueAnalysis::sameValue(lhsIdx, rhsIdx))
      return false;
  }
  return true;
}

static bool loadsAreSatisfiedByDominatingStores(DbAnalysis &dbAnalysis,
                                                DbAcquireOp acquire,
                                                func::FuncOp func) {
  if (!acquire || !func)
    return false;

  DbAnalysis::AcquireAccessOperationMap dbRefToMemOps;
  dbAnalysis.collectAcquireAccessOperations(acquire, dbRefToMemOps);
  if (dbRefToMemOps.empty())
    return false;

  SmallVector<std::pair<Operation *, MemAccessSite>, 8> stores;
  SmallVector<std::pair<Operation *, MemAccessSite>, 8> loads;
  for (auto &entry : dbRefToMemOps) {
    DbRefOp dbRef = entry.first;
    for (Operation *memOp : entry.second) {
      auto site = getMemAccessSite(dbRef, memOp);
      if (!site)
        continue;
      if (isa<memref::StoreOp, affine::AffineStoreOp>(memOp))
        stores.push_back({memOp, std::move(*site)});
      else if (isa<memref::LoadOp, affine::AffineLoadOp>(memOp))
        loads.push_back({memOp, std::move(*site)});
    }
  }

  if (stores.empty() || loads.empty())
    return false;

  DominanceInfo domInfo(func);
  for (auto &[loadOp, loadSite] : loads) {
    bool covered = false;
    for (auto &[storeOp, storeSite] : stores) {
      if (!domInfo.dominates(storeOp, loadOp))
        continue;
      if (!sameMemAccessSite(storeSite, loadSite))
        continue;
      covered = true;
      break;
    }
    if (!covered) {
      return false;
    }
  }

  return true;
}

static bool isInsideRepeatableControl(Operation *op) {
  if (!op)
    return false;

  for (Operation *parent = op->getParentOp(); parent;
       parent = parent->getParentOp()) {
    if (isa<scf::ForOp, scf::WhileOp, scf::ParallelOp, scf::ForallOp,
            affine::AffineForOp, affine::AffineParallelOp>(parent))
      return true;
  }

  return false;
}

} // namespace

///===----------------------------------------------------------------------===///
/// Pass Implementation
/// Tighten Datablock access modes and infer storage types
///===----------------------------------------------------------------------===///
namespace {
struct DbModeTighteningPass
    : public ::impl::DbModeTighteningBase<DbModeTighteningPass> {
  DbModeTighteningPass(mlir::carts::arts::AnalysisManager *AM, bool forceInout)
      : AM(AM) {
    assert(AM && "AnalysisManager must be provided externally");
    this->forceInout = forceInout;
  }

  void runOnOperation() override;

  /// Mode adjustment
  bool adjustDbModes();

  /// DB storage-type inference annotations
  void inferDbStorageTypes();

  /// Runtime dependency DB mode verdict annotations
  bool stampRuntimeDbModeVerdicts();

  /// Graph rebuild
  void invalidateAndRebuildGraph();

private:
  ModuleOp module;
  mlir::carts::arts::AnalysisManager *AM = nullptr;
};
} // namespace

void DbModeTighteningPass::runOnOperation() {
  module = getOperation();

  ARTS_INFO_HEADER(DbModeTighteningPass);
  ARTS_DEBUG_REGION(module.dump(););

  assert(AM && "AnalysisManager must be provided externally");

  /// TODO(PERF): adjustDbModes and inferDbStorageTypes both perform separate
  /// module.walk(FuncOp) + getOrCreateGraph. These could be fused into a
  /// single walk if analysis invalidation between phases is handled carefully.

  /// Graph construction and analysis
  invalidateAndRebuildGraph();

  /// Adjust DB modes based on access patterns
  bool changed = adjustDbModes();

  if (changed) {
    ARTS_INFO(" Module has changed, invalidating analyses");
    AM->invalidate();
  }

  /// Infer DB storage-type annotations (local_only, read_only_after_init)
  inferDbStorageTypes();

  bool verdictsChanged = stampRuntimeDbModeVerdicts();
  if (verdictsChanged)
    AM->invalidate();

  ARTS_INFO_FOOTER(DbModeTighteningPass);
  ARTS_DEBUG_REGION(module.dump(););
}

///===----------------------------------------------------------------------===///
/// Adjust DB modes based on accesses.
/// Based on the collected loads and stores, adjust the DB mode to in, out, or
/// inout.
///===----------------------------------------------------------------------===///
bool DbModeTighteningPass::adjustDbModes() {
  ARTS_DEBUG_HEADER(AdjustDBModes);
  bool changed = false;

  module.walk([&](func::FuncOp func) {
    DbAnalysis &dbAnalysis = AM->getDbAnalysis();

    /// First, adjust per-acquire modes
    dbAnalysis.forEachDbAcquire(func, [&](DbAcquireOp acqOp) {
      std::optional<DbAnalysis::AcquireAccessSummary> accessSummary =
          dbAnalysis.getAcquireAccessSummary(acqOp);
      bool hasLoads = accessSummary && accessSummary->hasLoads;
      bool hasStores = accessSummary && accessSummary->hasStores;

      /// Some acquires already carry an authoritative dependency mode
      /// (explicit control dependencies, worker-local partial reductions).
      /// Do not re-infer or optimize those modes from local memory accesses.
      if (acqOp.getPreserveAccessMode()) {
        ARTS_DEBUG("AcquireOp: " << acqOp
                                 << " preserving explicit dependency mode "
                                 << acqOp.getMode());
        return;
      }

      /// If the rewritten IR no longer exposes concrete memory accesses
      /// through this acquire, do not degrade the previously established
      /// access contract. Later lowering stages may still rely on that mode
      /// even when the access path has been rewritten through db_ref users or
      /// opaque pointer views that this analysis cannot classify precisely.
      if (!hasLoads && !hasStores) {
        ARTS_DEBUG("AcquireOp: " << acqOp
                                 << " has no visible memory accesses; "
                                    "preserving existing mode "
                                 << acqOp.getMode());
        return;
      }

      bool loadsCoveredByLocalStores =
          hasLoads && hasStores &&
          loadsAreSatisfiedByDominatingStores(dbAnalysis, acqOp, func);

      ArtsMode originalMode = acqOp.getMode();
      ArtsMode newMode = ArtsMode::in;
      if (originalMode == ArtsMode::out && hasStores) {
        // An explicit out dependency is a scheduling contract established
        // before later pointer/db-ref rewrites obscure the original structured
        // memory facts. Do not widen it back to inout from conservative local
        // access rediscovery; in-place/read-modify-write codelets enter this
        // pass as readwrite/inout instead.
        newMode = ArtsMode::out;
      } else if (hasStores && (!hasLoads || loadsCoveredByLocalStores))
        newMode = ArtsMode::out;
      else if (hasLoads && hasStores)
        newMode = ArtsMode::inout;
      else
        newMode = ArtsMode::in;

      if (loadsCoveredByLocalStores) {
        ARTS_DEBUG("AcquireOp: " << acqOp
                                 << " reloads only task-local stored values; "
                                    "treating access as out");
      }

      if (hasStores && forceInout) {
        ARTS_DEBUG("AcquireOp: " << acqOp
                                 << " forcing inout mode (CARTS_FORCE_INOUT)");
        newMode = ArtsMode::inout;
      }

      if (newMode == ArtsMode::out) {
        DbAllocOp allocOp =
            accessSummary ? accessSummary->rootAlloc : DbAllocOp();
        LoopAnalysis &loopAnalysis = AM->getLoopAnalysis();
        bool fullWrite =
            !allocOp ||
            writesFullAllocation(dbAnalysis, acqOp, allocOp, loopAnalysis);
        bool proofTrustedPartitionedWrite =
            allocOp && !fullWrite &&
            canPreserveProofTrustedPartitionedWrite(acqOp);
        if (proofTrustedPartitionedWrite) {
          ARTS_DEBUG("AcquireOp: "
                     << acqOp
                     << " writes within a proof-trusted partitioned slice; "
                        "keeping out mode");
        } else if (allocOp && !fullWrite) {
          ARTS_DEBUG("AcquireOp: " << acqOp
                                   << " writes partial region; upgrading to "
                                      "inout to preserve untouched elements");
          newMode = ArtsMode::inout;
        }
      }

      /// Each acquire's mode is derived from its own accesses only.
      /// Nested acquires will be processed separately by DbAnalysis iteration.
      if (newMode == acqOp.getMode())
        return;

      ARTS_DEBUG("AcquireOp: " << acqOp << " from " << acqOp.getMode() << " to "
                               << newMode);
      acqOp.setModeAttr(ArtsModeAttr::get(acqOp.getContext(), newMode));
      if (newMode == ArtsMode::in)
        ++numAcquiresTightenedToRead;
      else if (newMode == ArtsMode::out)
        ++numAcquiresTightenedToOut;
      changed = true;
    });

    /// Then, adjust alloc dbMode - collect modes from all acquires in hierarchy
    dbAnalysis.forEachDbAlloc(func, [&](DbAllocOp allocOp) {
      std::optional<ArtsMode> maxMode =
          dbAnalysis.getCombinedAcquireModeForAlloc(allocOp);
      if (!maxMode) {
        ARTS_DEBUG("AllocOp: " << allocOp
                               << " has no child acquires with visible modes; "
                                  "preserving existing alloc mode");
        return;
      }

      /// Update the alloc mode
      ArtsMode currentDbMode = allocOp.getMode();
      if (currentDbMode != *maxMode) {
        ARTS_DEBUG("AllocOp: " << allocOp << " from " << currentDbMode << " to "
                               << *maxMode);
        allocOp.setModeAttr(ArtsModeAttr::get(allocOp.getContext(), *maxMode));
        ++numAllocModesAdjusted;
        changed = true;
      }

      DbMode targetDbMode = DbUtils::convertArtsModeToDbMode(*maxMode);
      if (allocOp.getDbMode() != targetDbMode) {
        ARTS_DEBUG("AllocOp: " << allocOp << " dbMode from "
                               << allocOp.getDbMode() << " to "
                               << targetDbMode);
        allocOp.setDbModeAttr(
            DbModeAttr::get(allocOp.getContext(), targetDbMode));
        ++numAllocDbModesAdjusted;
        changed = true;
      }
    });
  });

  ARTS_DEBUG_FOOTER(AdjustDBModes);
  return changed;
}

///===----------------------------------------------------------------------===///
/// Infer DB storage-type annotations.
/// Walks all DbAllocOps and sets UnitAttr annotations based on access patterns:
///   - arts.local_only: all acquires are intranode (no distributed contract)
///   - arts.read_only_after_init: after the first write, all subsequent
///   acquires
///     are read-only
///===----------------------------------------------------------------------===///
void DbModeTighteningPass::inferDbStorageTypes() {
  ARTS_DEBUG_HEADER(InferDbStorageTypes);

  module.walk([&](func::FuncOp func) {
    DbAnalysis &dbAnalysis = AM->getDbAnalysis();

    dbAnalysis.forEachDbAlloc(func, [&](DbAllocOp allocOp) {
      if (!allocOp)
        return;

      /// ---------------------------------------------------------------
      /// 1. Local-only (PIN candidate):
      ///    A DB is local-only when the alloc itself is not distributed
      ///    AND none of its acquires have a distribution contract.
      /// ---------------------------------------------------------------
      bool isLocalOnly = !hasDistributedDbAllocation(allocOp.getOperation());

      if (isLocalOnly &&
          dbAnalysis.allocationHasDistributedAcquireContract(allocOp))
        isLocalOnly = false;

      if (isLocalOnly) {
        allocOp.setLocalOnly(true);
        ++numDbsMarkedLocalOnly;
        ARTS_DEBUG("AllocOp: " << allocOp << " => local_only (PIN candidate)");
      }

      /// ---------------------------------------------------------------
      /// 2. Read-only-after-write:
      ///    Collect all acquires with their program order, then check if
      ///    after the first writer, every subsequent acquire is read-only.
      /// ---------------------------------------------------------------
      SmallVector<DbAnalysis::OrderedAcquireSummary, 16> orderedAcquires =
          dbAnalysis.getOrderedAcquiresForAlloc(allocOp);

      /// Need at least one acquire to reason about
      if (orderedAcquires.empty())
        return;

      /// Find the first writer and check all subsequent acquires. The
      /// read_only_after_init runtime hint is only valid when the initializer
      /// writer is a single dynamic write. A writer nested under repeatable
      /// control can execute once per loop iteration/epoch, even if static IR
      /// order shows all readers after it.
      bool foundWriter = false;
      bool allReadAfterWrite = true;
      unsigned writerCount = 0;
      bool writerInRepeatableControl = false;

      for (const auto &entry : orderedAcquires) {
        DbAcquireOp acquireOp = entry.acquire;
        ArtsMode mode = acquireOp.getMode();
        bool isWriter = DbUtils::isWriterMode(mode);

        if (!foundWriter) {
          if (isWriter) {
            foundWriter = true;
            writerInRepeatableControl =
                isInsideRepeatableControl(acquireOp.getOperation());
          }
          if (isWriter)
            ++writerCount;
          continue;
        }

        /// After the first writer, all must be read-only
        if (isWriter) {
          allReadAfterWrite = false;
          ++writerCount;
          continue;
        }
      }

      if (foundWriter && allReadAfterWrite && writerCount == 1 &&
          !writerInRepeatableControl) {
        allocOp.setReadOnlyAfterInit(true);
        ++numDbsMarkedReadOnlyAfterInit;
        ARTS_DEBUG("AllocOp: " << allocOp
                               << " => read_only_after_init (ONCE candidate)");
      }
    });
  });

  ARTS_DEBUG_FOOTER(InferDbStorageTypes);
}

///===----------------------------------------------------------------------===///
/// Stamp runtime dependency DB mode verdicts.
/// The ARTS layer owns policy decisions that can select unordered local
/// DB_MODE_RW. ARTS-RT lowering consumes this attr mechanically and no longer
/// re-discovers policy from dependency pattern/body facts.
///===----------------------------------------------------------------------===///
bool DbModeTighteningPass::stampRuntimeDbModeVerdicts() {
  ARTS_DEBUG_HEADER(StampRuntimeDBModeVerdicts);
  bool changed = false;

  module.walk([&](func::FuncOp func) {
    DbAnalysis &dbAnalysis = AM->getDbAnalysis();

    dbAnalysis.forEachDbAcquire(func, [&](DbAcquireOp acqOp) {
      if (!acqOp)
        return;

      RuntimeDbMode verdict =
          selectRuntimeDbModeVerdict(dbAnalysis, acqOp, module);
      if (acqOp.getRuntimeDbMode() && *acqOp.getRuntimeDbMode() == verdict)
        return;

      acqOp.setRuntimeDbModeAttr(
          RuntimeDbModeAttr::get(acqOp.getContext(), verdict));
      changed = true;
      ARTS_DEBUG("AcquireOp: " << acqOp << " runtime DB mode verdict "
                               << static_cast<int32_t>(verdict));
    });
  });

  ARTS_DEBUG_FOOTER(StampRuntimeDBModeVerdicts);
  return changed;
}

///===----------------------------------------------------------------------===///
/// Invalidate and rebuild the graph
///===----------------------------------------------------------------------===///
void DbModeTighteningPass::invalidateAndRebuildGraph() {
  AM->invalidateAndRebuildGraphs(module);
}

////===----------------------------------------------------------------------===////
/// Pass creation
////===----------------------------------------------------------------------===////
namespace mlir {
namespace carts::arts {
std::unique_ptr<Pass>
createDbModeTighteningPass(mlir::carts::arts::AnalysisManager *AM,
                           bool forceInout) {
  return std::make_unique<DbModeTighteningPass>(AM, forceInout);
}
} // namespace carts::arts
} // namespace mlir
