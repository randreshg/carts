///==========================================================================///
/// File: EdtGraph.h
///
/// Defines EdtGraph for EDT analysis.
///==========================================================================///

#ifndef ARTS_ANALYSIS_GRAPHS_EDT_EDTGRAPH_H
#define ARTS_ANALYSIS_GRAPHS_EDT_EDTGRAPH_H

#include "arts/Dialect.h" // For EdtOp
#include "arts/analysis/graphs/base/EdgeBase.h"
#include "arts/analysis/graphs/base/NodeBase.h"
#include "arts/analysis/graphs/db/DbGraph.h"
#include "arts/analysis/graphs/edt/EdtEdge.h"
#include "arts/analysis/graphs/edt/EdtNode.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/Support/JSON.h"
#include "llvm/Support/raw_ostream.h"
#include <memory>

namespace mlir {
namespace arts {

class EdtDepEdge;
class EdtAnalysis;

/// Represents task dependencies with edges labeled by data blocks.
class EdtGraph {
public:
  EdtGraph(func::FuncOp func, DbGraph *dbGraph, EdtAnalysis *EA);

  void build();
  void buildNodesOnly();
  void invalidate();
  void print(llvm::raw_ostream &os);
  llvm::json::Value exportToJsonValue(bool includeAnalysis = false) const;
  void exportToJson(llvm::raw_ostream &os,
                    bool includeAnalysis = false) const;
  NodeBase *getEntryNode() const;
  NodeBase *getOrCreateNode(Operation *op);
  NodeBase *getNode(Operation *op) const;
  EdtNode *getEdtNode(EdtOp edt) const;
  void forEachNode(const std::function<void(NodeBase *)> &fn) const;
  bool addEdge(NodeBase *from, NodeBase *to, EdgeBase *edge);

  /// Edt-specific methods
  bool isEdtReachable(EdtOp from, EdtOp to);
  func::FuncOp getFunction() const { return func; }
  bool hasDbGraph() const { return dbGraph != nullptr; }
  DbGraph *getDbGraph() const { return dbGraph; }
  size_t size() const { return nodes.size(); }

  /// Check if two EDTs are independent (no shared DB accesses)
  bool areEdtsIndependent(EdtOp a, EdtOp b);

private:
  func::FuncOp func;
  DbGraph *dbGraph;
  EdtAnalysis *edtAnalysis;
  DenseMap<EdtOp, std::unique_ptr<EdtNode>> edtNodes;
  SmallVector<NodeBase *, 8> nodes;
  DenseMap<std::pair<NodeBase *, NodeBase *>, std::unique_ptr<EdgeBase>> edges;
  bool isBuilt = false;
  bool needsRebuild = true;

  /// Private helpers
  void collectNodes();
  void linkEdtsToLoops();
  void buildDependencies();
};

} // namespace arts
} // namespace mlir

#endif // ARTS_ANALYSIS_GRAPHS_EDT_EDTGRAPH_H
