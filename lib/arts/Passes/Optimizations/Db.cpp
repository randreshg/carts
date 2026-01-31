///==========================================================================///
/// File: Db.cpp
/// Pass for DB mode adjustments based on access patterns.
/// Partitioning logic has been moved to DbPartitioning.cpp.
///==========================================================================///

/// Dialects
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/BuiltinOps.h"
/// Arts
#include "../ArtsPassDetails.h"
#include "arts/Analysis/ArtsAnalysisManager.h"
#include "arts/Analysis/Graphs/Db/DbGraph.h"
#include "arts/Analysis/Loop/LoopAnalysis.h"
#include "arts/ArtsDialect.h"
#include "arts/Passes/ArtsPasses.h"
/// Other
#include "mlir/Pass/Pass.h"
/// Debug
#include "arts/Analysis/Graphs/Db/DbNode.h"
#include "arts/Utils/ArtsDebug.h"
#include "arts/Utils/ArtsUtils.h"
#include "arts/Utils/DatablockUtils.h"
#include "arts/Utils/ValueUtils.h"
#include <cstdlib>

ARTS_DEBUG_SETUP(db);

using namespace mlir;
using namespace mlir::func;
using namespace mlir::arts;

namespace {

static bool isConstIndex(Value v, int64_t &out) {
  if (!v)
    return false;
  return ValueUtils::getConstantIndex(ValueUtils::stripNumericCasts(v), out);
}

static bool isConstZero(Value v) {
  int64_t val = 0;
  return isConstIndex(v, val) && val == 0;
}

static bool isConstOne(Value v) {
  int64_t val = 0;
  return isConstIndex(v, val) && val == 1;
}

static bool isSameValueOrConst(Value a, Value b) {
  if (!a || !b)
    return false;
  Value aStripped = ValueUtils::stripNumericCasts(a);
  Value bStripped = ValueUtils::stripNumericCasts(b);
  if (aStripped == bStripped)
    return true;
  int64_t aConst = 0;
  int64_t bConst = 0;
  if (isConstIndex(aStripped, aConst) && isConstIndex(bStripped, bConst))
    return aConst == bConst;
  return false;
}

static bool isLoopFullRange(LoopNode *loop, Value dimSize) {
  if (!loop || !dimSize)
    return false;
  auto *op = loop->getLoopOp();
  auto forOp = dyn_cast_or_null<scf::ForOp>(op);
  if (!forOp)
    return false;

  Value lb = ValueUtils::stripNumericCasts(forOp.getLowerBound());
  if (!isConstZero(lb))
    return false;

  Value step = ValueUtils::stripNumericCasts(forOp.getStep());
  if (!isConstOne(step))
    return false;

  Value ub = ValueUtils::stripNumericCasts(forOp.getUpperBound());
  return isSameValueOrConst(ub, dimSize);
}

static bool isIndexFullCoverage(Value idx, Value dimSize,
                                ArrayRef<LoopNode *> loops) {
  if (!idx || !dimSize)
    return false;

  int64_t dimConst = 0;
  if (isConstIndex(dimSize, dimConst) && dimConst == 1)
    return true;

  idx = ValueUtils::stripNumericCasts(idx);

  int64_t idxConst = 0;
  /// Constant index only covers full range if size == 1.
  if (isConstIndex(idx, idxConst))

    return dimConst == 1 && idxConst == 0;

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
          DatablockUtils::collectFullIndexChain(dbRef, memOp);
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
// Pass Implementation
// Adjust Datablock modes based on access patterns
///===----------------------------------------------------------------------===///
namespace {
struct DbPass : public arts::DbBase<DbPass> {
  DbPass(ArtsAnalysisManager *AM) : AM(AM) {
    assert(AM && "ArtsAnalysisManager must be provided externally");
  }

  void runOnOperation() override;

  /// Mode adjustment
  bool adjustDbModes();

  /// Graph rebuild
  void invalidateAndRebuildGraph();

private:
  ModuleOp module;
  ArtsAnalysisManager *AM = nullptr;
};
} // namespace

void DbPass::runOnOperation() {
  module = getOperation();

  ARTS_INFO_HEADER(DbPass);
  ARTS_DEBUG_REGION(module.dump(););

  assert(AM && "ArtsAnalysisManager must be provided externally");

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
std::unique_ptr<Pass> createDbPass(ArtsAnalysisManager *AM) {
  return std::make_unique<DbPass>(AM);
}
} // namespace arts
} // namespace mlir
