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
  bool isDepReachable(DbDepOp from, DbDepOp to);
  bool hasDataDep(DbDepOp from, DbDepOp to);
  DbAllocOp getParentAlloc(DbDepOp dep);
  bool mayAlias(DbAllocOp alloc1, DbAllocOp alloc2);
  bool mayAlias(DbDepOp dep1, DbDepOp dep2);

  DbAllocNode *getOrCreateAllocNode(DbAllocOp dbAllocOp);
  DbAllocNode *getAllocNode(DbAllocOp dbAllocOp);
  DbDepNode *getOrCreateDepNode(DbAllocOp allocation, DbDepOp dep);
  DbDepNode *getDepNode(DbDepOp dep);

  bool insertDepEdge(DbDepOp from, DbDepOp to, DbDepType type);
  bool insertAllocEdge(DbAllocOp from, DbAllocOp to);
  DbDepEdge *getDependenceEdge(DbDepOp from, DbDepOp to);
  DbAllocEdge *getAllocEdge(DbAllocOp from, DbAllocOp to);

  const DenseSet<DbDepEdge *> &getInDepEdges(DbDepNode *node);
  const DenseSet<DbDepEdge *> &getOutDepEdges(DbDepNode *node);
  const DenseSet<DbAllocEdge *> &getInAllocEdges(DbAllocNode *node);
  const DenseSet<DbAllocEdge *> &getOutAllocEdges(DbAllocNode *node);

  void forEachAllocNode(const function<void(DbAllocNode *)> &fn);
  void forEachDepNode(const function<void(DbDepNode *)> &fn);

  void build();
  void invalidate();

  void print(llvm::raw_ostream &os);
  void printStatistics(llvm::raw_ostream &os);
  void exportToDot(llvm::raw_ostream &os);

  bool hasNodes() const { return !allocNodes.empty(); }
  bool hasEdges() const { return !depEdges.empty(); }

  func::FuncOp getFunction() const { return func; }
  DbInfo *getNode(Operation *op);
  DbInfo *getNode(DbDepOp dbOp);
  DbInfo *getEntryNode();
  bool addEdge(DbDepOp from, DbDepOp to, DbDepType type);
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
  DenseMap<DbDepOp, DbDepNode *> depNodeMap;
  DenseMap<pair<DbDepNode *, DbDepNode *>, unique_ptr<DbDepEdge>> depEdges;
  DenseMap<pair<DbAllocNode *, DbAllocNode *>, unique_ptr<DbAllocEdge>> allocEdges;

  void collectNodes();
  void buildDependencies();
};

} // namespace arts
} // namespace mlir

#endif // ARTS_ANALYSIS_DB_GRAPH_DBGRAPH_H