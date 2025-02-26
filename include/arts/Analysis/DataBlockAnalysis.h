///==========================================================================
/// File: DataBlockAnalysis.h
///
/// This pass performs a comprehensive analysis on arts.datablock operations.
///
/// It:
///   1. Collects arts.datablock ops top-level region of a function.
///   2. Classifies each datablock as memory read or write based on its "mode"
///      attribute:
///         "in"   → read-only,
///         "out"  → write-only,
///         "inout"→ mixed read/write.
///       (The datablock op implements MemoryEffectOpInterface so that polygeist
///       alias analysis sees its effects.)
///   4. Runs a forward BFS from every scf.for induction variable (IV) to mark
///      datablocks whose indices depend on a loop IV as isLoopDependent.
///   5. Builds a dependency graph
///   6. Collects usage statistics for each datablock: total use count and the
///      set of parent regions in which it is used, average use count and
///      average region frequency.
///
/// Optimizations:
///   1. Deduplicates datablock nodes by comparing their mode, base ptr, and
///      numeric infoing
///   2. Detects “out-only” datablocks (writer nodes with no consumers) and
///      reports them as removable.
///   3. Additionally, it detects if a datablock is used only for reads versus
///      mixed read/write, and reports candidates for fusion (if used only in
///      one region) or elimination (if read-only and used rarely).
///==========================================================================

#ifndef MLIR_ANALYSIS_DATABLOCKANALYSIS_H
#define MLIR_ANALYSIS_DATABLOCKANALYSIS_H

#include "arts/ArtsDialect.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/IR/AffineMap.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/Dominance.h"
#include "mlir/IR/Types.h"
#include "mlir/Support/LLVM.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include <cstdint>
#include <sys/types.h>

namespace mlir {
namespace arts {

///==========================================================================
/// DatablockAnalysis Class Declaration
///==========================================================================
class DatablockAnalysis {
public:
  explicit DatablockAnalysis(Operation *module);

  /// Node representing one arts.datablock op in the dependency graph.
  struct Node {
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
    bool isPtrDb, isSingle;

    /// Analysis results
    bool isLoopDependent = false;
    SmallVector<MemoryEffects::EffectInstance, 2> effects;

    /// Uses
    unsigned useCount;
    Node *parent = nullptr;
    arts::EdtOp edtParent = nullptr;
    arts::EdtOp userEdt = nullptr;
    uint32_t userEdtPos = 0;

    /// A duplicated node is a node with the same mode, aliasing base memref,
    /// and indices.
    SetVector<unsigned> duplicates;
    /// Set of nodes that alias this node.
    SetVector<unsigned> aliases;
  };

  enum NodeComp { Equal, BaseAlias, Different };

  /// Edge from a producer (writer) node to a consumer (reader) node.
  struct Edge {
    unsigned producerID;
    unsigned consumerID;
    bool isDirect = true;
    bool isLoopDependent = false;
  };

  /// The complete dependency graph.
  struct Graph {
    SmallVector<Node, 8> nodes;
    SmallVector<Edge, 8> edges;
  };

  /// Run the analysis on a single function (its top-level region) and store the
  /// graph.
  Graph analyzeFunction(func::FuncOp func);
  /// Print the graph for a given function.
  void printGraph(func::FuncOp func);
  /// Compute and print additional statistics for a given function's graph.
  void computeStatistics(func::FuncOp func);
  /// Get the graph for a given function, or create it if it does not exist.
  Graph &getOrCreateGraph(func::FuncOp func);
  /// Get db node
  Node &getNode(arts::DataBlockOp dbOp) { return nodeMap[dbOp]; }

private:
  /// Dependency analysis.
  bool mayDepend(Node &prod, Node &cons, bool &isDominated,
                 bool &isLoopDependent, DominanceInfo &domInfo);
  bool ptrMayAlias(Node &A, Node &B);
  bool ptrMayAlias(Node &A, Value val);
  NodeComp compare(Node &A, Node &B);
  void buildAdjacency(Graph &graph, DominanceInfo &domInfo);

  /// Analysis the loops of the region and collect the values that depend on
  /// them
  void analyzeLoops(Region &region,
                    DenseMap<Value, SmallVector<Operation *, 4>> &valMap);

  /// Node collection
  void collectNodes(Region &region, Graph &graph);

  /// Uses
  void collectUses(Node &node);

  /// Check if an load/store operation uses a datablock
  // bool isLoadStoreUse(Operation *op, Node &node);

  /// Statistics
  void computeStatistics(Graph &graph);

  /// Other
  void printGraph(Graph &graph);

  /// Helper functions for mode classification.
  bool isWriter(Node &node) {
    return node.mode == "out" || node.mode == "inout";
  }

  bool isReader(Node &node) {
    return node.mode == "in" || node.mode == "inout";
  }

  /// Map from function to its dependency graph.
  llvm::DenseMap<func::FuncOp, Graph> functionGraphMap;
  /// Map from each arts.datablock op to its corresponding Node.
  llvm::DenseMap<arts::DataBlockOp, Node> nodeMap;
  /// Set of equivalent datablock nodes.
  llvm::SmallDenseSet<unsigned> equivalentNodes;

public:
  /// Optimizations
  void fuseAdjacentNodes(Graph &graph);
  void detectOutOnlyNodes(Graph &graph);
  void deduplicateNodes(Graph &graph);

  /// Results
  // DenseMap<arts::DataBlockOp, Node> replaceMap;
  // SmallVector<Node, 4> toRewire;
  // SmallVector<arts::DataBlockOp> unusedDbs;
};

} // namespace arts
} // namespace mlir

#endif // MLIR_ANALYSIS_DATABLOCKANALYSIS_H