///==========================================================================///
/// File: EdtEdge.cpp
/// Implementation of EDT edges for graph analysis.
///==========================================================================///

#include "carts/dialect/arts/Analysis/graphs/edt/EdtEdge.h"
#include "carts/dialect/arts/Analysis/graphs/base/NodeBase.h"

using namespace mlir::carts;
using namespace mlir::carts::arts;

EdtDepEdge::EdtDepEdge(NodeBase *from, NodeBase *to, const DbEdge &edge)
    : EdtDepEdge(from, to, ArrayRef<DbEdge>(edge)) {}

EdtDepEdge::EdtDepEdge(NodeBase *from, NodeBase *to, ArrayRef<DbEdge> edges)
    : from(from), to(to), dbEdges(edges.begin(), edges.end()) {
  assert(from && "Source node cannot be null");
  assert(to && "Destination node cannot be null");
  assert(!dbEdges.empty() && "EDT dependency edge must carry DB fanout");
}

void EdtDepEdge::appendDbEdges(ArrayRef<DbEdge> edges) {
  assert(!edges.empty() && "Cannot append empty DB fanout");
  for (const DbEdge &edge : edges) {
    bool found = false;
    for (const DbEdge &existing : dbEdges) {
      if (existing == edge) {
        found = true;
        break;
      }
    }
    if (!found)
      dbEdges.push_back(edge);
  }
}
