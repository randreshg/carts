///==========================================================================///
/// File: DbGraph.h
///
/// Defines DbGraph as a lightweight hierarchy for database operations.
///==========================================================================///

#ifndef ARTS_ANALYSIS_GRAPHS_DB_DBGRAPH_H
#define ARTS_ANALYSIS_GRAPHS_DB_DBGRAPH_H

#include "arts/ArtsDialect.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "llvm/ADT/DenseMap.h"
#include <functional>
#include <memory>
#include <string>

namespace mlir {
namespace arts {

/// Forward declarations
class DbAnalysis;
class ArtsMetadataManager;

class DbAllocNode;
class DbAcquireNode;

/// DbGraph: Maintains the hierarchy of DbAlloc/DbAcquire nodes plus metrics.
class DbGraph {
public:
  DbGraph(func::FuncOp func, DbAnalysis *analysis);
  ~DbGraph();

  void build();
  void buildNodesOnly();
  void invalidate();
  void print(llvm::raw_ostream &os);

  /// Export graph to JSON
  void exportToJson(llvm::raw_ostream &os, bool includeAnalysis = false) const;

  /// Retrieve node accessors by concrete op type
  DbAllocNode *getDbAllocNode(DbAllocOp op) const;
  DbAcquireNode *getDbAcquireNode(DbAcquireOp op) const;

  /// Convenience methods for getting or creating specific node types
  DbAllocNode *getOrCreateAllocNode(DbAllocOp op);
  DbAcquireNode *getOrCreateAcquireNode(DbAcquireOp op);

  /// Apply a function to every node (alloc/acquire)
  void forEachAllocNode(const std::function<void(DbAllocNode *)> &fn) const;
  void forEachAcquireNode(const std::function<void(DbAcquireNode *)> &fn) const;

  /// Get allocation info for a given allocation operation
  const DbAllocNode &getAllocInfo(DbAllocOp alloc) const;

  /// Get operation order for ordering comparisons
  unsigned getOpOrder(Operation *op) const {
    auto it = opOrder.find(op);
    return it == opOrder.end() ? 0u : it->second;
  }

  /// Get DbAnalysis instance
  DbAnalysis *getAnalysis() const { return analysis; }
  func::FuncOp getFunction() const { return func; }

private:
  func::FuncOp func;
  DbAnalysis *analysis;

  /// Node maps
  DenseMap<DbAllocOp, std::unique_ptr<DbAllocNode>> allocNodes;
  DenseMap<DbAcquireOp, DbAcquireNode *> acquireNodeMap;

  unsigned nextAllocId = 1;

  /// Private helpers
  void collectNodes();
  void computeOpOrder();
  void computeMetrics();
  std::string generateAllocId(unsigned id);

  /// Metrics computation helpers
  void computeAllocMetrics(DbAllocOp alloc, DbAllocNode *allocNode);
  void processAcquireNode(DbAcquireNode *acq, DbAllocNode &info);
  void
  computeCriticalSpan(DbAllocNode &info,
                      const SmallVectorImpl<DbAcquireNode *> &acquireNodes);
  void
  computeCriticalPath(DbAllocNode &info,
                      const SmallVectorImpl<DbAcquireNode *> &acquireNodes);
  void computeLoopDepth(DbAllocNode &info,
                        const SmallVectorImpl<DbAcquireNode *> &acquireNodes);
  void computeLongLivedFlag(DbAllocNode &info);
  void computeEscapingFlag(DbAllocOp alloc, DbAllocNode &info);
  void computePeakMetrics();
  void computeReuseCandidates();

private:
  DenseMap<Operation *, unsigned> opOrder;
  uint64_t peakLiveDbs = 0;
  unsigned long long peakBytes = 0;
  bool built = false;
  bool needsRebuild = true;
};

} // namespace arts
} // namespace mlir

#endif // ARTS_ANALYSIS_GRAPHS_DB_DBGRAPH_H
