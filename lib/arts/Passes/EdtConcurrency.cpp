//===----------------------------------------------------------------------===//
// EdtConcurrency.cpp - Concurrency-driven optimizations using EDT graph
//===----------------------------------------------------------------------===//

#include "ArtsPassDetails.h"
#include "arts/Analysis/Graphs/Edt/EdtGraph.h"
#include "arts/Analysis/Graphs/Db/DbGraph.h"
#include "arts/Analysis/Graphs/Edt/EdtConcurrencyGraph.h"
#include "arts/Analysis/Edt/EdtAnalysis.h"
#include "arts/Analysis/Edt/EdtPlacement.h"
#include "arts/Analysis/Db/DbPlacement.h"
#include "arts/Passes/ArtsPasses.h"
#include "arts/Utils/ArtsDebug.h"

using namespace mlir;
using namespace mlir::arts;

ARTS_DEBUG_SETUP(edt)

namespace {
struct EdtConcurrencyPass : public arts::EdtConcurrencyBase<EdtConcurrencyPass> {
  void runOnOperation() override {
    ModuleOp module = getOperation();

    module.walk([&](func::FuncOp func) {
      std::unique_ptr<DbGraph> dbGraph = std::make_unique<DbGraph>(func, nullptr);
      dbGraph->build();
      if (!dbGraph)
        return;
      std::unique_ptr<EdtGraph> edtGraph = std::make_unique<EdtGraph>(func, dbGraph.get());
      edtGraph->build();
      if (edtGraph->getNumTasks() == 0)
        return;

      EdtAnalysis edtAnalysis(module);
      edtAnalysis.analyze();
      EdtConcurrencyGraph cg(edtGraph.get(), &edtAnalysis);
      cg.build();

      // 1) Barrier pruning: remove barriers if ALL pre/post EDT pairs are concurrent
      // and low-risk (no write hazards) per affinity metrics
      SmallVector<BarrierOp, 8> toErase;
      func.walk([&](BarrierOp barrier) {
        Block *block = barrier->getBlock();
        SmallVector<EdtOp, 8> before, after;
        bool past = false;
        for (Operation &op : *block) {
          if (&op == barrier.getOperation()) { past = true; continue; }
          if (auto edt = dyn_cast<EdtOp>(&op)) (past ? after : before).push_back(edt);
        }
        if (before.empty() || after.empty()) return;

        // Require all pairs be concurrent
        for (EdtOp a : before) for (EdtOp b : after) {
          if (edtGraph->isTaskReachable(a, b) || edtGraph->isTaskReachable(b, a))
            return; // not all pairs concurrent
        }

        // All pairs concurrent: ensure low risk
        for (EdtOp a : before) for (EdtOp b : after) {
          auto aff = edtAnalysis.affinity(a.getOperation(), b.getOperation());
          if (aff.mayConflict || aff.concurrencyRisk > 0.0) return; // risky
        }
        toErase.push_back(barrier);
      });
      for (auto b : toErase) b.erase();

      // 2) Co-location hints: group tasks with high locality and no conflicts
      struct DSU { llvm::DenseMap<Operation*, Operation*> p; Operation* f(Operation* x){
        auto it=p.find(x); if(it==p.end()||it->second==x) return p[x]=x; return it->second=f(it->second);} void u(Operation*a,Operation*b){a=f(a);b=f(b); if(a!=b)p[b]=a;} } dsu;
      for (Operation *t : cg.getTasks()) dsu.p[t]=t;
      for (const auto &e : cg.getEdges()) {
        if (!e.mayConflict && e.localityScore >= 0.6) dsu.u(e.a, e.b);
      }
      llvm::DenseMap<Operation*, unsigned> repToGroup;
      unsigned nextG=1;
      for (Operation *t : cg.getTasks()) {
        Operation *r = dsu.f(t);
        unsigned g = repToGroup.try_emplace(r, nextG).first->second;
        if (g == nextG && r == t) nextG++;
        t->setAttr("arts.placement_group", StringAttr::get(module.getContext(), ("G" + std::to_string(g)).c_str()));
      }

      // 3) Conflict fencing: insert a barrier between high-risk concurrent pairs in same block
      auto insertBarrierBetween = [&](Operation *a, Operation *b) {
        if (a->getBlock() != b->getBlock()) return;
        Block *blk = a->getBlock();
        // Determine order
        bool aBeforeB = true;
        for (Operation &op : *blk) {
          if (&op == a) { aBeforeB = true; break; }
          if (&op == b) { aBeforeB = false; break; }
        }
        Operation *first = aBeforeB ? a : b;
        Operation *second = aBeforeB ? b : a;
        // Check if a barrier already exists between them
        for (Operation *it = first->getNextNode(); it && it != second; it = it->getNextNode()) {
          if (isa<BarrierOp>(it)) return;
        }
        OpBuilder builder(second);
        builder.create<BarrierOp>(second->getLoc());
      };

      for (const auto &e : cg.getEdges()) {
        if (edtGraph->isTaskReachable(static_cast<EdtOp>(e.a), static_cast<EdtOp>(e.b)) ||
            edtGraph->isTaskReachable(static_cast<EdtOp>(e.b), static_cast<EdtOp>(e.a)))
          continue; // already ordered
        if (e.mayConflict && e.concurrencyRisk >= 0.8) insertBarrierBetween(e.a, e.b);
      }

      ARTS_DEBUG_SECTION("edt-concurrency-pass", {
        std::string s;
        llvm::raw_string_ostream os(s);
        cg.print(os);
        ARTS_DBGS() << os.str();
      });

      // Compute EDT placement decisions with a default 4-node fabric
      EdtPlacementHeuristics placer(edtGraph.get(), &edtAnalysis);
      auto nodes = EdtPlacementHeuristics::makeNodeNames(4);
      auto decisions = placer.compute(nodes);

      // Apply placement as IR attributes: set i32 attr "node" on each arts.edt
      // Also emit debug JSON
      {
        llvm::StringMap<unsigned> nodeIndex;
        for (unsigned i = 0; i < nodes.size(); ++i) nodeIndex[nodes[i]] = i;
        for (const auto &d : decisions) {
          auto it = nodeIndex.find(d.chosenNode);
          unsigned idx = (it != nodeIndex.end()) ? it->second : 0u;
          OpBuilder builder(module.getContext());
          d.edtOp->setAttr("node", IntegerAttr::get(builder.getI32Type(), (int32_t)idx));
        }
        ARTS_DEBUG_SECTION("edt-placement-json", {
          std::string js;
          llvm::raw_string_ostream jos(js);
          placer.exportToJson(func, decisions, jos);
          ARTS_DBGS() << js;
        });
      }

      // Compute and apply DbAlloc placement based on EDT node hints
      {
        DbPlacementHeuristics dbPlacer(dbGraph.get());
        auto dbDecisions = dbPlacer.compute(func, nodes);
        OpBuilder builder(module.getContext());
        llvm::StringMap<unsigned> nodeIndex;
        for (unsigned i = 0; i < nodes.size(); ++i) nodeIndex[nodes[i]] = i;
        for (const auto &d : dbDecisions) {
          auto it = nodeIndex.find(d.chosenNode);
          unsigned idx = (it != nodeIndex.end()) ? it->second : 0u;
          if (Operation *op = d.dbAllocOp) {
            op->setAttr("node", IntegerAttr::get(builder.getI32Type(), (int32_t)idx));
          }
        }
        ARTS_DEBUG_SECTION("db-placement-json", {
          std::string js;
          llvm::raw_string_ostream jos(js);
          dbPlacer.exportToJson(func, dbDecisions, jos);
          ARTS_DBGS() << js;
        });
      }
    });
  }
};
} // namespace

std::unique_ptr<Pass> mlir::arts::createEdtConcurrencyPass() {
  return std::make_unique<EdtConcurrencyPass>();
}


