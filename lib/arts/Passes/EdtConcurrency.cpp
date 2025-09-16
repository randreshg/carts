///==========================================================================
/// File: EdtConcurrency.cpp
/// Defines concurrency-driven optimizations using EDT graph analysis.
///==========================================================================

#include "ArtsPassDetails.h"
#include "arts/Analysis/Db/DbPlacement.h"
#include "arts/Analysis/Edt/EdtAnalysis.h"
#include "arts/Analysis/Edt/EdtPlacement.h"
#include "arts/Analysis/Graphs/Db/DbGraph.h"
#include "arts/Analysis/Graphs/Edt/EdtConcurrencyGraph.h"
#include "arts/Analysis/Graphs/Edt/EdtGraph.h"
#include "arts/ArtsDialect.h"
#include "arts/Passes/ArtsPasses.h"
#include "arts/Utils/ArtsDebug.h"
#include <optional>
#include <tuple>

using namespace mlir;
using namespace mlir::arts;

ARTS_DEBUG_SETUP(edt)

namespace {

/// Disjoint Set Union for grouping EDTs
struct DisjointSetUnion {
  llvm::DenseMap<EdtOp, EdtOp> parent;

  EdtOp find(EdtOp x) {
    auto it = parent.find(x);
    if (it == parent.end() || it->second == x)
      return parent[x] = x;
    return it->second = find(it->second);
  }

  void unite(EdtOp a, EdtOp b) {
    a = find(a);
    b = find(b);
    if (a != b)
      parent[b] = a;
  }
};

/// Helper functions for EdtConcurrencyPass
class EdtConcurrencyOptimizer {
private:
  ModuleOp module;
  EdtGraph *edtGraph;
  EdtAnalysis *edtAnalysis;
  EdtConcurrencyGraph *concurrencyGraph;

public:
  EdtConcurrencyOptimizer(ModuleOp module, EdtGraph *edtGraph,
                          EdtAnalysis *edtAnalysis, EdtConcurrencyGraph *cg)
      : module(module), edtGraph(edtGraph), edtAnalysis(edtAnalysis),
        concurrencyGraph(cg) {}

  /// Remove unnecessary barriers between low-risk concurrent EDTs
  void pruneBarriers(func::FuncOp func) {
    SmallVector<BarrierOp, 8> toErase;

    func.walk([&](BarrierOp barrier) {
      if (canRemoveBarrier(barrier)) {
        toErase.push_back(barrier);
      }
    });

    for (auto barrier : toErase) {
      barrier.erase();
    }
  }

  /// Group EDTs with high locality and no conflicts
  void createPlacementGroups(func::FuncOp func) {
    DisjointSetUnion dsu;

    /// Initialize all EDTs as separate groups
    for (EdtOp edt : concurrencyGraph->getEdts())
      dsu.parent[edt] = edt;

    /// Unite EDTs with high locality and no conflicts
    for (const auto &edge : concurrencyGraph->getEdges()) {
      if (!edge.mayConflict && edge.localityScore >= 0.6)
        dsu.unite(edge.from, edge.to);
    }

    /// Assign group attributes
    assignGroupAttributes(dsu);
  }

  /// Insert barriers between high-risk concurrent EDTs
  void insertConflictBarriers() {
    for (const auto &edge : concurrencyGraph->getEdges()) {
      if (shouldInsertBarrier(edge))
        insertBarrierBetween(edge.from, edge.to);
    }
  }

  /// Apply EDT and DB placement decisions
  void applyPlacementDecisions(func::FuncOp func, DbGraph *dbGraph) {
    auto nodes = EdtPlacementHeuristics::makeNodeNames(4);

    /// EDT placement
    EdtPlacementHeuristics edtPlacer(edtGraph, edtAnalysis);
    auto edtDecisions = edtPlacer.compute(nodes);
    applyEdtPlacement(edtDecisions, nodes);

    /// DB placement
    DbPlacementHeuristics dbPlacer(dbGraph);
    auto dbDecisions = dbPlacer.compute(func, nodes);
    applyDbPlacement(dbDecisions, nodes);

    /// Debug output
    emitDebugOutput(func, edtPlacer, edtDecisions, dbPlacer, dbDecisions);
  }

private:
  bool canRemoveBarrier(BarrierOp barrier) {
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
    for (EdtOp a : before) {
      for (EdtOp b : after) {
        auto affinity = edtAnalysis->affinity(a, b);
        if (affinity.mayConflict || affinity.concurrencyRisk > 0.0) {
          return false; /// Risky pair found
        }
      }
    }

    return true;
  }

  void assignGroupAttributes(const DisjointSetUnion &dsu) {
    llvm::DenseMap<EdtOp, unsigned> repToGroup;
    unsigned nextGroup = 1;

    for (EdtOp edt : concurrencyGraph->getEdts()) {
      EdtOp representative = const_cast<DisjointSetUnion &>(dsu).find(edt);
      unsigned group =
          repToGroup.try_emplace(representative, nextGroup).first->second;

      if (group == nextGroup && representative == edt) {
        nextGroup++;
      }

      edt->setAttr("arts.placement_group",
                   StringAttr::get(module.getContext(),
                                   ("G" + std::to_string(group)).c_str()));
    }
  }

  bool shouldInsertBarrier(const EdtConcurrencyEdge &edge) {
    /// Skip if EDTs are already ordered
    if (edtGraph->isEdtReachable(edge.from, edge.to) ||
        edtGraph->isEdtReachable(edge.to, edge.from)) {
      return false;
    }

    /// Insert barrier for high-risk concurrent pairs
    return edge.mayConflict && edge.concurrencyRisk >= 0.8;
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
    ARTS_DEBUG_SECTION("edt-concurrency-pass", {
      std::string s;
      llvm::raw_string_ostream os(s);
      concurrencyGraph->print(os);
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

struct EdtConcurrencyPass
    : public arts::EdtConcurrencyBase<EdtConcurrencyPass> {
  void runOnOperation() override {
    ModuleOp module = getOperation();

    module.walk([&](func::FuncOp func) {
      /// Build analysis graphs
      auto graphs = buildAnalysisGraphs(func, module);
      if (!graphs.has_value()) {
        return; /// Skip if no EDTs found
      }

      auto [dbGraph, edtGraph, edtAnalysis, concurrencyGraph] =
          std::move(graphs.value());

      /// Create optimizer and apply transformations
      EdtConcurrencyOptimizer optimizer(
          module, edtGraph.get(), edtAnalysis.get(), concurrencyGraph.get());

      optimizer.pruneBarriers(func);
      optimizer.createPlacementGroups(func);
      optimizer.insertConflictBarriers();
      optimizer.applyPlacementDecisions(func, dbGraph.get());
    });
  }

private:
  std::optional<std::tuple<std::unique_ptr<DbGraph>, std::unique_ptr<EdtGraph>,
                           std::unique_ptr<EdtAnalysis>,
                           std::unique_ptr<EdtConcurrencyGraph>>>
  buildAnalysisGraphs(func::FuncOp func, ModuleOp module) {
    /// Build DB graph
    auto dbGraph = std::make_unique<DbGraph>(func, nullptr);
    dbGraph->build();

    /// Build EDT graph
    auto edtGraph = std::make_unique<EdtGraph>(func, dbGraph.get());
    edtGraph->build();

    if (edtGraph->getNumTasks() == 0) {
      return std::nullopt; /// No EDTs to process
    }

    /// Build EDT analysis
    auto edtAnalysis =
        std::make_unique<EdtAnalysis>(module, dbGraph.get(), edtGraph.get());
    edtAnalysis->analyze();

    /// Build concurrency graph
    auto concurrencyGraph = std::make_unique<EdtConcurrencyGraph>(
        edtGraph.get(), edtAnalysis.get());
    concurrencyGraph->build();

    return std::make_tuple(std::move(dbGraph), std::move(edtGraph),
                           std::move(edtAnalysis), std::move(concurrencyGraph));
  }
};
} // namespace

std::unique_ptr<Pass> mlir::arts::createEdtConcurrencyPass() {
  return std::make_unique<EdtConcurrencyPass>();
}