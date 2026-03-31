///==========================================================================///
/// File: DbModeTightening.cpp
/// Pass for DB mode tightening and storage-type inference.
/// Partitioning logic has been moved to DbPartitioning.cpp.
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
#include "mlir/IR/Dominance.h"
#include "mlir/IR/BuiltinOps.h"
/// Arts
#define GEN_PASS_DEF_DBMODETIGHTENING
#include "arts/Dialect.h"
#include "arts/passes/Passes.h"
#include "mlir/Pass/Pass.h"
#include "arts/passes/Passes.h.inc"
#include "arts/Dialect.h"
#include "arts/analysis/AnalysisManager.h"
#include "arts/analysis/graphs/db/DbGraph.h"
#include "arts/analysis/loop/LoopAnalysis.h"
#include "arts/passes/Passes.h"
/// Other
#include "mlir/Pass/Pass.h"
/// Debug
#include "arts/analysis/graphs/db/DbNode.h"
#include "arts/analysis/value/ValueAnalysis.h"
#include "arts/utils/BlockedAccessUtils.h"
#include "arts/utils/DbUtils.h"
#include "arts/utils/Debug.h"
#include "arts/utils/OperationAttributes.h"
#include "arts/utils/Utils.h"
#include <cstdlib>

ARTS_DEBUG_SETUP(db_mode_tightening);

using namespace mlir;
using namespace mlir::func;
using namespace mlir::arts;

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
  auto cmp =
      ValueAnalysis::stripNumericCasts(select.getCondition()).getDefiningOp<
          arith::CmpIOp>();
  if (!cmp)
    return false;

  Value lhs = ValueAnalysis::stripNumericCasts(cmp.getLhs());
  Value rhs = ValueAnalysis::stripNumericCasts(cmp.getRhs());
  auto pred = cmp.getPredicate();

  auto matchesZeroClamp = [&](Value zeroArm, Value otherArm) {
    if (!ValueAnalysis::isZeroConstant(zeroArm) ||
        !arts::isKnownNonPositive(otherArm))
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
  return arts::areEquivalentOwnedSliceExtents(ub, dimSize);
}

static bool isIndexFullCoverage(Value idx, Value dimSize,
                                ArrayRef<LoopNode *> loops) {
  if (!idx || !dimSize)
    return false;

  auto dimConstOpt = ValueAnalysis::tryFoldConstantIndex(
      ValueAnalysis::stripNumericCasts(dimSize));
  if (dimConstOpt && *dimConstOpt == 1)
    return true;

  idx = ValueAnalysis::stripNumericCasts(idx);

  /// Constant index only covers full range if size == 1.
  auto idxConstOpt = ValueAnalysis::tryFoldConstantIndex(idx);
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

static SmallVector<Value> getEffectiveSliceDimSizes(DbAcquireNode *acqNode,
                                                    DbAllocOp allocOp) {
  SmallVector<Value> dimSizes(allocOp.getElementSizes().begin(),
                              allocOp.getElementSizes().end());
  if (!acqNode || !allocOp || dimSizes.empty())
    return dimSizes;

  const DbAcquirePartitionFacts &facts = acqNode->getPartitionFacts();
  for (const DbPartitionEntryFact &entry : facts.entries) {
    if (!entry.mappedDim || !entry.representativeSize)
      continue;
    if (*entry.mappedDim < dimSizes.size())
      dimSizes[*entry.mappedDim] = entry.representativeSize;
  }

  if (std::optional<unsigned> dim = facts.inferSingleMappedDim()) {
    Value blockOffset;
    Value blockSize;
    if (succeeded(acqNode->computeBlockInfo(blockOffset, blockSize)) &&
        blockSize) {
      if (*dim < dimSizes.size()) {
        /// Graph-side block info refines the nominal block size to the
        /// acquire's actual owned slice, including dynamic tail blocks.
        dimSizes[*dim] = blockSize;
      }
    }
  }

  return dimSizes;
}

static bool writesFullAllocation(DbAcquireNode *acqNode, DbAllocOp allocOp,
                                 LoopAnalysis &loopAnalysis) {
  if (!acqNode || !allocOp)
    return true;
  if (!acqNode->hasStores())
    return true;

  EdtOp edt = acqNode->getEdtUser();
  if (!edt)
    return true;

  SmallVector<LoopNode *> loops;
  loopAnalysis.collectLoopsInOperation(edt, loops);

  DenseMap<DbRefOp, SetVector<Operation *>> dbRefToMemOps;
  acqNode->collectAccessOperations(dbRefToMemOps);
  if (dbRefToMemOps.empty())
    return true;

  SmallVector<Value> dimSizes = getEffectiveSliceDimSizes(acqNode, allocOp);
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
    return MemAccessSite{dbRef.getSource(),
                         SmallVector<Value, 4>(fullIndexChain.begin(),
                                               fullIndexChain.end())};
  if (auto store = dyn_cast<memref::StoreOp>(op))
    return MemAccessSite{dbRef.getSource(),
                         SmallVector<Value, 4>(fullIndexChain.begin(),
                                               fullIndexChain.end())};
  if (auto load = dyn_cast<affine::AffineLoadOp>(op))
    return MemAccessSite{dbRef.getSource(),
                         SmallVector<Value, 4>(fullIndexChain.begin(),
                                               fullIndexChain.end())};
  if (auto store = dyn_cast<affine::AffineStoreOp>(op))
    return MemAccessSite{dbRef.getSource(),
                         SmallVector<Value, 4>(fullIndexChain.begin(),
                                               fullIndexChain.end())};
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

static bool loadsAreSatisfiedByDominatingStores(DbAcquireNode *acqNode,
                                                func::FuncOp func) {
  if (!acqNode || !func)
    return false;

  DenseMap<DbRefOp, SetVector<Operation *>> dbRefToMemOps;
  acqNode->collectAccessOperations(dbRefToMemOps);
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

/// Recursively collect combined access modes from all acquire levels.
static void collectModesRecursive(DbAcquireNode *acqNode, ArtsMode &combined) {
  ArtsMode mode = acqNode->getDbAcquireOp().getMode();
  combined = combineAccessModes(combined, mode);
  acqNode->forEachChildNode([&](NodeBase *child) {
    if (auto *nestedAcq = dyn_cast<DbAcquireNode>(child))
      collectModesRecursive(nestedAcq, combined);
  });
}

/// Recursively check whether any acquire has a distribution contract.
static void checkDistributedRecursive(DbAcquireNode *acqNode,
                                      bool &isLocalOnly) {
  if (!isLocalOnly)
    return;
  const DbAcquirePartitionFacts &facts = acqNode->getPartitionFacts();
  if (facts.hasDistributionContract) {
    isLocalOnly = false;
    return;
  }
  acqNode->forEachChildNode([&](NodeBase *child) {
    if (auto *nestedAcq = dyn_cast<DbAcquireNode>(child))
      checkDistributedRecursive(nestedAcq, isLocalOnly);
  });
}

/// Entry for collectAcquiresRecursive: acquire info with program order.
struct AcquireOrderEntry {
  unsigned order;
  DbAcquireNode *node;
};

/// Recursively collect all acquire nodes with their program order.
static void collectAcquiresRecursive(DbAcquireNode *acqNode, DbGraph &graph,
                                     SmallVectorImpl<AcquireOrderEntry> &out) {
  DbAcquireOp acqOp = acqNode->getDbAcquireOp();
  unsigned order = graph.getOpOrder(acqOp.getOperation());
  out.push_back({order, acqNode});
  acqNode->forEachChildNode([&](NodeBase *child) {
    if (auto *nestedAcq = dyn_cast<DbAcquireNode>(child))
      collectAcquiresRecursive(nestedAcq, graph, out);
  });
}

} // namespace

///===----------------------------------------------------------------------===///
/// Pass Implementation
/// Tighten Datablock access modes and infer storage types
///===----------------------------------------------------------------------===///
namespace {
struct DbModeTighteningPass
    : public impl::DbModeTighteningBase<DbModeTighteningPass> {
  DbModeTighteningPass(mlir::arts::AnalysisManager *AM) : AM(AM) {
    assert(AM && "AnalysisManager must be provided externally");
  }

  void runOnOperation() override;

  /// Mode adjustment
  bool adjustDbModes();

  /// DB storage-type inference annotations
  void inferDbStorageTypes();

  /// Graph rebuild
  void invalidateAndRebuildGraph();

private:
  ModuleOp module;
  mlir::arts::AnalysisManager *AM = nullptr;
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
  bool forceInout = std::getenv("CARTS_FORCE_INOUT") != nullptr;

  module.walk([&](func::FuncOp func) {
    DbGraph &graph = AM->getDbAnalysis().getOrCreateGraph(func);

    /// First, adjust per-acquire modes
    graph.forEachAcquireNode([&](DbAcquireNode *acqNode) {
      bool hasLoads = acqNode->hasLoads();
      bool hasStores = acqNode->hasStores();

      DbAcquireOp acqOp = acqNode->getDbAcquireOp();
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
          loadsAreSatisfiedByDominatingStores(acqNode, func);

      ArtsMode newMode = ArtsMode::in;
      if (hasStores && (!hasLoads || loadsCoveredByLocalStores))
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
        DbAllocNode *allocNode = acqNode->getRootAlloc();
        DbAllocOp allocOp = allocNode ? allocNode->getDbAllocOp() : DbAllocOp();
        LoopAnalysis &loopAnalysis = AM->getLoopAnalysis();
        bool fullWrite =
            !allocOp || writesFullAllocation(acqNode, allocOp, loopAnalysis);
        if (allocOp && !fullWrite) {
          ARTS_DEBUG("AcquireOp: " << acqOp
                                   << " writes partial region; upgrading to "
                                      "inout to preserve untouched elements");
          newMode = ArtsMode::inout;
        }
      }

      /// Each DbAcquireNode's mode is derived from its own accesses only.
      /// Nested acquires will be processed separately by forEachAcquireNode.
      if (newMode == acqOp.getMode())
        return;

      ARTS_DEBUG("AcquireOp: " << acqOp << " from " << acqOp.getMode() << " to "
                               << newMode);
      acqOp.setModeAttr(ArtsModeAttr::get(acqOp.getContext(), newMode));
      changed = true;
    });

    /// Then, adjust alloc dbMode - collect modes from all acquires in hierarchy
    graph.forEachAllocNode([&](DbAllocNode *allocNode) {
      ArtsMode maxMode = ArtsMode::in;
      bool sawAcquireMode = false;

      allocNode->forEachChildNode([&](NodeBase *child) {
        if (auto *acqNode = dyn_cast<DbAcquireNode>(child)) {
          sawAcquireMode = true;
          collectModesRecursive(acqNode, maxMode);
        }
      });

      if (!sawAcquireMode) {
        ARTS_DEBUG("AllocOp: " << allocNode->getDbAllocOp()
                               << " has no child acquires with visible modes; "
                                  "preserving existing alloc mode");
        return;
      }

      /// Update the alloc mode
      DbAllocOp allocOp = allocNode->getDbAllocOp();
      ArtsMode currentDbMode = allocOp.getMode();
      if (currentDbMode == maxMode)
        return;
      ARTS_DEBUG("AllocOp: " << allocOp << " from " << currentDbMode << " to "
                             << maxMode);
      allocOp.setModeAttr(ArtsModeAttr::get(allocOp.getContext(), maxMode));

      changed = true;
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
    DbGraph &graph = AM->getDbAnalysis().getOrCreateGraph(func);

    graph.forEachAllocNode([&](DbAllocNode *allocNode) {
      DbAllocOp allocOp = allocNode->getDbAllocOp();
      if (!allocOp)
        return;

      /// ---------------------------------------------------------------
      /// 1. Local-only (PIN candidate):
      ///    A DB is local-only when the alloc itself is not distributed
      ///    AND none of its acquires have a distribution contract.
      /// ---------------------------------------------------------------
      bool isLocalOnly = !hasDistributedDbAllocation(allocOp.getOperation());

      if (isLocalOnly) {
        /// Check all acquire children (recursively) for distribution contracts
        allocNode->forEachChildNode([&](NodeBase *child) {
          if (auto *acqNode = dyn_cast<DbAcquireNode>(child))
            checkDistributedRecursive(acqNode, isLocalOnly);
        });
      }

      if (isLocalOnly) {
        allocOp->setAttr(AttrNames::Operation::LocalOnly,
                         UnitAttr::get(allocOp.getContext()));
        ARTS_DEBUG("AllocOp: " << allocOp << " => local_only (PIN candidate)");
      }

      /// ---------------------------------------------------------------
      /// 2. Read-only-after-write:
      ///    Collect all acquires with their program order, then check if
      ///    after the first writer, every subsequent acquire is read-only.
      /// ---------------------------------------------------------------
      SmallVector<AcquireOrderEntry> orderedAcquires;

      allocNode->forEachChildNode([&](NodeBase *child) {
        if (auto *acqNode = dyn_cast<DbAcquireNode>(child))
          collectAcquiresRecursive(acqNode, graph, orderedAcquires);
      });

      /// Need at least one acquire to reason about
      if (orderedAcquires.empty())
        return;

      /// Sort by program order
      llvm::sort(orderedAcquires,
                 [](const AcquireOrderEntry &a, const AcquireOrderEntry &b) {
                   return a.order < b.order;
                 });

      /// Find the first writer and check all subsequent acquires
      bool foundWriter = false;
      bool allReadAfterWrite = true;

      for (const auto &entry : orderedAcquires) {
        ArtsMode mode = entry.node->getDbAcquireOp().getMode();
        bool isWriter = DbUtils::isWriterMode(mode);

        if (!foundWriter) {
          if (isWriter)
            foundWriter = true;
          continue;
        }

        /// After the first writer, all must be read-only
        if (isWriter) {
          allReadAfterWrite = false;
          break;
        }
      }

      if (foundWriter && allReadAfterWrite) {
        allocOp->setAttr(AttrNames::Operation::ReadOnlyAfterInit,
                         UnitAttr::get(allocOp.getContext()));
        ARTS_DEBUG("AllocOp: " << allocOp
                               << " => read_only_after_init (ONCE candidate)");
      }
    });
  });

  ARTS_DEBUG_FOOTER(InferDbStorageTypes);
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
namespace arts {
std::unique_ptr<Pass>
createDbModeTighteningPass(mlir::arts::AnalysisManager *AM) {
  return std::make_unique<DbModeTighteningPass>(AM);
}
} // namespace arts
} // namespace mlir
