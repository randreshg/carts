///==========================================================================///
/// File: EdtGraph.h
///
/// Defines EdtGraph derived from GraphBase for EDT analysis.
///==========================================================================///

#ifndef ARTS_ANALYSIS_GRAPHS_EDT_EDTGRAPH_H
#define ARTS_ANALYSIS_GRAPHS_EDT_EDTGRAPH_H

#include "arts/ArtsDialect.h" // For EdtOp
#include "arts/analysis/graphs/base/EdgeBase.h"
#include "arts/analysis/graphs/base/GraphBase.h"
#include "arts/analysis/graphs/base/GraphTrait.h"
#include "arts/analysis/graphs/base/NodeBase.h"
#include "arts/analysis/graphs/db/DbGraph.h"
#include "arts/analysis/graphs/edt/EdtEdge.h"
#include "arts/analysis/graphs/edt/EdtNode.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/GraphTraits.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include <memory>

namespace mlir {
namespace arts {

class EdtDepEdge;
class AnalysisManager;

/// Represents task dependencies with edges labeled by data blocks.
class EdtGraph : public GraphBase {
public:
  EdtGraph(func::FuncOp func, DbGraph *dbGraph, AnalysisManager *AM);

  void build() override;
  void buildNodesOnly() override;
  void invalidate() override;
  void print(llvm::raw_ostream &os) override;
  void exportToJson(llvm::raw_ostream &os,
                    bool includeAnalysis = false) const override;
  NodeBase *getEntryNode() const override;
  NodeBase *getOrCreateNode(Operation *op) override;
  NodeBase *getNode(Operation *op) const override;
  EdtNode *getEdtNode(EdtOp edt) const;
  void forEachNode(const std::function<void(NodeBase *)> &fn) const override;
  bool addEdge(NodeBase *from, NodeBase *to, EdgeBase *edge) override;

  /// Edt-specific methods
  bool isEdtReachable(EdtOp from, EdtOp to);
  func::FuncOp getFunction() const { return func; }
  bool hasDbGraph() const { return dbGraph != nullptr; }
  DbGraph *getDbGraph() const { return dbGraph; }

  /// Check if two EDTs are independent (no shared DB accesses)
  bool areEdtsIndependent(EdtOp a, EdtOp b);

  /// For GraphTraits iterators
  NodesIterator nodesBegin() override { return nodes.begin(); }
  NodesIterator nodesEnd() override { return nodes.end(); }
  ChildIterator childBegin(NodeBase *node) override;
  ChildIterator childEnd(NodeBase *node) override;

private:
  func::FuncOp func;
  DbGraph *dbGraph;
  AnalysisManager *analysisManager;
  DenseMap<EdtOp, std::unique_ptr<EdtNode>> edtNodes;
  SmallVector<NodeBase *, 8> nodes;
  /// Cache of children per node for GraphBase child iterators.
  DenseMap<NodeBase *, SmallVector<NodeBase *, 8>> childrenCache;

  /// Ensure DenseMap header is considered directly used
  using __EnsureDenseMapUsed = llvm::DenseMap<int, int>;

  /// Private helpers
  void collectNodes();
  void linkEdtsToLoops();
  void buildDependencies();
  void populateChildrenCache(NodeBase *node);
};

} // namespace arts
} // namespace mlir

/// GraphTraits specialization for EdtGraph
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

} // namespace llvm

#endif // ARTS_ANALYSIS_GRAPHS_EDT_EDTGRAPH_H
