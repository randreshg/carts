///==========================================================================
/// File: EdtConcurrencyGraph.cpp
///
/// This file implements the concurrency graph for EDT analysis.
///==========================================================================

#include "arts/Analysis/Graphs/Edt/EdtConcurrencyGraph.h"

using namespace mlir;
using namespace mlir::arts;

void EdtConcurrencyGraph::clear() {
  tasks.clear();
  edges.clear();
}

void EdtConcurrencyGraph::build() {
  clear();
  if (!edtGraph || !edtGraph->hasDbGraph())
    return;

  /// Collect all tasks in a stable order
  edtGraph->forEachNode([&](NodeBase *node) {
    if (auto *t = dyn_cast<EdtTaskNode>(node))
      tasks.push_back(t->getEdtOp().getOperation());
  });

  /// Build undirected edges A-B where neither is reachable from the other
  for (size_t i = 0; i < tasks.size(); ++i) {
    for (size_t j = i + 1; j < tasks.size(); ++j) {
      EdtOp a = static_cast<EdtOp>(tasks[i]);
      EdtOp b = static_cast<EdtOp>(tasks[j]);
      bool depAB = edtGraph->isTaskReachable(a, b);
      bool depBA = edtGraph->isTaskReachable(b, a);
      if (!depAB && !depBA) {
        EdtConcurrencyEdge e{};
        e.a = a.getOperation();
        e.b = b.getOperation();
        if (analysis) {
          auto aff = analysis->affinity(e.a, e.b);
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
  os << "EdtConcurrencyGraph: tasks=" << tasks.size()
     << ", edges=" << edges.size() << "\n";
  for (auto &e : edges) {
    os << "  (" << e.a << ", " << e.b << ")"
       << " overlap=" << e.dataOverlap << " hazard=" << e.hazardScore
       << " reuse=" << e.reuseProximity << " local=" << e.localityScore
       << " risk=" << e.concurrencyRisk
       << " conflict=" << (e.mayConflict ? 1 : 0) << "\n";
  }
}

void EdtConcurrencyGraph::exportToDot(llvm::raw_ostream &os) const {
  os << "graph EdtConcurrency {\n";
  /// Nodes
  for (auto t : tasks) {
    auto *n = static_cast<EdtTaskNode *>(edtGraph->getNode(t));
    if (!n)
      continue;
    os << "  " << n->getHierId() << " [label=\"" << n->getHierId() << "\"];\n";
  }
  /// Edges
  for (auto &e : edges) {
    auto *na = static_cast<EdtTaskNode *>(edtGraph->getNode(e.a));
    auto *nb = static_cast<EdtTaskNode *>(edtGraph->getNode(e.b));
    if (!na || !nb)
      continue;
    os << "  " << na->getHierId() << " -- " << nb->getHierId()
       << " [label=\"ovl=" << e.dataOverlap << ",hz=" << e.hazardScore
       << "\"];\n";
  }
  os << "}\n";
}

void EdtConcurrencyGraph::exportToJson(llvm::raw_ostream &os) const {
  os << "{\n  \"tasks\": [\n";
  bool first = true;
  for (auto t : tasks) {
    if (!first)
      os << ",\n";
    first = false;
    auto *n = static_cast<EdtTaskNode *>(edtGraph->getNode(t));
    os << "    {\"id\": \"" << (n ? n->getHierId() : "") << "\"}";
  }
  os << "\n  ],\n  \"edges\": [\n";
  first = true;
  for (auto &e : edges) {
    if (!first)
      os << ",\n";
    first = false;
    auto *na = static_cast<EdtTaskNode *>(edtGraph->getNode(e.a));
    auto *nb = static_cast<EdtTaskNode *>(edtGraph->getNode(e.b));
    os << "    {\"a\": \"" << (na ? na->getHierId() : "") << "\", \"b\": \""
       << (nb ? nb->getHierId() : "") << "\", \"ovl\": " << e.dataOverlap
       << ", \"hz\": " << e.hazardScore << ", \"reuse\": " << e.reuseProximity
       << ", \"local\": " << e.localityScore
       << ", \"risk\": " << e.concurrencyRisk
       << ", \"conflict\": " << (e.mayConflict ? "true" : "false") << "}";
  }
  os << "\n  ]\n}\n";
}
