///==========================================================================
/// File: DataBlockAnalysis.h
///
/// This pass performs a comprehensive analysis on arts.datablock operations.
///==========================================================================

#ifndef MLIR_ANALYSIS_DATABLOCKANALYSIS_H
#define MLIR_ANALYSIS_DATABLOCKANALYSIS_H

#include "arts/ArtsDialect.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/AffineMap.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/Dominance.h"
#include "mlir/IR/Types.h"
#include "mlir/Support/LLVM.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include <cstdint>
#include <sys/types.h>
#include <utility>

namespace mlir {
namespace arts {
class DatablockAnalysis;
class DatablockNode;
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
  DatablockNode(DatablockAnalysis *DA) : DA(DA) {}

  /// Interface
  bool isWriter() { return mode == "out" || mode == "inout"; }
  bool isReader() { return mode == "in" || mode == "inout"; }

  /// Uses
  void collectUses();

  /// Attributes
  unsigned id = 0;
  arts::DataBlockOp op;

  /// Db attributtes
  StringRef mode;
  Value ptr;
  SmallVector<Value, 4> indices, sizes;
  Type elementType;
  uint64_t elementTypeSize;
  MemRefType resultType;
  AffineMap affineMap;
  bool hasPtrDb, isSingle;

  /// Analysis results
  bool isLoopDependent = false;
  SmallVector<MemoryEffects::EffectInstance, 2> effects;

  /// Uses
  unsigned useCount;
  DatablockNode *parent = nullptr;
  arts::EdtOp edtParent = nullptr;
  arts::EdtOp userEdt = nullptr;
  uint32_t userEdtPos = 0;

  /// A duplicated node is a node with the same mode, aliasing base memref,
  /// and indices.
  SetVector<unsigned> duplicates;
  /// Set of nodes that alias this node.
  SetVector<unsigned> aliases;

private:
  DatablockAnalysis *DA;
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
  using Edge = std::pair<unsigned, unsigned>; // producerID, consumerID

  /// Interface
  func::FuncOp getFunction() { return func; }
  DatablockNode *getNode(arts::DataBlockOp dbOp) { return nodeMap[dbOp]; }
  DatablockNode *getNode(unsigned id) { return nodes[id]; }
  bool isEntryNode(unsigned id) const { return id == entryDbNode.id; }
  bool hasNodes() const { return !nodes.empty(); }
  SetVector<unsigned> getProducers() const;
  llvm::SetVector<unsigned> getConsumers(unsigned producerID);

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

private:
  /// Attributes
  func::FuncOp func;
  DatablockAnalysis *DA;
  SmallVector<DatablockNode *, 4> nodes;
  llvm::DenseMap<unsigned, llvm::SetVector<unsigned>> edges;

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
  DatablockNode entryDbNode;
};

//===----------------------------------------------------------------------===//
// DatablockAnalysis
//===----------------------------------------------------------------------===//
class DatablockAnalysis {
public:
  explicit DatablockAnalysis(Operation *module);
  ~DatablockAnalysis();

  friend class DatablockGraph;

  /// Get the graph for a given function, or create it if it does not exist.
  DatablockGraph *getOrCreateGraph(func::FuncOp func);

private:
  /// Dependency analysis.
  bool mayDepend(DatablockNode &prod, DatablockNode &cons, bool &isDirect);
  bool ptrMayAlias(DatablockNode &A, DatablockNode &B);
  bool ptrMayAlias(DatablockNode &A, Value val);
  DatablockNodeComp compare(DatablockNode &A, DatablockNode &B);

  /// Analysis the loops of the region and collect the values that depend on
  /// them
  void analyzeLoops(Region &region,
                    DenseMap<Value, SmallVector<Operation *, 4>> &valMap);

  /// Other
  void printGraph(func::FuncOp func);

  /// Returns true if `target` is reachable from `source` in the EDT CFG.
  bool isReachable(Operation *source, Operation *target);

  /// Set of equivalent datablock nodes.
  llvm::SmallDenseSet<unsigned> equivalentNodes;

  /// Map from function to its dependency graph.
  llvm::DenseMap<func::FuncOp, DatablockGraph *> functionGraphMap;
};

} // namespace arts
} // namespace mlir

#endif // MLIR_ANALYSIS_DATABLOCKANALYSIS_H