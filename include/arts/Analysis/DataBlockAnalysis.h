///==========================================================================
/// File: DataBlockAnalysis.h
///
/// This pass performs a comprehensive analysis on arts.datablock operations.
///
/// It:
///   1. Collects arts.datablock ops top-level region of a function.
///   2. Parses each datablock’s (offsets, sizes, strides) to compute
///      compile-time information dimension by dimension. It then extracts
///      constant values when possible and recursively folds simple add/mul
///      chains.
///   3. Classifies each datablock as memory read or write based on its "mode"
///      attribute:
///         "in"   → read-only,
///         "out"  → write-only,
///         "inout"→ mixed read/write.
///       (The datablock op implements MemoryEffectOpInterface so that polygeist
///       alias analysis sees its effects.)
///   4. Runs a forward BFS from every scf.for induction variable (IV) to mark
///      datablocks whose offsets depend on a loop IV as isLoopDependent.
///   6. Builds a dependency graph (read-after-write edges) based on numeric
///      i, dominance (skipped if isLoopDependent is true), and
///      alias information.
///   7. Collects usage statistics for each datablock: total use count and the
///      set of parent regions in which it is used, average use count and
///      average region frequency.
///   8. Deduplicates datablock nodes by comparing their mode, base memref, and
///      numeric infoing
///   9. Detects “out-only” datablocks (writer nodes with no consumers) and
///      reports them as removable.
///  10. Additionally, it detects if a datablock is used only for reads versus
///      mixed read/write, and reports candidates for fusion (if used only in
///      one region) or elimination (if read-only and used rarely).
///==========================================================================

#ifndef MLIR_ANALYSIS_DATABLOCKANALYSIS_H
#define MLIR_ANALYSIS_DATABLOCKANALYSIS_H

#include "arts/ArtsDialect.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/IR/Dominance.h"
#include "mlir/Support/LLVM.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/SmallVector.h"

namespace mlir {
namespace arts {

///==========================================================================
/// DatablockAnalysis Class Declaration
///==========================================================================
class DatablockAnalysis {
public:
  explicit DatablockAnalysis(Operation *module);

  /// Structure to hold numeric infoing information for a multi-dimensional
  /// subview.
  struct SubviewInfo {
    bool valid = false;
    SmallVector<int64_t, 4> offsets;
    SmallVector<int64_t, 4> sizes;
    SmallVector<int64_t, 4> strides;
  };

  /// Node representing one arts.datablock op in the dependency graph.
  struct Node {
    unsigned id = 0;
    arts::DataBlockOp op;
    /// Db attributtes
    std::string mode;
    Value baseMemref;
    SmallVector<Value, 4> offsets;
    SmallVector<Value, 4> sizes;
    SmallVector<Value, 4> strides;
    bool isLoad;
    bool baseIsDb;
    /// Analysis results
    SubviewInfo info;
    unsigned useCount;
    bool isLoopDependent = false;
    arts::EdtOp edtUser = nullptr;
    arts::EdtOp edtParent = nullptr;
    llvm::SmallDenseSet<Region *> userRegions;
    SmallVector<MemoryEffects::EffectInstance, 2> effects;
    /// A duplicated node is a node with the same mode, aliasing base memref,
    /// and offsets.
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
  /// Get the for op that uses a given offset.
  SetVector<scf::ForOp> getLoopsFromOffset(Value dbOffset);
  /// Get db node
  Node &getNode(arts::DataBlockOp dbOp) { return nodeMap[dbOp]; }

private:
  /// Dependency analysis.
  bool mayOverlap(Node &A, Node &B);
  bool mayDepend(Node &prod, Node &cons, bool &isDominated,
                 bool &isLoopDependent, DominanceInfo &domInfo);
  bool baseMayAlias(Node &A, Node &B);
  NodeComp compare(Node &A, Node &B);
  void buildAdjacency(Graph &graph, DominanceInfo &domInfo);

  /// Analysis
  void analyzeLoops(Region &region,
                    DenseMap<Value, llvm::SmallVector<scf::ForOp, 4>> &valMap);

  /// Node collection
  void collectNodes(Region &region, Graph &graph);
  void setSubviewInfo(Node &node);

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
  /// Map from each datablock offset Value to the set of loops it depends on
  llvm::DenseMap<Value, SetVector<scf::ForOp>> offsetMap;
  /// Set of equivalent datablock nodes.
  llvm::SmallDenseSet<unsigned> equivalentNodes;

public:
  /// Optimizations
  void fuseAdjacentNodes(Graph &graph);
  void detectOutOnlyNodes(Graph &graph);
  void deduplicateNodes(Graph &graph);
};

} // namespace arts
} // namespace mlir

#endif // MLIR_ANALYSIS_DATABLOCKANALYSIS_H