///==========================================================================
/// File: EdtConcurrencyGraph.cpp
///
/// This file implements the concurrency graph for EDT analysis.
///==========================================================================

#include "arts/Analysis/Graphs/Edt/EdtConcurrencyGraph.h"
#include "arts/Utils/ArtsUtils.h"
#include "llvm/Support/JSON.h"

using namespace mlir;
using namespace mlir::arts;

void EdtConcurrencyGraph::clear() {
  edts.clear();
  edges.clear();
}

void EdtConcurrencyGraph::build() {
  clear();
  if (!edtGraph || !edtGraph->hasDbGraph())
    return;

  /// Collect all edts in a stable order
  edtGraph->forEachNode([&](NodeBase *node) {
    if (auto *t = dyn_cast<EdtNode>(node))
      edts.push_back(t->getEdtOp());
  });

  /// Build undirected edges A-B where neither is reachable from the other
  for (size_t i = 0; i < edts.size(); ++i) {
    for (size_t j = i + 1; j < edts.size(); ++j) {
      EdtOp from = edts[i];
      EdtOp to = edts[j];
      bool depAB = edtGraph->isEdtReachable(from, to);
      bool depBA = edtGraph->isEdtReachable(to, from);
      if (!depAB && !depBA) {
        EdtConcurrencyEdge e{};
        e.from = from;
        e.to = to;
        if (analysis) {
          auto aff = analysis->affinity(from, to);
          e.dataOverlap = aff.dataOverlap;
          e.hazardScore = aff.hazardScore;
          e.mayConflict = aff.mayConflict;
          e.reuseProximity = aff.reuseProximity;
          e.localityScore = aff.localityScore;
          e.concurrencyRisk = aff.concurrencyRisk;
        }
        edges.push_back(e);
      }
    }
  }
}

void EdtConcurrencyGraph::print(llvm::raw_ostream &os) const {
  os << "EdtConcurrencyGraph: edts=" << edts.size()
     << ", edges=" << edges.size() << "\n";
  for (auto &e : edges) {
    os << "  (" << e.from << ", " << e.to << ")"
       << " overlap=" << e.dataOverlap << " hazard=" << e.hazardScore
       << " reuse=" << e.reuseProximity << " local=" << e.localityScore
       << " risk=" << e.concurrencyRisk
       << " conflict=" << (e.mayConflict ? 1 : 0) << "\n";
  }
}

void EdtConcurrencyGraph::exportToJson(llvm::raw_ostream &os,
                                       bool includeAnalysis) const {
  using namespace llvm::json;

  Object root;

  Array tasksArr;
  for (auto t : edts) {
    auto *n = edtGraph->getNode(t);
    tasksArr.push_back(
        Object{{"id", n ? sanitizeString(n->getHierId()) : std::string("")}});
  }
  root["edts"] = std::move(tasksArr);

  Array edgesArr;
  for (auto &e : edges) {
    auto *na = edtGraph->getNode(e.from);
    auto *nb = edtGraph->getNode(e.to);
    edgesArr.push_back(
        Object{{"from", na ? sanitizeString(na->getHierId()) : std::string("")},
               {"to", nb ? sanitizeString(nb->getHierId()) : std::string("")},
               {"ovl", e.dataOverlap},
               {"hz", e.hazardScore},
               {"reuse", e.reuseProximity},
               {"local", e.localityScore},
               {"risk", e.concurrencyRisk},
               {"conflict", e.mayConflict},
               {"a", na ? sanitizeString(na->getHierId()) : std::string("")},
               {"b", nb ? sanitizeString(nb->getHierId()) : std::string("")}});
  }
  root["edges"] = std::move(edgesArr);

  /// Include analysis data if requested
  if (includeAnalysis) {
    root["analysis"] = Object{{"numTasks", (double)edts.size()},
                              {"numEdges", (double)edges.size()},
                              {"hasAnalysis", analysis != nullptr}};
  }

  os << llvm::json::Value(std::move(root)) << "\n";
}
