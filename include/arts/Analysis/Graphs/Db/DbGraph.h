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
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/GraphTraits.h"
#include "llvm/ADT/STLExtras.h"
#include <functional>
#include <memory>
#include <string>

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

// DbGraph: Specialized graph for data blocks (alloc/acquire/release).
// Responsibility: build nodes/edges over ARTS DB ops; provide queries like
// parent alloc, reachability, and acquire/release pairing. No IR mutation.
class DbGraph : public GraphBase {
public:
  DbGraph(func::FuncOp func, DbAnalysis *analysis);
  ~DbGraph();

  void build() override;
  void invalidate() override;
  void print(llvm::raw_ostream &os) const override;
  void exportToDot(llvm::raw_ostream &os) const override;
  void exportToJson(llvm::raw_ostream &os) const;
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
  DbDataFlowAnalysis *dataFlowAnalysis = nullptr;

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
  void computeOpOrder();
  void computeMetrics();
  void computeReuseColoring();
  DbAllocOp findRootAllocOp(Operation *op);
  std::string generateAllocId(unsigned id);
  std::string sanitizeForDot(StringRef s) const;
  std::string getFunctionName() const;

public:
  /// Iterate only allocation nodes
  void forEachAllocNode(const std::function<void(DbAllocNode *)> &fn) const {
    for (const auto &pair : allocNodes) fn(pair.second.get());
  }

  struct AcquireInterval {
    unsigned beginIndex = 0;
    unsigned endIndex = 0;   // inclusive order index of matching release
    DbAcquireOp acquire;
    DbReleaseOp release;
  };

  struct DbAllocMetrics {
    unsigned allocIndex = 0;
    unsigned endIndex = 0;   // last release order
    unsigned numAcquires = 0;
    unsigned numReleases = 0;
    uint64_t staticBytes = 0; // 0 if unknown
    SmallVector<AcquireInterval, 8> intervals;
    bool isLongLived = false;
    bool maybeEscaping = false;
    unsigned maxLoopDepth = 0;     // from LoopAnalysis
    unsigned criticalSpan = 0;     // union length of intervals in op order
    unsigned criticalPath = 0;     // precise union of disjoint intervals
    uint64_t totalAccessBytes = 0; // sum of per-interval estimated bytes
  };

  const DenseMap<DbAllocOp, DbAllocMetrics> &getAllocMetrics() const {
    return allocMetrics;
  }

private:
  DenseMap<Operation *, unsigned> opOrder;
  DenseMap<DbAllocOp, DbAllocMetrics> allocMetrics;
  uint64_t peakLiveDbs = 0;
  uint64_t peakBytes = 0; // 0 if unknown
  SmallVector<std::pair<DbAllocOp, DbAllocOp>, 8> reuseCandidates;
  DenseMap<DbAllocOp, unsigned> allocColor;               // color per alloc
  DenseMap<unsigned, SmallVector<DbAllocOp>> colorGroups; // color -> allocs
  
  // Utilities
  unsigned getOrder(Operation *op) const {
    auto it = opOrder.find(op);
    return it == opOrder.end() ? 0u : it->second;
  }
  static bool isConstantIndex(Value v, int64_t &cst);
  static uint64_t getElementTypeByteSize(Type elemTy);
  static bool rangesOverlap(int64_t a0, int64_t a1, int64_t b0, int64_t b1) {
    return !(a1 <= b0 || b1 <= a0);
  }
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