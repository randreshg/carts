///==========================================================================
/// File: ArtsDatablockPass.cpp
///
/// This pass performs a comprehensive analysis on arts.datablock operations.
///
/// It:
///   1. Collects arts.datablock ops top-level region of a function.
///   2. Parses each datablock’s (offsets, sizes, strides) to compute numeric
///      bounding information dimension by dimension. extracts constant values
///      when possible and recursively folds simple add/mul chains.
///   3. Classifies each datablock as memory read or write based on its "mode"
///      attribute:
///         "in"   → read-only,
///         "out"  → write-only,
///         "inout"→ mixed read/write.
///       (The datablock op implements MemoryEffectOpInterface so that polygeist
///       alias analysis sees its effects.)
///   4. Runs a forward BFS from every scf.for induction variable (IV) to mark
///      datablocks whose offsets depend on a loop IV as crossIteration.
///   6. Builds a dependency graph (read-after-write edges) based on numeric
///      bounding, dominance (skipped if crossIteration is true), and
///      alias information.
///   7. Collects usage statistics for each datablock: total use count and the
///      set of parent regions in which it is used, average use count and
///      average region frequency.
///   8. Deduplicates datablock nodes by comparing their mode, base memref, and
///      numeric bounding
///   9. Detects “out-only” datablocks (writer nodes with no consumers) and
///      reports them as removable.
///  10. Additionally, it detects if a datablock is used only for reads versus
///      mixed read/write, and reports candidates for fusion (if used only in
///      one region) or elimination (if read-only and used rarely).
///==========================================================================

/// Dialects
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "polygeist/Ops.h"
/// Arts
#include "ArtsPassDetails.h"
#include "arts/ArtsDialect.h"
#include "arts/Passes/ArtsPasses.h"
/// Other
#include "mlir/IR/Dominance.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/SmallVector.h"
/// Debug
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#define DEBUG_TYPE "convert-arts-to-funcs"
#define line "-----------------------------------------\n"
#define dbgs() (llvm::dbgs())
#define DBGS() (dbgs() << "[" DEBUG_TYPE "] ")
/// Others
#include <optional>

using namespace mlir;
using namespace mlir::func;
using namespace mlir::arith;
using namespace mlir::polygeist;

namespace {
///==========================================================================
/// DatablockAnalysis Class Declaration
///==========================================================================

class DatablockAnalysis {
public:
  /// Structure to hold numeric bounding information for a multi-dimensional
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
    std::string mode;
    Value baseMemref;
    SubviewInfo bound;
    bool crossIteration = false;
    SmallVector<Operation *, 4> topUsers;
    unsigned useCount = 0;
    llvm::SmallDenseSet<Region *> userRegions;
    SmallVector<MemoryEffects::EffectInstance, 2> effects;
  };

  /// Edge from a producer (writer) node to a consumer (reader) node.
  struct Edge {
    unsigned producerID;
    unsigned consumerID;
  };

  /// The complete dependency graph.
  struct Graph {
    SmallVector<Node, 8> nodes;
    SmallVector<Edge, 8> edges;
  };

  /// OffsetMap maps an offset Value to the list of node IDs that use it.
  struct OffsetMap {
    llvm::DenseMap<Value, SmallVector<unsigned, 4>> offsetUsers;
  };

  /// Analyze the given region using domInfo and Polygeist alias analysis AA.
  /// Returns the dependency graph.
  static Graph analyze(Region &region, DominanceInfo &domInfo);

  /// Print the dependency graph.
  static void printGraph(const Graph &graph);

  /// Compute and print additional statistics: average use count and average
  /// region frequency.
  static void computeStatistics(const Graph &graph);

public:
  static bool isWriter(const std::string &mode) {
    return mode == "out" || mode == "inout";
  }

  static bool isReader(const std::string &mode) {
    return mode == "in" || mode == "inout";
  }
  static std::optional<int64_t> computeConstant(Value val);

  /// Try to extract an integer constant from a Value
  static int64_t tryParseIndexConstant(Value val);

  /// Parse the subview info from a datablock op.
  static SubviewInfo parseSubviewInfo(arts::DataBlockOp dbOp);

  /// Collect all arts.datablock ops in the region into the Graph, and build
  /// the OffsetMap.
  static void collectNodes(Region &region, Graph &graph, OffsetMap &oMap);

  /// Run BFS from each scf.for induction variable to mark nodes as
  /// crossIteration.
  static void runBFS(Region &region, OffsetMap &oMap, Graph &graph);

  /// Check if two nodes numeric bounds overlap.
  static bool boundingOverlap(Node &A, Node &B);

  /// Check dependency: if either node is crossIteration, skip dominance;
  /// otherwise, require that prod dominates cons.
  static bool checkDependency(Node &prod, Node &cons, DominanceInfo &domInfo);

  /// Build dependency (adjacency) edges in the graph.
  static void buildAdjacency(Graph &graph, DominanceInfo &domInfo);

  /// Deduplicate nodes by comparing their values (mode, base memref, and
  /// numeric bounding).
  static void deduplicateNodes(Graph &graph);

  /// Check if two nodes are equivalent.
  static bool areNodesEquivalent(Node &A, Node &B);

  /// Run BFS from a given loop induction variable.
  static void BFSForward(Value iv, DenseSet<Value> &visited, OffsetMap &oMap,
                         Graph &graph);

  /// OPTIMIZATIONS
  /// Fuse adjacent nodes (if their subviews are contiguous along one dimension)
  /// to improve spatial locality. Two nodes are fusible if they have the same
  /// base, same mode, same number of dimensions, and there exists at least one
  /// dimension d such that:
  ///   A.bound.offset[d] + A.bound.sizes[d] == B.bound.offset[d]
  /// and for all other dimensions, their offsets and sizes are equal.
  /// The fused node will have its size in that dimension updated to the sum.
  static void fuseAdjacentNodes(Graph &graph);

  /// Detect out-only datablocks (writer nodes with no consumers).
  static void detectOutOnlyNodes(Graph &graph);
};

///==========================================================================
/// DatablockAnalysis Class Implementation
///==========================================================================
std::optional<int64_t> DatablockAnalysis::computeConstant(Value val) {
  if (auto cst = val.getDefiningOp<arith::ConstantIndexOp>())
    return cst.value();
  if (auto addOp = dyn_cast_or_null<arith::AddIOp>(val.getDefiningOp())) {
    auto lhs = computeConstant(addOp.getOperand(0));
    auto rhs = computeConstant(addOp.getOperand(1));
    if (lhs && rhs)
      return *lhs + *rhs;
  }
  if (auto mulOp = dyn_cast_or_null<arith::MulIOp>(val.getDefiningOp())) {
    auto lhs = computeConstant(mulOp.getOperand(0));
    auto rhs = computeConstant(mulOp.getOperand(1));
    if (lhs && rhs)
      return *lhs * *rhs;
  }
  return std::nullopt;
}

int64_t DatablockAnalysis::tryParseIndexConstant(Value val) {
  if (auto c = computeConstant(val))
    return *c;
  ValueOrInt voi(val);
  if (!voi.isValue)
    return voi.i_val;
  return -1;
}

DatablockAnalysis::SubviewInfo
DatablockAnalysis::parseSubviewInfo(arts::DataBlockOp dbOp) {
  SubviewInfo sb;
  auto offVals = dbOp.getOffsets();
  auto sizVals = dbOp.getSizes();
  auto strVals = dbOp.getStrides();
  if (offVals.size() != sizVals.size() || sizVals.size() != strVals.size())
    return sb;
  unsigned rank = offVals.size();
  sb.offsets.resize(rank);
  sb.sizes.resize(rank);
  sb.strides.resize(rank);
  for (unsigned i = 0; i < rank; i++) {
    int64_t o = tryParseIndexConstant(offVals[i]);
    int64_t s = tryParseIndexConstant(sizVals[i]);
    int64_t st = tryParseIndexConstant(strVals[i]);
    /// Dynamic dimension → invalid bound.
    if (o < 0 || s < 0 || st < 0)
      return sb;
    sb.offsets[i] = o;
    sb.sizes[i] = s;
    sb.strides[i] = st;
  }
  sb.valid = true;
  return sb;
}

void DatablockAnalysis::collectNodes(Region &region, Graph &graph,
                                     OffsetMap &oMap) {
  unsigned nextID = 0;
  for (Block &blk : region) {
    for (Operation &op : blk) {
      if (auto dbOp = dyn_cast<arts::DataBlockOp>(op)) {
        Node node;
        node.id = nextID++;
        node.op = dbOp;
        if (auto modeAttr = dbOp->getAttrOfType<StringAttr>("mode"))
          node.mode = modeAttr.str();
        else
          node.mode = "unknown";
        node.baseMemref = dbOp.getBase();
        node.bound = parseSubviewInfo(dbOp);
        /// Collect memory effects.
        if (auto memEff = dyn_cast<MemoryEffectOpInterface>(dbOp))
          memEff.getEffects(node.effects);
        /// Gather top-level users (e.g., arts.edt, arts.parallel) in the same
        /// block.
        auto numUses = 0;
        for (auto &use : dbOp->getUses()) {
          Operation *userOp = use.getOwner();
          if (userOp->getBlock() == &blk) {
            numUses++;
            if (isa<arts::EdtOp>(userOp) || isa<arts::ParallelOp>(userOp))
              node.topUsers.push_back(userOp);
          }
        }
        node.useCount = numUses;
        for (auto &use : dbOp->getUses()) {
          if (auto *r = use.getOwner()->getParentRegion())
            node.userRegions.insert(r);
        }
        graph.nodes.push_back(node);
        unsigned idx = graph.nodes.size() - 1;
        for (Value offVal : dbOp.getOffsets())
          oMap.offsetUsers[offVal].push_back(idx);
      }
    }
  }
}

void DatablockAnalysis::BFSForward(Value iv, DenseSet<Value> &visited,
                                   OffsetMap &oMap, Graph &graph) {
  SmallVector<Value, 8> queue;
  visited.insert(iv);
  queue.push_back(iv);
  while (!queue.empty()) {
    Value cur = queue.pop_back_val();
    if (auto it = oMap.offsetUsers.find(cur); it != oMap.offsetUsers.end()) {
      for (unsigned nodeID : it->second)
        graph.nodes[nodeID].crossIteration = true;
    }
    for (auto &use : cur.getUses()) {
      Operation *owner = use.getOwner();
      for (Value res : owner->getResults()) {
        if (visited.insert(res).second)
          queue.push_back(res);
      }
    }
  }
}

void DatablockAnalysis::runBFS(Region &region, OffsetMap &oMap, Graph &graph) {
  SmallVector<scf::ForOp, 8> loops;
  region.walk([&](scf::ForOp fop) { loops.push_back(fop); });
  for (auto fop : loops) {
    Value iv = fop.getInductionVar();
    DenseSet<Value> visited;
    BFSForward(iv, visited, oMap, graph);
  }
}

bool DatablockAnalysis::boundingOverlap(Node &A, Node &B) {
  if (!A.bound.valid || !B.bound.valid)
    return true;
  if (A.bound.offsets.size() != B.bound.offsets.size())
    return true;
  for (unsigned d = 0; d < A.bound.offsets.size(); d++) {
    int64_t Astart = A.bound.offsets[d];
    int64_t Aend = Astart + A.bound.sizes[d];
    int64_t Bstart = B.bound.offsets[d];
    int64_t Bend = Bstart + B.bound.sizes[d];
    if (Aend <= Bstart || Bend <= Astart)
      return false;
  }
  return true;
}

bool DatablockAnalysis::checkDependency(Node &prod, Node &cons,
                                        DominanceInfo &domInfo) {
  if (prod.crossIteration || cons.crossIteration)
    return true;
  return domInfo.dominates(prod.op.getOperation(), cons.op.getOperation());
}

void DatablockAnalysis::buildAdjacency(Graph &graph, DominanceInfo &domInfo) {
  for (unsigned i = 0; i < graph.nodes.size(); i++) {
    for (unsigned j = 0; j < graph.nodes.size(); j++) {
      if (i == j)
        continue;
      Node &A = graph.nodes[i];
      Node &B = graph.nodes[j];
      if (!isWriter(A.mode) || !isReader(B.mode))
        continue;
      /// First check base memrefs.
      if (A.baseMemref != B.baseMemref)
        continue;
      /// Then check memory effects.
      bool aliasEffects = false;
      for (auto &eA : A.effects) {
        for (auto &eB : B.effects) {
          if (mayAlias(eA, eB)) {
            aliasEffects = true;
            break;
          }
        }
        if (aliasEffects)
          break;
      }
      if (!aliasEffects)
        continue;
      /// Finally, check numeric bounding.
      if (!boundingOverlap(A, B))
        continue;
      if (checkDependency(A, B, domInfo))
        graph.edges.push_back({A.id, B.id});
    }
  }
}

bool DatablockAnalysis::areNodesEquivalent(Node &A, Node &B) {
  if (A.mode != B.mode)
    return false;
  if (A.baseMemref != B.baseMemref)
    return false;
  if (A.bound.valid != B.bound.valid)
    return false;
  if (A.bound.valid) {
    if (A.bound.offsets.size() != B.bound.offsets.size())
      return false;
    for (unsigned i = 0; i < A.bound.offsets.size(); ++i) {
      if (A.bound.offsets[i] != B.bound.offsets[i] ||
          A.bound.sizes[i] != B.bound.sizes[i] ||
          A.bound.strides[i] != B.bound.strides[i])
        return false;
    }
  }
  return true;
}


void DatablockAnalysis::computeStatistics(const Graph &graph) {
  if (graph.nodes.empty())
    return;
  unsigned totalUses = 0;
  unsigned totalRegions = 0;
  for (const auto &node : graph.nodes) {
    totalUses += node.useCount;
    totalRegions += node.userRegions.size();
  }
  double avgUses = (double)totalUses / graph.nodes.size();
  double avgRegions = (double)totalRegions / graph.nodes.size();
  LLVM_DEBUG(DBGS() << "Average use count per datablock: " << avgUses << "\n"
                    << "Average region frequency per datablock: " << avgRegions
                    << "\n");
}

void DatablockAnalysis::printGraph(const Graph &graph) {
  std::string info;
  llvm::raw_string_ostream os(info);
  os << "Nodes:\n";
  for (const auto &n : graph.nodes) {
    os << "  #" << n.id << " " << n.mode << " base=" << n.baseMemref;
    if (n.bound.valid) {
      os << " bound=[";
      for (unsigned d = 0; d < n.bound.offsets.size(); d++) {
        os << n.bound.offsets[d] << ".."
           << (n.bound.offsets[d] + n.bound.sizes[d]);
        if (d + 1 < n.bound.offsets.size())
          os << ", ";
      }
      os << "]";
    } else {
      os << " bound=(unknown)";
    }
    os << " crossIteration=" << (n.crossIteration ? "true" : "false");
    os << " useCount=" << n.useCount;
    os << " usedInRegions={";
    for (Region *r : n.userRegions)
      os << r << " ";
    os << "}\n";
  }
  os << "Edges:\n";
  for (const auto &e : graph.edges)
    os << "  #" << e.producerID << " -> #" << e.consumerID << "\n";
  os << "Total nodes: " << graph.nodes.size() << "\n";
  LLVM_DEBUG(DBGS() << os.str());
}

void DatablockAnalysis::fuseAdjacentNodes(Graph &graph) {
  llvm::SmallDenseSet<unsigned> fusedIDs;
  for (unsigned i = 0; i < graph.nodes.size(); i++) {
    for (unsigned j = i + 1; j < graph.nodes.size(); j++) {
      if (fusedIDs.contains(j))
        continue;
      const auto &A = graph.nodes[i];
      const auto &B = graph.nodes[j];
      if (A.mode != B.mode)
        continue;
      if (A.baseMemref != B.baseMemref)
        continue;
      if (!A.bound.valid || !B.bound.valid)
        continue;
      if (A.bound.offsets.size() != B.bound.offsets.size())
        continue;
      bool fusible = false;
      unsigned fuseDim = 0;
      unsigned rank = A.bound.offsets.size();
      for (unsigned d = 0; d < rank; d++) {
        // Check if dimension d is adjacent.
        if (A.bound.offsets[d] + A.bound.sizes[d] == B.bound.offsets[d]) {
          bool equalOtherDims = true;
          for (unsigned k = 0; k < rank; k++) {
            if (k == d)
              continue;
            if (A.bound.offsets[k] != B.bound.offsets[k] ||
                A.bound.sizes[k] != B.bound.sizes[k]) {
              equalOtherDims = false;
              break;
            }
          }
          if (equalOtherDims) {
            fusible = true;
            fuseDim = d;
            break;
          }
        }
      }
      if (fusible) {
        // Fuse node B into node A.
        auto &A_mut = graph.nodes[i];
        A_mut.bound.sizes[fuseDim] += B.bound.sizes[fuseDim];
        A_mut.useCount += B.useCount;
        for (Region *r : B.userRegions)
          A_mut.userRegions.insert(r);
        for (Operation *op : B.topUsers)
          A_mut.topUsers.push_back(op);
        fusedIDs.insert(j);
        LLVM_DEBUG(DBGS() << "Fused node #" << j << " into node #" << i
                          << " along dimension " << fuseDim << "\n");
      }
    }
  }
  llvm::SmallVector<Node, 8> newNodes;
  for (unsigned i = 0; i < graph.nodes.size(); i++) {
    if (!fusedIDs.contains(i))
      newNodes.push_back(graph.nodes[i]);
  }
  graph.nodes = newNodes;
}

void DatablockAnalysis::deduplicateNodes(Graph &graph) {
  llvm::SmallDenseSet<unsigned> duplicateIDs;
  for (unsigned i = 0; i < graph.nodes.size(); i++) {
    for (unsigned j = i + 1; j < graph.nodes.size(); j++) {
      if (areNodesEquivalent(graph.nodes[i], graph.nodes[j])) {
        duplicateIDs.insert(j);
        // graph.nodes[j].op->replaceAllUsesWith(graph.nodes[i].op.getResult());
        graph.nodes[j].op.erase();
        LLVM_DEBUG(DBGS() << "Deduplicated datablock node #" << j
                          << " (duplicate of #" << i << ")\n");
      }
    }
  }
  SmallVector<Node, 8> uniqueNodes;
  for (unsigned i = 0; i < graph.nodes.size(); i++) {
    if (!duplicateIDs.contains(i))
      uniqueNodes.push_back(graph.nodes[i]);
  }
  graph.nodes = uniqueNodes;
}

void DatablockAnalysis::detectOutOnlyNodes(Graph &graph) {
  llvm::SmallDenseSet<unsigned> outOnlyIDs;
  for (unsigned i = 0; i < graph.nodes.size(); i++) {
    const auto &node = graph.nodes[i];
    if (isWriter(node.mode) && node.topUsers.empty()) {
      outOnlyIDs.insert(i);
      LLVM_DEBUG(DBGS() << "No consumer from #" << i
                        << " => out-only => removable\n");
    }
  }
  llvm::SmallVector<Node, 8> newNodes;
  for (unsigned i = 0; i < graph.nodes.size(); i++) {
    if (!outOnlyIDs.contains(i))
      newNodes.push_back(graph.nodes[i]);
  }
  graph.nodes = newNodes;
}

DatablockAnalysis::Graph DatablockAnalysis::analyze(Region &region,
                                                    DominanceInfo &domInfo) {
  Graph graph;
  OffsetMap oMap;
  collectNodes(region, graph, oMap);
  runBFS(region, oMap, graph);
  buildAdjacency(graph, domInfo);
  deduplicateNodes(graph);
  return graph;
}

///==========================================================================
/// ArtsDatablockPass Class Implementation
///==========================================================================
struct ArtsDatablockPass
    : public arts::ArtsDatablockPassBase<ArtsDatablockPass> {

  void runOnOperation() override {
    ModuleOp module = getOperation();
    MLIRContext *ctx = &getContext();

    for (auto func : module.getOps<func::FuncOp>()) {
      DominanceInfo domInfo(func);
      auto graph = DatablockAnalysis::analyze(func.getBody(), domInfo);
      DatablockAnalysis::printGraph(graph);
      DatablockAnalysis::computeStatistics(graph);

      /// Detect out-only nodes.
      DatablockAnalysis::detectOutOnlyNodes(graph);

      /// Additionally, report usage pattern:
      /// - Detect if a datablock is used only for reads versus mixed
      /// read/write.
      /// - Report if a datablock is used only in a single region (candidate
      /// for fusion).
      for (const auto &node : graph.nodes) {
        if (node.mode == "in")
          LLVM_DEBUG(DBGS() << "Node #" << node.id << " is read-only.\n");
        else if (node.mode == "out")
          LLVM_DEBUG(DBGS() << "Node #" << node.id << " is write-only.\n");
        else if (node.mode == "inout")
          LLVM_DEBUG(DBGS()
                     << "Node #" << node.id << " is mixed read/write.\n");

        if (node.userRegions.size() == 1)
          LLVM_DEBUG(DBGS() << "Datablock node #" << node.id
                            << " is used only in region "
                            << *(node.userRegions.begin())
                            << " (candidate for fusion).\n");
        else if (!node.userRegions.empty())
          LLVM_DEBUG(DBGS() << "Datablock node #" << node.id << " is used in "
                            << node.userRegions.size()
                            << " regions (cannot fuse easily).\n");
      }
    }
  }
};

} // end anonymous namespace

///==========================================================================
/// Pass creation.
///==========================================================================
namespace mlir {
namespace arts {
std::unique_ptr<Pass> createArtsDatablockPass() {
  return std::make_unique<ArtsDatablockPass>();
}
} // namespace arts
} // namespace mlir