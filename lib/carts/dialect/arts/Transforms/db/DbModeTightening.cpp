///==========================================================================///
/// File: DbModeTightening.cpp
/// Pass for DB mode tightening and storage-type inference.
/// This pass reconciles DB modes and storage policy against committed IR facts
/// and observed uses.
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
#include "carts/dialect/arts/IR/ArtsDialect.h"
#include "carts/passes/Passes.h"
#include "mlir/Pass/Pass.h"
/// Debug
#include "carts/dialect/arts/Utils/BlockedAccessUtils.h"
#include "carts/dialect/arts/Utils/DbUtils.h"
#include "carts/dialect/arts/Utils/EdtUtils.h"
#include "carts/dialect/arts/Utils/LoweringFactUtils.h"
#include "carts/dialect/arts/Utils/LoopStructureUtils.h"
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

struct AcquireAccessSummary {
  DbAcquireOp acquire;
  DbAllocOp rootAlloc;
  EdtOp edtUser;
  bool hasLoads = false;
  bool hasStores = false;
};

struct OrderedAcquireSummary {
  unsigned order = 0;
  DbAcquireOp acquire;
};

struct LoopInfo {
  scf::ForOp loop;
  Value iv;
  Value lowerBound;
  Value upperBound;
  Value step;
  unsigned depth = 0;
};

using AcquireAccessOperationMap = DenseMap<DbRefOp, SetVector<Operation *>>;

static DbAllocOp getRootAlloc(DbAcquireOp acquire) {
  if (!acquire)
    return {};
  return dyn_cast_or_null<DbAllocOp>(
      DbUtils::getUnderlyingDbAlloc(acquire.getSourcePtr()));
}

static bool acquireBelongsToAlloc(DbAcquireOp acquire, DbAllocOp alloc) {
  return acquire && alloc && getRootAlloc(acquire) == alloc;
}

static void forEachDbAcquire(func::FuncOp func,
                             llvm::function_ref<void(DbAcquireOp)> fn) {
  func.walk([&](DbAcquireOp acquire) { fn(acquire); });
}

static void forEachDbAlloc(func::FuncOp func,
                           llvm::function_ref<void(DbAllocOp)> fn) {
  func.walk([&](DbAllocOp alloc) { fn(alloc); });
}

static void collectAcquireAccessOperations(DbAcquireOp acquire,
                                           AcquireAccessOperationMap &result) {
  if (!acquire)
    return;
  auto [edt, blockArg] = EdtUtils::getBlockArgumentForAcquire(acquire);
  if (!edt || !blockArg || !edt->getParentRegion())
    return;

  SmallVector<Value, 16> worklist{blockArg};
  SetVector<Value> visited;
  while (!worklist.empty()) {
    Value current = worklist.pop_back_val();
    if (!visited.insert(current))
      continue;

    for (Operation *user : current.getUsers()) {
      Region *userRegion = user->getParentRegion();
      if (!userRegion || !edt.getBody().isAncestor(userRegion))
        continue;

      if (auto dbRef = dyn_cast<DbRefOp>(user)) {
        result.try_emplace(dbRef);
        Value refResult = dbRef.getResult();
        worklist.push_back(refResult);
        SetVector<Operation *> memOps;
        DbUtils::collectReachableMemoryOps(refResult, memOps, &edt.getBody());
        for (Operation *memOp : memOps)
          result[dbRef].insert(memOp);
      }
    }
  }
}

static AcquireAccessSummary getAcquireAccessSummary(DbAcquireOp acquire) {
  AcquireAccessSummary summary;
  summary.acquire = acquire;
  summary.rootAlloc = getRootAlloc(acquire);
  auto [edt, blockArg] = EdtUtils::getBlockArgumentForAcquire(acquire);
  (void)blockArg;
  summary.edtUser = edt;

  AcquireAccessOperationMap accesses;
  collectAcquireAccessOperations(acquire, accesses);
  for (auto &[dbRef, memOps] : accesses) {
    (void)dbRef;
    for (Operation *memOp : memOps) {
      auto access = DbUtils::getMemoryAccessInfo(memOp);
      if (!access)
        continue;
      summary.hasLoads |= access->isRead();
      summary.hasStores |= access->isWrite();
    }
  }
  return summary;
}

static std::optional<ArtsMode> getCombinedAcquireModeForAlloc(DbAllocOp alloc) {
  if (!alloc)
    return std::nullopt;

  auto func = alloc->getParentOfType<func::FuncOp>();
  if (!func)
    return std::nullopt;

  ArtsMode combined = ArtsMode::in;
  bool sawAcquire = false;
  func.walk([&](DbAcquireOp acquire) {
    if (!acquireBelongsToAlloc(acquire, alloc))
      return;
    combined = combineAccessModes(combined, acquire.getMode());
    sawAcquire = true;
  });
  if (!sawAcquire)
    return std::nullopt;
  return combined;
}

static bool acquireHasDistributionFacts(DbAcquireOp acquire) {
  if (!acquire)
    return false;
  if (auto facts = resolveAcquireFacts(acquire))
    if (facts->hasDistributionFacts())
      return true;
  auto [edt, blockArg] = EdtUtils::getBlockArgumentForAcquire(acquire);
  (void)blockArg;
  if (edt && (getEdtDistributionKind(edt.getOperation()) ||
              getEdtDistributionPattern(edt.getOperation())))
    return true;
  if (auto epoch = acquire->getParentOfType<EpochOp>())
    return getEdtDistributionKind(epoch.getOperation()) ||
           getEdtDistributionPattern(epoch.getOperation());
  return false;
}

static bool allocationHasDistributedAcquireFacts(DbAllocOp alloc) {
  if (!alloc)
    return false;
  auto func = alloc->getParentOfType<func::FuncOp>();
  if (!func)
    return false;

  bool found = false;
  func.walk([&](DbAcquireOp acquire) {
    if (found)
      return WalkResult::interrupt();
    if (acquireBelongsToAlloc(acquire, alloc) &&
        acquireHasDistributionFacts(acquire)) {
      found = true;
      return WalkResult::interrupt();
    }
    return WalkResult::advance();
  });
  return found;
}

static SmallVector<OrderedAcquireSummary, 16>
getOrderedAcquiresForAlloc(DbAllocOp alloc) {
  SmallVector<OrderedAcquireSummary, 16> ordered;
  if (!alloc)
    return ordered;
  auto func = alloc->getParentOfType<func::FuncOp>();
  if (!func)
    return ordered;

  unsigned order = 0;
  func.walk([&](Operation *op) {
    if (auto acquire = dyn_cast<DbAcquireOp>(op))
      if (acquireBelongsToAlloc(acquire, alloc))
        ordered.push_back({order, acquire});
    ++order;
  });
  return ordered;
}

static void collectLoops(Operation *scope, SmallVectorImpl<LoopInfo> &loops) {
  if (!scope)
    return;
  scope->walk([&](scf::ForOp loop) {
    loops.push_back({loop, loop.getInductionVar(), loop.getLowerBound(),
                     loop.getUpperBound(), loop.getStep(),
                     getLoopDepth(loop.getOperation())});
  });
}

static SmallVector<Value> getEffectiveSliceDimSizes(DbAcquireOp acquire,
                                                    DbAllocOp allocOp) {
  SmallVector<Value> dimSizes;
  if (allocOp)
    dimSizes.append(allocOp.getElementSizes().begin(),
                    allocOp.getElementSizes().end());
  if (!acquire || !allocOp || dimSizes.empty())
    return dimSizes;

  auto facts = resolveAcquireFacts(acquire);
  if (!facts || facts->spatial.ownerDims.empty())
    return dimSizes;

  ValueRange partitionSizes = acquire.getPartitionSizes();
  if (partitionSizes.empty())
    partitionSizes = acquire.getSizes();
  for (auto [idx, size] : llvm::enumerate(partitionSizes)) {
    if (idx >= facts->spatial.ownerDims.size())
      break;
    int64_t dim = facts->spatial.ownerDims[idx];
    if (dim >= 0 && static_cast<size_t>(dim) < dimSizes.size())
      dimSizes[dim] = size;
  }
  return dimSizes;
}

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

static bool isLoopFullRange(const LoopInfo &loop, Value dimSize) {
  if (!loop.loop || !dimSize)
    return false;

  Value lb = loop.lowerBound;
  Value step = loop.step;
  Value ub = loop.upperBound;
  if (!lb || !step || !ub)
    return false;

  if (!isProvablyZeroLoopLowerBound(lb))
    return false;
  if (!ValueAnalysis::isOneConstant(ValueAnalysis::stripNumericCasts(step)))
    return false;
  return areEquivalentOwnedSliceExtents(ub, dimSize);
}

static bool isIndexFullCoverage(Value idx, Value dimSize,
                                ArrayRef<LoopInfo> loops) {
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

  auto foldArtsConstantIndex = [](Value value,
                                  unsigned depth) -> std::optional<int64_t> {
    return arts::tryFoldConstantIndex(ValueAnalysis::stripNumericCasts(value),
                                      depth);
  };

  const LoopInfo *best = nullptr;
  ValueAnalysis::IndexExpr bestExpr;
  int bestDepth = -1;

  for (const LoopInfo &loop : loops) {
    if (!loop.loop)
      continue;
    auto expr = ValueAnalysis::analyzeIndexExprWith(idx, loop.iv,
                                                    foldArtsConstantIndex);
    if (!expr.dependsOnIV)
      continue;
    int depth = loop.depth;
    if (!best || depth > bestDepth) {
      best = &loop;
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

  return isLoopFullRange(*best, dimSize);
}

static bool writesFullAllocation(DbAcquireOp acquire, DbAllocOp allocOp) {
  if (!acquire || !allocOp)
    return true;

  AcquireAccessSummary accessSummary = getAcquireAccessSummary(acquire);
  if (!accessSummary.hasStores)
    return true;

  EdtOp edt = accessSummary.edtUser;
  if (!edt)
    return true;

  SmallVector<LoopInfo, 8> loops;
  collectLoops(edt, loops);

  AcquireAccessOperationMap dbRefToMemOps;
  collectAcquireAccessOperations(acquire, dbRefToMemOps);
  if (dbRefToMemOps.empty())
    return true;

  SmallVector<Value> dimSizes = getEffectiveSliceDimSizes(acquire, allocOp);
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
  return false;
}

static RuntimeDbMode getOrderedRuntimeDbMode(ArtsMode mode) {
  return DbUtils::orderedRuntimeDbMode(mode);
}

static bool hasTrustedPartitionedWriteFacts(DbAcquireOp acquire) {
  if (!acquire)
    return false;

  std::optional<PartitionMode> mode = acquire.getPartitionMode();
  if (!mode || !usesBlockLayout(*mode))
    return false;

  bool hasPartitionWindow =
      (!acquire.getPartitionOffsets().empty() &&
       !acquire.getPartitionSizes().empty()) ||
      (!acquire.getOffsets().empty() && !acquire.getSizes().empty());
  if (!hasPartitionWindow)
    return false;

  if (auto facts = resolveAcquireFacts(acquire))
    if (facts->hasExplicitStencilFacts() && facts->supportsBlockHalo() &&
        facts->hasOwnerDims())
      return true;

  return false;
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
  return hasTrustedPartitionedWriteFacts(acquire);
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

static bool canUseSingleNodeCoarseUnorderedOutWrite(DbAcquireOp acquire,
                                                    EdtOp edtOp,
                                                    ModuleOp module,
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
  std::optional<PartitionMode> partitionMode = acquire.getPartitionMode();
  if (!partitionMode || *partitionMode != PartitionMode::coarse)
    return false;
  auto alloc = dyn_cast_or_null<DbAllocOp>(
      DbUtils::getUnderlyingDbAlloc(acquire.getSourcePtr()));
  if (!alloc || !alloc.getLocalOnly().value_or(false))
    return false;
  auto depPattern = getDepPattern(edtOp.getOperation());
  if (!depPattern || !isInPlaceSafeUnorderedPattern(*depPattern))
    return false;

  if (acquire.getMode() == ArtsMode::inout && payloadMayRead)
    return false;

  return true;
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
  std::optional<PartitionMode> partitionMode = acquire.getPartitionMode();
  if (!partitionMode || *partitionMode != PartitionMode::coarse)
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

static RuntimeDbMode selectRuntimeDbModeVerdict(DbAcquireOp acquire,
                                                ModuleOp module) {
  if (!acquire)
    return RuntimeDbMode::ew;

  RuntimeDbMode orderedMode = getOrderedRuntimeDbMode(acquire.getMode());
  if (orderedMode == RuntimeDbMode::ro)
    return orderedMode;

  AcquireAccessSummary accessSummary = getAcquireAccessSummary(acquire);
  EdtOp edtOp = accessSummary.edtUser;
  bool payloadMayRead = accessSummary.hasLoads;
  std::optional<unsigned> depIndex = getDependencyIndex(edtOp, acquire);

  if (canUseUnorderedLocalWrite(acquire, edtOp, module) ||
      canUseSingleNodeCoarseUnorderedOutWrite(acquire, edtOp, module,
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

static bool loadsAreSatisfiedByDominatingStores(DbAcquireOp acquire,
                                                func::FuncOp func) {
  if (!acquire || !func)
    return false;

  AcquireAccessOperationMap dbRefToMemOps;
  collectAcquireAccessOperations(acquire, dbRefToMemOps);
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
  explicit DbModeTighteningPass(bool forceInout) {
    this->forceInout = forceInout;
  }

  void runOnOperation() override;

  /// Mode adjustment
  bool adjustDbModes();

  /// DB storage-type inference annotations
  void inferDbStorageTypes();

  /// Runtime dependency DB mode verdict annotations
  bool commitRuntimeDbModeVerdicts();

private:
  ModuleOp module;
};
} // namespace

void DbModeTighteningPass::runOnOperation() {
  module = getOperation();

  ARTS_INFO_HEADER(DbModeTighteningPass);
  ARTS_DEBUG_REGION(module.dump(););

  /// Adjust DB modes based on access patterns
  bool changed = adjustDbModes();

  /// Infer DB storage-type annotations (local_only, read_only_after_init)
  inferDbStorageTypes();

  (void)changed;
  (void)commitRuntimeDbModeVerdicts();

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
    /// First, adjust per-acquire modes
    forEachDbAcquire(func, [&](DbAcquireOp acqOp) {
      AcquireAccessSummary accessSummary = getAcquireAccessSummary(acqOp);
      bool hasLoads = accessSummary.hasLoads;
      bool hasStores = accessSummary.hasStores;

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
      /// access facts. Later lowering stages may still rely on that mode
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
          loadsAreSatisfiedByDominatingStores(acqOp, func);

      ArtsMode originalMode = acqOp.getMode();
      ArtsMode newMode = ArtsMode::in;
      if (originalMode == ArtsMode::out && hasStores) {
        // An explicit out dependency is a scheduling facts established
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
        DbAllocOp allocOp = accessSummary.rootAlloc;
        bool fullWrite = !allocOp || writesFullAllocation(acqOp, allocOp);
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
      /// Nested acquires are visited separately by the direct IR walk.
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
    forEachDbAlloc(func, [&](DbAllocOp allocOp) {
      std::optional<ArtsMode> maxMode = getCombinedAcquireModeForAlloc(allocOp);
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
///   - arts.local_only: all acquires are intranode (no distributed facts)
///   - arts.read_only_after_init: after the first write, all subsequent
///   acquires
///     are read-only
///===----------------------------------------------------------------------===///
void DbModeTighteningPass::inferDbStorageTypes() {
  ARTS_DEBUG_HEADER(InferDbStorageTypes);

  module.walk([&](func::FuncOp func) {
    forEachDbAlloc(func, [&](DbAllocOp allocOp) {
      if (!allocOp)
        return;

      /// ---------------------------------------------------------------
      /// 1. Local-only (PIN candidate):
      ///    A DB is local-only when the alloc itself is not distributed
      ///    AND none of its acquires have a distribution facts.
      /// ---------------------------------------------------------------
      bool isLocalOnly = !hasDistributedDbAllocation(allocOp.getOperation());

      if (isLocalOnly && allocationHasDistributedAcquireFacts(allocOp))
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
      SmallVector<OrderedAcquireSummary, 16> orderedAcquires =
          getOrderedAcquiresForAlloc(allocOp);

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
/// Commit runtime dependency DB mode verdicts.
/// The ARTS layer owns policy decisions that can select unordered local
/// DB_MODE_RW. ARTS-RT lowering consumes this attr mechanically and no longer
/// re-discovers policy from dependency pattern/body facts.
///===----------------------------------------------------------------------===///
bool DbModeTighteningPass::commitRuntimeDbModeVerdicts() {
  ARTS_DEBUG_HEADER(CommitRuntimeDBModeVerdicts);
  bool changed = false;

  module.walk([&](func::FuncOp func) {
    forEachDbAcquire(func, [&](DbAcquireOp acqOp) {
      if (!acqOp)
        return;

      RuntimeDbMode verdict = selectRuntimeDbModeVerdict(acqOp, module);
      if (acqOp.getRuntimeDbMode() && *acqOp.getRuntimeDbMode() == verdict)
        return;

      acqOp.setRuntimeDbModeAttr(
          RuntimeDbModeAttr::get(acqOp.getContext(), verdict));
      changed = true;
      ARTS_DEBUG("AcquireOp: " << acqOp << " runtime DB mode verdict "
                               << static_cast<int32_t>(verdict));
    });
  });

  ARTS_DEBUG_FOOTER(CommitRuntimeDBModeVerdicts);
  return changed;
}

////===----------------------------------------------------------------------===////
/// Pass creation
////===----------------------------------------------------------------------===////
namespace mlir {
namespace carts::arts {
std::unique_ptr<Pass> createDbModeTighteningPass(bool forceInout) {
  return std::make_unique<DbModeTighteningPass>(forceInout);
}
} // namespace carts::arts
} // namespace mlir
