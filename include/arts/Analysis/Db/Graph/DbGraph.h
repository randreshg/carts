#ifndef ARTS_ANALYSIS_DB_GRAPH_DBGRAPH_H
#define ARTS_ANALYSIS_DB_GRAPH_DBGRAPH_H

#include "arts/Analysis/Db/DbDataFlowAnalysis.h"
#include "arts/Analysis/Db/Graph/DbEdge.h"
#include "arts/Analysis/Db/Graph/DbNode.h"
#include "arts/ArtsDialect.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Support/LLVM.h"
#include <cassert>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

namespace mlir {
namespace arts {
using namespace std;

class DbGraph {
public:
  DbGraph(func::FuncOp func, DbAnalysis *analysis);
  bool isAllocReachable(DbAllocOp from, DbAllocOp to);
  bool isAccessReachable(DbAccessOp from, DbAccessOp to);
  bool hasDataDep(DbAccessOp from, DbAccessOp to);
  DbAllocOp getParentAlloc(DbAccessOp access);
  bool mayAlias(DbAllocOp alloc1, DbAllocOp alloc2);
  bool mayAlias(DbAccessOp access1, DbAccessOp access2);

  DbAllocNode *getOrCreateAllocNode(DbAllocOp dbAllocOp);
  DbAllocNode *getAllocNode(DbAllocOp dbAllocOp);
  DbAccessNode *getOrCreateAccessNode(DbAllocOp allocation, DbAccessOp access);
  DbAccessNode *getAccessNode(DbAccessOp access);

  bool insertDepEdge(DbAccessOp from, DbAccessOp to, DbDepType type);
  bool insertAllocEdge(DbAllocOp from, DbAllocOp to);
  DbDepEdge *getDependenceEdge(DbAccessOp from, DbAccessOp to);
  DbAllocEdge *getAllocEdge(DbAllocOp from, DbAllocOp to);

  const DenseSet<DbDepEdge *> &getInDepEdges(DbAccessNode *node);
  const DenseSet<DbDepEdge *> &getOutDepEdges(DbAccessNode *node);
  const DenseSet<DbAllocEdge *> &getInAllocEdges(DbAllocNode *node);
  const DenseSet<DbAllocEdge *> &getOutAllocEdges(DbAllocNode *node);

  void forEachAllocNode(const function<void(DbAllocNode *)> &fn);
  void forEachAccessNode(const function<void(DbAccessNode *)> &fn);

  void build();
  void invalidate();

  void print(llvm::raw_ostream &os);
  void printStatistics(llvm::raw_ostream &os);
  void exportToDot(llvm::raw_ostream &os);

  bool hasNodes() const { return !allocNodes.empty(); }
  bool hasEdges() const { return !depEdges.empty(); }

  func::FuncOp getFunction() const { return func; }
  DbInfo *getNode(Operation *op);
  DbInfo *getNode(DbControlOp dbOp);
  DbInfo *getEntryNode();
  bool addEdge(DbAccessOp from, DbAccessOp to, DbDepType type);
  bool addEdge(DbAllocOp from, DbAllocOp to);
  size_t getNumEdges() const;

private:
  func::FuncOp func;
  DbAnalysis *analysis;
  std::unique_ptr<DbDataFlowAnalysis> dataFlowAnalysis;
  bool isBuilt = false;
  bool needsRebuild = true;
  unsigned nextAllocId;

  DenseMap<DbAllocOp, unique_ptr<DbAllocNode>> allocNodes;
  DenseMap<DbAccessOp, DbAccessNode *> accessNodeMap;
  DenseMap<pair<DbAccessNode *, DbAccessNode *>, unique_ptr<DbDepEdge>> depEdges;
  DenseMap<pair<DbAllocNode *, DbAllocNode *>, unique_ptr<DbAllocEdge>> allocEdges;

  void collectNodes();
  void buildDependencies();
};

} // namespace arts
} // namespace mlir

#endif // ARTS_ANALYSIS_DB_GRAPH_DBGRAPH_H