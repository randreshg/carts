//===----------------------------------------------------------------------===//
// Edt/EdtGraph.h - EdtGraph derived from GraphBase
//===----------------------------------------------------------------------===//

#ifndef ARTS_ANALYSIS_GRAPHS_EDT_EDTGRAPH_H
#define ARTS_ANALYSIS_GRAPHS_EDT_EDTGRAPH_H

#include "arts/Analysis/Graphs/Db/DbGraph.h"
#include "arts/Analysis/Graphs/Base/EdgeBase.h"
#include "arts/Analysis/Graphs/Base/GraphBase.h"
#include "arts/Analysis/Graphs/Base/NodeBase.h"
#include "arts/ArtsDialect.h" // For EdtOp
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/GraphTraits.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include <memory>

#define DEBUG_TYPE "edt-graph"

namespace mlir {
namespace arts {

class EdtTaskNode;
class EdtDepEdge;

// EdtGraph: Represents task dependencies with edges labeled by data blocks.
class EdtGraph : public GraphBase {
public:
  EdtGraph(func::FuncOp func, DbGraph *dbGraph);

  void build() override;
  void invalidate() override;
  void print(llvm::raw_ostream &os) const override;
  void exportToDot(llvm::raw_ostream &os) const override;
  NodeBase *getEntryNode() const override;
  NodeBase *getOrCreateNode(Operation *op) override;
  NodeBase *getNode(Operation *op) const override;
  void forEachNode(const std::function<void(NodeBase *)> &fn) const override;
  bool addEdge(NodeBase *from, NodeBase *to, EdgeBase *edge) override;

  // Edt-specific methods
  bool isTaskReachable(EdtOp from, EdtOp to);
  llvm::SmallVector<NodeBase *> getDbDependencies(EdtOp task) const;

  // For GraphTraits iterators
  NodesIterator nodesBegin() override { return nodes.begin(); }
  NodesIterator nodesEnd() override { return nodes.end(); }

private:
  func::FuncOp func;
  DbGraph *dbGraph; // Reference to data block graph
  DenseMap<EdtOp, std::unique_ptr<EdtTaskNode>> taskNodes;
  std::vector<NodeBase *> nodes; // Non-owning for iteration

  // Private helpers
  void collectNodes();
  void buildDependencies();
  std::string sanitizeForDot(StringRef s) const;
};

} // namespace arts
} // namespace mlir

// GraphTraits specialization for EdtGraph
namespace llvm {
template <>
struct GraphTraits<mlir::arts::EdtGraph *>
    : public BaseGraphTraits<mlir::arts::EdtGraph> {
  static auto getTo(mlir::arts::EdgeBase *edge) { return edge->getTo(); }

  static ChildIteratorType child_begin(NodeRef N) {
    return map_iterator(N->getOutEdges().begin(), &getTo);
  }
  static ChildIteratorType child_end(NodeRef N) {
    return map_iterator(N->getOutEdges().end(), &getTo);
  }
};

template <>
struct DOTGraphTraits<mlir::arts::EdtGraph *> : public DefaultDOTGraphTraits {
  DOTGraphTraits(bool simple = false) : DefaultDOTGraphTraits(simple) {}

  std::string getNodeLabel(mlir::arts::NodeBase *Node,
                           mlir::arts::EdtGraph *Graph) {
    return Node->getHierId().str();
  }

  std::string getEdgeAttributes(mlir::arts::NodeBase *From,
                                typename BaseGraphTraits<
                                    mlir::arts::EdtGraph>::ChildIteratorType It,
                                mlir::arts::EdtGraph *Graph) {
    mlir::arts::NodeBase *To = *It;
    auto edges = From->getOutEdges();
    for (auto *edge : edges) {
      if (edge->getTo() == To) {
        return "label=\"" + edge->getType().str() + "\"";
      }
    }
    return "";
  }
};

} // namespace llvm

#endif // ARTS_ANALYSIS_GRAPHS_EDT_EDTGRAPH_H