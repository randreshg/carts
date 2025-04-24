///==========================================================================
/// File: DataBlockAnalysis.h
///
/// This pass performs a comprehensive analysis on arts.datablock operations.
///==========================================================================

#ifndef CARTS_ANALYSIS_DATABLOCKANALYSIS_H
#define CARTS_ANALYSIS_DATABLOCKANALYSIS_H

#include "arts/Analysis/LoopAnalysis.h"
#include "arts/ArtsDialect.h"
#include "mlir/Analysis/FlatLinearValueConstraints.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/AffineMap.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/Dominance.h"
#include "mlir/IR/Types.h"
#include "mlir/Pass/AnalysisManager.h"
#include "mlir/Support/LLVM.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/IR/PassManager.h"
#include <cstdint>
#include <sys/types.h>
#include <utility>

namespace mlir {
namespace arts {

class DatablockAnalysis;
class DatablockNode;
class DatablockGraph;
enum DatablockNodeComp { Equal, BaseAlias, Different };

/// An Environment maps each db op to the latest Node that defines it.
using Environment = llvm::DenseMap<arts::DataBlockOp, DatablockNode *>;

//===----------------------------------------------------------------------===//
// DatablockNode
// Node representing one arts.datablock op in the dependency graph.
//===----------------------------------------------------------------------===//
class DatablockNode {
  friend class DatablockGraph;
  friend class DatablockAnalysis;

public:
  DatablockNode(DatablockGraph *DG, arts::DataBlockOp dbOp);

  /// Interface
  bool isWriter() { return mode == "out" || mode == "inout"; }
  bool isReader() { return mode == "in" || mode == "inout"; }
  bool isOnlyReader() { return mode == "in"; }
  bool isOnlyWriter() { return mode == "out"; }

  /// Attributes
  unsigned id = 0;
  arts::DataBlockOp op;

  /// Db attributtes
  StringRef mode;
  Value ptr;
  SmallVector<Value, 4> indices, sizes;
  Type elementType;
  Value elementTypeSize;
  MemRefType resultType;
  DomainAttr domain;
  bool hasPtrDb, hasSingleSize, hasGuid;

  /// Analysis results
  SmallVector<MemoryEffects::EffectInstance, 2> effects;

  /// Uses
  unsigned useCount;
  DatablockNode *parent = nullptr;
  arts::EdtOp edtParent = nullptr;
  arts::EdtOp edtUser = nullptr;
  uint32_t userEdtPos = 0;

  /// A duplicated node is a node with the same mode, aliasing base memref,
  /// and indices.
  SetVector<unsigned> duplicates;
  /// Set of nodes that alias this node.
  SetVector<unsigned> aliases;
  /// List of stores and loads that access the same db.
  SmallVector<Operation *, 4> loadsAndStores;

private:
  DatablockGraph *DG;
  DatablockAnalysis *DA;

  /// Uses
  void collectInfo();
  void collectUses();

  /// Compute region for affine ops or fallback for memref ops.
  bool computeRegion();
};

//===----------------------------------------------------------------------===//
// DatablockGraph
//===----------------------------------------------------------------------===//
class DatablockGraph {
public:
  explicit DatablockGraph(func::FuncOp func, DatablockAnalysis *DA);
  ~DatablockGraph() {
    for (auto *node : nodes)
      delete node;
  }
  friend class DatablockAnalysis;
  friend class DatablockNode;
  using Edge = std::pair<unsigned, unsigned>; // producerID, consumerID

  /// Interface
  func::FuncOp getFunction() { return func; }
  SmallVector<DatablockNode *, 4> &getNodes() { return nodes; }
  DatablockNode *getNode(arts::DataBlockOp dbOp) { return nodeMap[dbOp]; }
  DatablockNode *getNode(unsigned id) { return nodes[id]; }
  bool isEntryNode(unsigned id) const { return id == entryDbNode->id; }
  bool hasNodes() const { return !nodes.empty(); }
  SetVector<unsigned> getProducers() const;
  SetVector<unsigned> getConsumers(unsigned producerID);

  /// Add an edge from a producer node to a consumer node.
  bool addEdge(DatablockNode &prod, DatablockNode &cons);

  void build();
  void print();
  void computeStatistics();
  void collectNodes(Region &region);

  /// Optimizations
  void fuseAdjacentNodes();
  void detectOutOnlyNodes();
  void deduplicateNodes();

  /// Analyzes the edges of the graph and returns true if the node only depends
  /// on the entry node.
  bool isOnlyDependentOnEntry(DatablockNode &node);

private:
  /// Attributes
  func::FuncOp func;
  DatablockAnalysis *DA;
  SmallVector<DatablockNode *, 4> nodes;
  DenseMap<unsigned, SetVector<unsigned>> edges;

  /// Map from each arts.datablock op to its corresponding Node.
  Environment nodeMap;

  /// Functions
  /// Process a structured region, updates the DDG and returns the new
  /// environment and whether the environment has changed.
  std::pair<Environment, bool> processRegion(Region &region, Environment &env);
  std::pair<Environment, bool> processEdt(arts::EdtOp edtOp, Environment &env);
  std::pair<Environment, bool> processFor(scf::ForOp forOp, Environment &env);
  std::pair<Environment, bool> processIf(scf::IfOp ifOp, Environment &env);
  std::pair<Environment, bool> processCall(func::CallOp callOp,
                                           Environment &env);

  /// Find datablock that define the dbNode in the given environment.
  SmallVector<DatablockNode *, 4> findDefinition(DatablockNode &dbNode,
                                                 Environment &env);

  /// Merge two environments by taking the union of definitions for each
  /// datablock.
  Environment mergeEnvironments(const Environment &env1,
                                const Environment &env2);

  /// Entry node
  DatablockNode *entryDbNode;
};

//===----------------------------------------------------------------------===//
// DatablockAnalysis
// This is a pass that performs a comprehensive analysis on arts.datablock
// operations.It builds a dependency graph for each function in the module
// and provides methods to query the graph.
//===----------------------------------------------------------------------===//
class DatablockAnalysis {

public:
  explicit DatablockAnalysis(Operation *module);
  ~DatablockAnalysis();
  friend class DatablockGraph;
  friend class DatablockNode;

  /// Get the graph for a given function, or create it if it does not exist.
  DatablockGraph *getOrCreateGraph(func::FuncOp func);
  bool isInvalidated(const AnalysisManager::PreservedAnalyses &pa) {
    printf("DatablockAnalysis::isInvalidated\n");
    return !pa.isPreserved<arts::DatablockAnalysis>();
  }

private:
  /// Dependency analysis.
  bool mayDepend(DatablockNode &prod, DatablockNode &cons, bool &isDirect);
  bool ptrMayAlias(DatablockNode &A, DatablockNode &B);
  bool ptrMayAlias(DatablockNode &A, Value val);
  DatablockNodeComp compare(DatablockNode &A, DatablockNode &B);

  /// Other
  void printGraph(func::FuncOp func);

  /// Map from function to its dependency graph.
  DenseMap<func::FuncOp, DatablockGraph *> functionGraphMap;

  /// Loop analysis.
  LoopAnalysis *loopAnalysis;
};
} // namespace arts
} // namespace mlir

#endif // CARTS_ANALYSIS_DATABLOCKANALYSIS_H