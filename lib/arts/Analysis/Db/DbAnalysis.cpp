///==========================================================================
/// File: DbAnalysis.cpp
/// Implementation of the DbAnalysis class for DB operation analysis.
///==========================================================================

#include "arts/Analysis/Db/DbAnalysis.h"
#include "arts/Analysis/Db/DbAliasAnalysis.h"
#include "arts/Analysis/Db/DbDataFlowAnalysis.h"
#include "arts/Analysis/Graphs/Db/DbGraph.h"
#include "arts/Analysis/Graphs/Db/DbNode.h"
#include "arts/Analysis/LoopAnalysis.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "llvm/Support/Debug.h"

#include "arts/Utils/ArtsDebug.h"
ARTS_DEBUG_SETUP(db_analysis);

using namespace mlir;
using namespace mlir::arts;

DbAnalysis::DbAnalysis(Operation *module) : module(module), solver() {
  ARTS_INFO("Initializing DbAnalysis for module");
  loopAnalysis = std::make_unique<LoopAnalysis>(module);
  dbAliasAnalysis = std::make_unique<DbAliasAnalysis>(this);
  dbDataFlowAnalysis = std::make_unique<DbDataFlowAnalysis>(solver);
  (void)solver.load<DbDataFlowAnalysis>();
}

DbAnalysis::~DbAnalysis() { ARTS_INFO("Destroying DbAnalysis"); }

DbGraph *DbAnalysis::getOrCreateGraph(func::FuncOp func) {
  ARTS_INFO("Getting or creating DbGraph for function: " << func.getName());
  auto it = functionGraphMap.find(func);
  if (it != functionGraphMap.end()) {
    ARTS_INFO("Found existing DbGraph.");
    return it->second.get();
  }

  ARTS_INFO("Creating new DbGraph for function: " << func.getName());
  auto newGraph = std::make_unique<DbGraph>(func, this);

  /// Initialize the dataflow analysis with the graph and analysis context
  dbDataFlowAnalysis->initialize(newGraph.get(), this);

  /// Build nodes and dependencies
  newGraph->build();

  /// Store the graph
  DbGraph *graphPtr = newGraph.get();
  functionGraphMap[func] = std::move(newGraph);
  return graphPtr;
}

DbGraph *DbAnalysis::getOrCreateGraphNodesOnly(func::FuncOp func) {
  ARTS_INFO("Getting or creating DbGraph (nodes only) for function: "
            << func.getName());
  auto it = functionGraphMap.find(func);
  if (it != functionGraphMap.end())
    return it->second.get();
  auto newGraph = std::make_unique<DbGraph>(func, this);

  /// Build nodes only
  newGraph->buildNodesOnly();

  /// Store the graph
  DbGraph *graphPtr = newGraph.get();
  functionGraphMap[func] = std::move(newGraph);
  return graphPtr;
}

DbAnalysis::OverlapKind DbAnalysis::estimateOverlap(arts::DbAcquireOp a,
                                                    arts::DbAcquireOp b) {
  SmallVector<int64_t, 4> saOffs, saSizes, sbOffs, sbSizes;
  {
    const DbAcquireInfo *ai = nullptr;
    const DbAcquireInfo *bi = nullptr;
    for (auto &[func, graph] : functionGraphMap) {
      if (!ai) {
        if (auto *na = dyn_cast_or_null<DbAcquireNode>(
                graph->getNode(a.getOperation())))
          ai = &na->getInfo();
      }
      if (!bi) {
        if (auto *nb = dyn_cast_or_null<DbAcquireNode>(
                graph->getNode(b.getOperation())))
          bi = &nb->getInfo();
      }
      if (ai && bi)
        break;
    }
    if (ai) {
      saOffs = ai->constOffsets;
      saSizes = ai->constSizes;
    } else {
      /// Fallback: derive constants directly from ops
      for (Value v : a.getOffsets())
        saOffs.push_back(
            v.getDefiningOp<mlir::arith::ConstantIndexOp>()
                ? v.getDefiningOp<mlir::arith::ConstantIndexOp>().value()
                : INT64_MIN);
      for (Value v : a.getSizes())
        saSizes.push_back(
            v.getDefiningOp<mlir::arith::ConstantIndexOp>()
                ? v.getDefiningOp<mlir::arith::ConstantIndexOp>().value()
                : INT64_MIN);
    }
    if (bi) {
      sbOffs = bi->constOffsets;
      sbSizes = bi->constSizes;
    } else {
      for (Value v : b.getOffsets())
        sbOffs.push_back(
            v.getDefiningOp<mlir::arith::ConstantIndexOp>()
                ? v.getDefiningOp<mlir::arith::ConstantIndexOp>().value()
                : INT64_MIN);
      for (Value v : b.getSizes())
        sbSizes.push_back(
            v.getDefiningOp<mlir::arith::ConstantIndexOp>()
                ? v.getDefiningOp<mlir::arith::ConstantIndexOp>().value()
                : INT64_MIN);
    }
  }
  size_t r = std::min(saSizes.size(), sbSizes.size());
  bool anyUnknown = false;
  bool disjoint = false;
  bool equal = (saSizes == sbSizes) && (saOffs == sbOffs);
  for (size_t i = 0; i < r; ++i) {
    int64_t ao = saOffs[i], asz = saSizes[i];
    int64_t bo = sbOffs[i], bsz = sbSizes[i];
    if (ao == INT64_MIN || asz == INT64_MIN || bo == INT64_MIN ||
        bsz == INT64_MIN) {
      anyUnknown = true;
      continue;
    }
    int64_t a0 = ao, a1 = ao + asz;
    int64_t b0 = bo, b1 = bo + bsz;
    bool overlap = !(a1 <= b0 || b1 <= a0);
    if (!overlap) {
      disjoint = true;
      break;
    }
  }
  if (disjoint)
    return OverlapKind::Disjoint;
  if (equal)
    return OverlapKind::Full;
  if (anyUnknown)
    return OverlapKind::Unknown;
  return OverlapKind::Partial;
}

bool DbAnalysis::invalidateGraph(func::FuncOp func) {
  ARTS_INFO("Invalidating DbGraph for function: " << func.getName());
  auto it = functionGraphMap.find(func);
  if (it != functionGraphMap.end()) {
    functionGraphMap.erase(it);
    ARTS_INFO("DbGraph invalidated successfully.");
    return true;
  }
  ARTS_WARN("DbGraph not found, could not invalidate.");
  return false;
}

void DbAnalysis::print(func::FuncOp func) {
  ARTS_INFO("Printing DbGraph for function: " << func.getName());
  DbGraph *graph = getOrCreateGraph(func);
  if (graph) {
    graph->print(ARTS_DBGS());
  } else {
    ARTS_ERROR("Could not get or create DbGraph for printing function "
               << func.getName());
  }
}
