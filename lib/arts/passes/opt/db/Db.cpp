///==========================================================================///
/// File: Db.cpp
/// Pass for DB mode adjustments based on access patterns.
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
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/BuiltinOps.h"
/// Arts
#include "../../PassDetails.h"
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
#include "arts/utils/DbUtils.h"
#include "arts/utils/Debug.h"
#include "arts/utils/OperationAttributes.h"
#include "arts/utils/Utils.h"
#include <cstdlib>

ARTS_DEBUG_SETUP(db);

using namespace mlir;
using namespace mlir::func;
using namespace mlir::arts;

namespace {

static bool isLoopFullRange(LoopNode *loop, Value dimSize) {
  if (!loop || !dimSize)
    return false;

  Value lb = loop->getLowerBound();
  Value step = loop->getStep();
  Value ub = loop->getUpperBound();
  if (!lb || !step || !ub)
    return false;

  if (!ValueAnalysis::isZeroConstant(ValueAnalysis::stripNumericCasts(lb)))
    return false;
  if (!ValueAnalysis::isOneConstant(ValueAnalysis::stripNumericCasts(step)))
    return false;
  return ValueAnalysis::sameValue(ValueAnalysis::stripNumericCasts(ub),
                                  dimSize);
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
    return dimConstOpt && *dimConstOpt == 1 && *idxConstOpt == 0;

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

  ValueRange elemSizes = allocOp.getElementSizes();
  if (elemSizes.empty())
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
      unsigned sizeRank = elemSizes.size();
      unsigned checkRank = std::min(memrefRank, sizeRank);

      for (unsigned d = 0; d < checkRank; ++d) {
        Value idx = indexChain[memrefStart + d];
        Value dimSize = elemSizes[d];
        if (!isIndexFullCoverage(idx, dimSize, loops))
          return false;
      }

      if (memrefRank != sizeRank)
        return false;
    }
  }

  return true;
}

} // namespace

///===----------------------------------------------------------------------===///
/// Pass Implementation
/// Adjust Datablock modes based on access patterns
///===----------------------------------------------------------------------===///
namespace {
struct DbPass : public arts::DbBase<DbPass> {
  DbPass(mlir::arts::AnalysisManager *AM) : AM(AM) {
    assert(AM && "AnalysisManager must be provided externally");
  }

  void runOnOperation() override;

  /// Mode adjustment
  bool adjustDbModes();

  /// Graph rebuild
  void invalidateAndRebuildGraph();

private:
  ModuleOp module;
  mlir::arts::AnalysisManager *AM = nullptr;
};
} // namespace

void DbPass::runOnOperation() {
  module = getOperation();

  ARTS_INFO_HEADER(DbPass);
  ARTS_DEBUG_REGION(module.dump(););

  assert(AM && "AnalysisManager must be provided externally");

  /// Graph construction and analysis
  invalidateAndRebuildGraph();

  /// Adjust DB modes based on access patterns
  bool changed = adjustDbModes();

  if (changed) {
    ARTS_INFO(" Module has changed, invalidating analyses");
    AM->invalidate();
  }

  ARTS_INFO_FOOTER(DbPass);
  ARTS_DEBUG_REGION(module.dump(););
}

///===----------------------------------------------------------------------===///
/// Adjust DB modes based on accesses.
/// Based on the collected loads and stores, adjust the DB mode to in, out, or
/// inout.
///===----------------------------------------------------------------------===///
bool DbPass::adjustDbModes() {
  ARTS_DEBUG_HEADER(AdjustDBModes);
  bool changed = false;

  module.walk([&](func::FuncOp func) {
    DbGraph &graph = AM->getDbGraph(func);

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

      ArtsMode newMode = ArtsMode::in;
      if (hasLoads && hasStores)
        newMode = ArtsMode::inout;
      else if (hasStores)
        newMode = ArtsMode::out;
      else
        newMode = ArtsMode::in;

      if (hasStores && std::getenv("CARTS_FORCE_INOUT")) {
        ARTS_DEBUG("AcquireOp: " << acqOp
                                 << " forcing inout mode (CARTS_FORCE_INOUT)");
        newMode = ArtsMode::inout;
      }

      if (newMode == ArtsMode::out) {
        DbAllocNode *allocNode = acqNode->getRootAlloc();
        DbAllocOp allocOp = allocNode ? allocNode->getDbAllocOp() : DbAllocOp();
        LoopAnalysis &loopAnalysis = AM->getLoopAnalysis();
        if (allocOp && !writesFullAllocation(acqNode, allocOp, loopAnalysis)) {
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

      /// Recursive helper to collect modes from all acquire levels
      std::function<void(DbAcquireNode *)> collectModes =
          [&](DbAcquireNode *acqNode) {
            ArtsMode mode = acqNode->getDbAcquireOp().getMode();
            maxMode = combineAccessModes(maxMode, mode);
            acqNode->forEachChildNode([&](NodeBase *child) {
              if (auto *nestedAcq = dyn_cast<DbAcquireNode>(child))
                collectModes(nestedAcq);
            });
          };

      allocNode->forEachChildNode([&](NodeBase *child) {
        if (auto *acqNode = dyn_cast<DbAcquireNode>(child))
          collectModes(acqNode);
      });

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
/// Invalidate and rebuild the graph
///===----------------------------------------------------------------------===///
void DbPass::invalidateAndRebuildGraph() {
  module.walk([&](func::FuncOp func) {
    AM->invalidateFunction(func);
    (void)AM->getDbGraph(func);
  });
}

////===----------------------------------------------------------------------===////
/// Pass creation
////===----------------------------------------------------------------------===////
namespace mlir {
namespace arts {
std::unique_ptr<Pass> createDbPass(mlir::arts::AnalysisManager *AM) {
  return std::make_unique<DbPass>(AM);
}
} // namespace arts
} // namespace mlir
