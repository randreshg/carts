//===----------------------------------------------------------------------===//
// GraphBase.h - Abstract base class for graphs in ARTS analysis
//===----------------------------------------------------------------------===//

#ifndef ARTS_ANALYSIS_GRAPHS_GRAPHBASE_H
#define ARTS_ANALYSIS_GRAPHS_GRAPHBASE_H

#include "mlir/IR/Operation.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/Support/raw_ostream.h"
#include <memory>
#include <vector>

namespace mlir {
namespace arts {

class NodeBase;
class EdgeBase;


/// Abstract base class for all graphs (e.g., DbGraph, EdtGraph).
/// Provides common interface for building, invalidating, printing, and exporting.
/// Manages nodes and edges polymorphically.
class GraphBase {
public:
  virtual ~GraphBase() = default;

  /// Build the graph from the IR (e.g., walk func to collect nodes/edges).
  virtual void build() = 0;

  /// Invalidate and clear the graph state.
  virtual void invalidate() = 0;

  /// Print a human-readable summary of the graph.
  virtual void print(llvm::raw_ostream &os) const = 0;

  /// Export the graph to DOT format for Graphviz.
  virtual void exportToDot(llvm::raw_ostream &os) const = 0;

  /// Get the entry node (e.g., first alloc or root task).
  virtual NodeBase *getEntryNode() const = 0;

  /// Get or create a node for the given operation (polymorphic dispatch).
  virtual NodeBase *getOrCreateNode(Operation *op) = 0;

  /// Get a node for the given operation if it exists.
  virtual NodeBase *getNode(Operation *op) const = 0;

  /// Iterate over all nodes.
  virtual void forEachNode(const std::function<void(NodeBase *)> &fn) const = 0;

  /// Add an edge between nodes (returns true if inserted).
  virtual bool addEdge(NodeBase *from, NodeBase *to, EdgeBase *edge) = 0;

  /// For GraphTraits: iterators over nodes.
  using NodesIterator = std::vector<NodeBase *>::iterator;
  virtual NodesIterator nodesBegin() = 0;
  virtual NodesIterator nodesEnd() = 0;

  /// For children (outgoing neighbors).
  using ChildIterator = std::vector<NodeBase *>::iterator;
  virtual ChildIterator childBegin(NodeBase *node) = 0;
  virtual ChildIterator childEnd(NodeBase *node) = 0;

protected:
  // Protected members for derived classes to manage state.
  std::vector<std::unique_ptr<NodeBase>> nodes;
  DenseMap<std::pair<NodeBase *, NodeBase *>, std::unique_ptr<EdgeBase>> edges;
  bool isBuilt = false;
  bool needsRebuild = true;
};

} // namespace arts
} // namespace mlir

#endif // ARTS_ANALYSIS_GRAPHS_GRAPHBASE_H