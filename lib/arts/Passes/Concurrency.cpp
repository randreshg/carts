///==========================================================================
/// File: Concurrency.cpp
/// Defines concurrency-driven optimizations using ARTS analysis managers.
///==========================================================================

#include "ArtsPassDetails.h"
#include "arts/Analysis/ArtsAnalysisManager.h"
#include "arts/Analysis/Concurrency/DbPlacement.h"
#include "arts/Analysis/Concurrency/EdtPlacement.h"
#include "arts/Analysis/Graphs/Concurrency/ConcurrencyGraph.h"
#include "arts/ArtsDialect.h"
#include "arts/Passes/ArtsPasses.h"
#include "arts/Utils/ArtsAbstractMachine.h"
#include "arts/Utils/ArtsDebug.h"

using namespace mlir;
using namespace mlir::arts;

ARTS_DEBUG_SETUP(edt_concurrency)

namespace {

/// Helper functions for ConcurrencyPass using analysis managers
class EdtConcurrencyOptimizer {
private:
  ModuleOp module;
  ArtsAnalysisManager *AM;

public:
  EdtConcurrencyOptimizer(ModuleOp module, ArtsAnalysisManager *AM)
      : module(module), AM(AM) {
    assert(AM && "AnalysisManager cannot be null");
  }

  /// Remove unnecessary barriers between low-risk concurrent EDTs
  void pruneBarriers(func::FuncOp func) {
    SmallVector<BarrierOp, 8> toErase;
    auto &edtGraph = AM->getEdtGraph(func);
    func.walk([&](BarrierOp barrier) {
      if (canRemoveBarrier(barrier, &edtGraph))
        toErase.push_back(barrier);
    });

    for (auto barrier : toErase)
      barrier.erase();
  }

  /// Group EDTs with high locality and no conflicts
  void createPlacementGroups(func::FuncOp func) {
    auto &concurrencyGraph = AM->getConcurrencyGraph(func);
    // concurrencyGraph.createPlacementGroups(module);
  }

  /// Insert barriers between high-risk concurrent EDTs
  void insertConflictBarriers() {
    // Implementation would iterate through concurrency edges
    // and insert barriers where needed
    // auto *concurrencyGraph = GM->getConcurrencyGraph();
    // auto *edtGraph = GM->getEdtGraph();
    // for (const auto &edge : concurrencyGraph->getEdges()) {
    //   if (shouldInsertBarrier(edge, edtGraph))
    //     insertBarrierBetween(*edge.from, *edge.to);
    // }
  }

  /// Apply EDT and DB placement decisions
  void applyPlacementDecisions(func::FuncOp func) {
    auto nodes = EdtPlacementHeuristics::makeNodeNames(4);
    auto &dbGraph = AM->getDbGraph(func);
    auto &edtGraph = AM->getEdtGraph(func);
    auto &edtAnalysis = AM->getEdtAnalysis();

    /// EDT placement
    EdtPlacementHeuristics edtPlacer(&edtGraph, &edtAnalysis);
    auto edtDecisions = edtPlacer.compute(nodes);
    applyEdtPlacement(edtDecisions, nodes);

    /// DB placement
    DbPlacementHeuristics dbPlacer(&dbGraph);
    auto dbDecisions = dbPlacer.compute(func, nodes);
    applyDbPlacement(dbDecisions, nodes);

    /// Debug output
    emitDebugOutput(func, edtPlacer, edtDecisions, dbPlacer, dbDecisions);
  }

private:
  bool canRemoveBarrier(BarrierOp barrier, EdtGraph *edtGraph) {
    Block *block = barrier->getBlock();
    SmallVector<EdtOp, 8> before, after;

    /// Collect EDTs before and after the barrier
    bool pastBarrier = false;
    for (Operation &op : *block) {
      if (&op == barrier.getOperation()) {
        pastBarrier = true;
        continue;
      }
      if (auto edt = dyn_cast<EdtOp>(&op)) {
        (pastBarrier ? after : before).push_back(edt);
      }
    }

    if (before.empty() || after.empty()) {
      return false;
    }

    /// Check if all pairs are concurrent
    for (EdtOp a : before) {
      for (EdtOp b : after) {
        if (edtGraph->isEdtReachable(a, b) || edtGraph->isEdtReachable(b, a)) {
          return false; /// Not all pairs are concurrent
        }
      }
    }

    /// Check if all pairs are low-risk
    auto &edtAnalysis = AM->getEdtAnalysis();
    for (EdtOp a : before) {
      for (EdtOp b : after) {
        auto affinity = edtAnalysis.affinity(a, b);
        if (affinity.mayConflict || affinity.concurrencyRisk > 0.0) {
          return false; /// Risky pair found
        }
      }
    }

    return true;
  }

  bool shouldInsertBarrier(const void *edge, EdtGraph *edtGraph) {
    /// Skip if EDTs are already ordered
    // if (edtGraph->isEdtReachable(*edge.from, *edge.to) ||
    //     edtGraph->isEdtReachable(*edge.to, *edge.from)) {
    //   return false;
    // }

    /// Insert barrier for high-risk concurrent pairs
    // return edge.mayConflict && edge.concurrencyRisk >= 0.8;
    return false;
  }

  void insertBarrierBetween(EdtOp a, EdtOp b) {
    if (a->getBlock() != b->getBlock())
      return;

    Block *block = a->getBlock();

    /// Determine order of operations
    bool aBeforeB = true;
    for (Operation &op : *block) {
      if (&op == a) {
        aBeforeB = true;
        break;
      }
      if (&op == b) {
        aBeforeB = false;
        break;
      }
    }

    EdtOp first = aBeforeB ? a : b;
    EdtOp second = aBeforeB ? b : a;

    /// Check if barrier already exists 2between them
    for (Operation *it = first->getNextNode(); it && it != second;
         it = it->getNextNode()) {
      if (isa<BarrierOp>(it)) {
        return; /// Barrier already exists
      }
    }

    /// Insert new barrier
    OpBuilder builder(second);
    builder.create<BarrierOp>(second->getLoc());
  }

  void applyEdtPlacement(const SmallVector<PlacementDecision, 16> &decisions,
                         const SmallVector<std::string, 8> &nodes) {
    llvm::StringMap<unsigned> nodeIndex;
    for (unsigned i = 0; i < nodes.size(); ++i) {
      nodeIndex[nodes[i]] = i;
    }

    OpBuilder builder(module.getContext());
    for (const auto &decision : decisions) {
      auto it = nodeIndex.find(decision.chosenNode);
      unsigned idx = (it != nodeIndex.end()) ? it->second : 0u;
      decision.edtOp->setAttr(
          "node", IntegerAttr::get(builder.getI32Type(), (int32_t)idx));
    }
  }

  void applyDbPlacement(const SmallVector<DbPlacementDecision, 16> &decisions,
                        const SmallVector<std::string, 8> &nodes) {
    llvm::StringMap<unsigned> nodeIndex;
    for (unsigned i = 0; i < nodes.size(); ++i) {
      nodeIndex[nodes[i]] = i;
    }

    OpBuilder builder(module.getContext());
    for (const auto &decision : decisions) {
      auto it = nodeIndex.find(decision.chosenNode);
      unsigned idx = (it != nodeIndex.end()) ? it->second : 0u;
      if (Operation *op = decision.dbAllocOp) {
        op->setAttr("node",
                    IntegerAttr::get(builder.getI32Type(), (int32_t)idx));
      }
    }
  }

  void
  emitDebugOutput(func::FuncOp func, const EdtPlacementHeuristics &edtPlacer,
                  const SmallVector<PlacementDecision, 16> &edtDecisions,
                  const DbPlacementHeuristics &dbPlacer,
                  const SmallVector<DbPlacementDecision, 16> &dbDecisions) {
    auto &concurrencyGraph = AM->getConcurrencyGraph(func);

    ARTS_DEBUG_SECTION("edt-concurrency-pass", {
      std::string s;
      llvm::raw_string_ostream os(s);
      concurrencyGraph.print(os);
      ARTS_DBGS() << os.str();
    });

    ARTS_DEBUG_SECTION("edt-placement-json", {
      std::string js;
      llvm::raw_string_ostream jos(js);
      edtPlacer.exportToJson(func, edtDecisions, jos);
      ARTS_DBGS() << js;
    });

    ARTS_DEBUG_SECTION("db-placement-json", {
      std::string js;
      llvm::raw_string_ostream jos(js);
      dbPlacer.exportToJson(func, dbDecisions, jos);
      ARTS_DBGS() << js;
    });
  }
};

} // namespace

struct ConcurrencyPass : public ConcurrencyBase<ConcurrencyPass> {
  ConcurrencyPass() = default;
  explicit ConcurrencyPass(ArtsAnalysisManager *AM) : AM(AM) {}
  void runOnOperation() override;

private:
  /// Lower a parallel EDT. If it contains an arts.for, create chunked DbAcquire
  /// deps per worker and outline the loop body as a task EDT per worker.
  void lowerParallelEdt(EdtOp op, const arts::ArtsAbstractMachine &machine);

  /// Deprecated helper removed: we now require an external manager
  ArtsAnalysisManager *AM = nullptr; ///< non-owning
};

void ConcurrencyPass::runOnOperation() {
  ModuleOp module = getOperation();

  module.walk([&](func::FuncOp func) {
    arts::ArtsAbstractMachine machine;

    SmallVector<EdtOp, 8> parallelEdts;
    func.walk([&](EdtOp edt) {
      if (edt.getType() == EdtType::parallel)
        parallelEdts.push_back(edt);
    });
    for (EdtOp edt : parallelEdts)
      lowerParallelEdt(edt, machine);

    assert(AM && "ArtsAnalysisManager must be provided externally");
    EdtConcurrencyOptimizer optimizer(module, AM);

    optimizer.pruneBarriers(func);
    optimizer.createPlacementGroups(func);
    optimizer.insertConflictBarriers();
    optimizer.applyPlacementDecisions(func);
  });
}

void ConcurrencyPass::lowerParallelEdt(
    EdtOp op, const arts::ArtsAbstractMachine &machine) {
  OpBuilder builder(op);
  Location loc = op.getLoc();

  ForOp containedForOp = nullptr;
  op.walk([&](ForOp forOp) {
    if (!containedForOp)
      containedForOp = forOp;
    return WalkResult::advance();
  });

  auto epochOp = builder.create<EpochOp>(loc);
  auto &region = epochOp.getRegion();
  if (region.empty())
    region.push_back(new Block());
  Block *newBlock = &region.front();
  builder.setInsertionPointToEnd(newBlock);

  Value lb = builder.create<arith::ConstantIndexOp>(loc, 0);
  Value ub;
  if (machine.hasValidThreads() && machine.hasValidNodeCount()) {
    long long total = machine.getThreads() * machine.getNodeCount();
    ub = builder.create<arith::ConstantIndexOp>(loc, (int64_t)total);
  } else if (machine.hasValidThreads()) {
    ub = builder.create<arith::ConstantIndexOp>(loc, machine.getThreads());
  } else {
    Value workers = builder.create<GetTotalWorkersOp>(loc);
    ub = builder.create<arith::IndexCastOp>(loc, builder.getIndexType(),
                                            workers);
  }
  Value one = builder.create<arith::ConstantIndexOp>(loc, 1);
  auto scfForOp = builder.create<scf::ForOp>(loc, lb, ub, one);

  SmallVector<Value> deps;
  if (containedForOp) {
    Value flb = containedForOp.getLowerBound()[0];
    Value fub = containedForOp.getUpperBound()[0];
    Value fst = containedForOp.getStep()[0];
    Value wid = scfForOp.getInductionVar();
    Value widIdx =
        builder.create<arith::IndexCastOp>(loc, builder.getIndexType(), wid);
    Value start = builder.create<arith::AddIOp>(
        loc, flb, builder.create<arith::MulIOp>(loc, widIdx, fst));
    Value totalIters = builder.create<arith::CeilDivSIOp>(
        loc, builder.create<arith::SubIOp>(loc, fub, flb), fst);
    Value base = builder.create<arith::DivSIOp>(loc, totalIters, ub);
    Value extra = builder.create<arith::RemSIOp>(loc, totalIters, ub);
    Value sizeIters = builder.create<arith::SelectOp>(
        loc,
        builder.create<arith::CmpIOp>(loc, arith::CmpIPredicate::slt, widIdx,
                                      extra),
        builder.create<arith::AddIOp>(loc, base, one), base);
    Value elemCount = builder.create<arith::MulIOp>(loc, sizeIters, fst);

    for (Value dep : op.getDependencies()) {
      auto acq = dep.getDefiningOp<DbAcquireOp>();
      if (!acq)
        continue;
      SmallVector<Value> indices(acq.getIndices().begin(),
                                 acq.getIndices().end());
      SmallVector<Value> offsets(acq.getOffsets().begin(),
                                 acq.getOffsets().end());
      SmallVector<Value> sizes(acq.getSizes().begin(), acq.getSizes().end());
      indices.push_back(start);
      offsets.push_back(start);
      sizes.push_back(elemCount);
      auto chunkAcq = builder.create<DbAcquireOp>(
          loc, acq.getMode(), acq.getSourceGuid(), acq.getSourcePtr(), indices,
          offsets, sizes);
      deps.push_back(chunkAcq.getPtr());
    }
  } else {
    for (Value dep : op.getDependencies())
      deps.push_back(dep);
  }

  builder.setInsertionPointToStart(scfForOp.getBody());
  auto newEdt = builder.create<EdtOp>(loc, EdtType::task, deps);
  Region &dst = newEdt.getRegion();
  if (dst.empty())
    dst.push_back(new Block());
  Block &dstBody = dst.front();
  dstBody.clear();

  if (containedForOp) {
    containedForOp->moveBefore(&dstBody, dstBody.end());
  } else {
    Region &src = op.getRegion();
    SmallVector<Operation *, 16> toMove;
    for (auto &block : src)
      for (auto &o : block)
        toMove.push_back(&o);
    for (auto *o : toMove)
      o->moveBefore(&dstBody, dstBody.end());
  }

  builder.setInsertionPointToEnd(newBlock);
  builder.create<YieldOp>(loc);
  op.erase();
}

// getAnalysisManager removed; external manager is required

namespace mlir {
namespace arts {

std::unique_ptr<Pass> createConcurrencyPass() {
  llvm_unreachable(
      "createConcurrencyPass() without ArtsAnalysisManager is not supported. "
      "Use createConcurrencyPass(ArtsAnalysisManager*) instead.");
}

std::unique_ptr<Pass> createConcurrencyPass(ArtsAnalysisManager *AM) {
  return std::make_unique<ConcurrencyPass>(AM);
}

} // namespace arts
} // namespace mlir
