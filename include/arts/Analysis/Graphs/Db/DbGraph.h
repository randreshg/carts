//===----------------------------------------------------------------------===//
// Db/DbGraph.h - DbGraph derived from GraphBase
//===----------------------------------------------------------------------===//

#ifndef ARTS_ANALYSIS_GRAPHS_DB_DBGRAPH_H
#define ARTS_ANALYSIS_GRAPHS_DB_DBGRAPH_H

#include "arts/Analysis/Graphs/Base/EdgeBase.h"
#include "arts/Analysis/Graphs/Base/GraphBase.h"
#include "arts/Analysis/Graphs/Base/GraphTrait.h"
#include "arts/Analysis/Graphs/Base/NodeBase.h"
#include "arts/ArtsDialect.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/GraphTraits.h"
#include "llvm/ADT/STLExtras.h"
#include <memory>

namespace mlir {
namespace arts {

// Forward declarations
class DbAnalysis;

class DbAllocNode;
class DbAcquireNode;
class DbReleaseNode;
class DbAllocEdge;
class DbLifetimeEdge;
class DbDataFlowAnalysis;

// Forward declarations
// class DbAnalysis;
class DbNode;
class DbEdge;

// DbGraph: Specialized graph for data blocks, acquires, and releases.
class DbGraph : public GraphBase {
public:
  DbGraph(func::FuncOp func, DbAnalysis *analysis);
  ~DbGraph();

  void build() override;
  void invalidate() override;
  void print(llvm::raw_ostream &os) const override;
  void exportToDot(llvm::raw_ostream &os) const override;
  NodeBase *getEntryNode() const override;
  NodeBase *getOrCreateNode(Operation *op) override;
  NodeBase *getNode(Operation *op) const override;
  void forEachNode(const std::function<void(NodeBase *)> &fn) const override;
  bool addEdge(NodeBase *from, NodeBase *to, EdgeBase *edge) override;

  /// Db-specific methods
  bool isAllocReachable(DbAllocOp from, DbAllocOp to);
  DbAllocOp getParentAlloc(Operation *op);
  bool mayAlias(DbAllocOp alloc1, DbAllocOp alloc2);

  /// For acquire/release
  bool hasAcquireReleasePair(DbAllocOp alloc);

  /// For GraphTraits iterators
  NodesIterator nodesBegin() override { return nodes.begin(); }
  NodesIterator nodesEnd() override { return nodes.end(); }
  ChildIterator childBegin(NodeBase *node) override;
  ChildIterator childEnd(NodeBase *node) override;

private:
  func::FuncOp func;
  DbAnalysis *analysis;
  std::unique_ptr<DbDataFlowAnalysis> dataFlowAnalysis;

  /// Node maps
  DenseMap<DbAllocOp, std::unique_ptr<DbAllocNode>> allocNodes;
  DenseMap<DbAcquireOp, DbAcquireNode *> acquireNodeMap;
  DenseMap<DbReleaseOp, DbReleaseNode *> releaseNodeMap;

  /// All nodes (non-owning for iteration)
  std::vector<NodeBase *> nodes;

  unsigned nextAllocId = 1;

  /// Private helpers
  void collectNodes();
  void buildDependencies();
  DbAllocOp findRootAllocOp(Operation *op);
  std::string generateAllocId(unsigned id);
  std::string sanitizeForDot(StringRef s) const;
  std::string getFunctionName() const;
};

} // namespace arts
} // namespace mlir

// GraphTraits specialization for DbGraph - uses base implementation
namespace llvm {
template <>
struct GraphTraits<mlir::arts::DbGraph *>
    : public BaseGraphTraits<mlir::arts::DbGraph> {};
} // namespace llvm

#endif // ARTS_ANALYSIS_GRAPHS_DB_DBGRAPH_H