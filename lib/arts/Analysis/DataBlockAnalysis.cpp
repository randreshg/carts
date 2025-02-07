///==========================================================================
/// File: DataBlockAnalysis.cpp
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

#include "arts/Analysis/DataBlockAnalysis.h"
/// Dialects
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/Operation.h"
#include "mlir/IR/Value.h"
#include "mlir/Support/LLVM.h"
#include "polygeist/Ops.h"
/// Arts
#include "arts/ArtsDialect.h"
#include "arts/Passes/ArtsPasses.h"
#include "arts/Utils/ArtsUtils.h"
/// Other
#include "mlir/IR/Dominance.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/SmallVector.h"
/// Debug
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#define DEBUG_TYPE "datablock-analysis"
#define line "-----------------------------------------\n"
#define dbgs() (llvm::dbgs())
#define DBGS() (dbgs() << "[" DEBUG_TYPE "] ")

using namespace mlir;
using namespace mlir::func;
using namespace mlir::arith;
using namespace mlir::polygeist;

namespace mlir {
namespace arts {

/// Constructor
DatablockAnalysis::DatablockAnalysis(Operation *module) {
  ModuleOp mod = cast<ModuleOp>(module);
  mod->walk([&](func::FuncOp func) { analyzeFunction(func); });
}

/// Public interface
DatablockAnalysis::Graph DatablockAnalysis::analyzeFunction(func::FuncOp func) {
  Graph graph;
  DominanceInfo domInfo(func);
  collectNodes(func.getBody(), graph);
  buildAdjacency(graph, domInfo);
  return graph;
}

void DatablockAnalysis::printGraph(func::FuncOp func) {
  LLVM_DEBUG(dbgs() << line);
  LLVM_DEBUG(DBGS() << "Printing graph for function: " << func.getName()
                    << "\n");
  if (functionGraphMap.count(func))
    printGraph(functionGraphMap[func]);
  else
    llvm::errs() << "No graph for function: " << func.getName() << "\n";
}

void DatablockAnalysis::computeStatistics(func::FuncOp func) {
  if (functionGraphMap.count(func))
    computeStatistics(functionGraphMap[func]);
}

DatablockAnalysis::Graph &
DatablockAnalysis::getOrCreateGraph(func::FuncOp func) {
  if (functionGraphMap.count(func))
    return functionGraphMap[func];
  functionGraphMap[func] = analyzeFunction(func);
  return functionGraphMap[func];
}

SetVector<scf::ForOp> DatablockAnalysis::getLoopsFromOffset(Value dbOffset) {
  if (offsetMap.count(dbOffset))
    return offsetMap[dbOffset];
  return {};
}

/// Dependency analysis.
bool DatablockAnalysis::mayOverlap(Node &A, Node &B) {
  if (!A.info.valid || !B.info.valid)
    return true;
  if (A.info.offsets.size() != B.info.offsets.size())
    return true;
  for (unsigned d = 0; d < A.info.offsets.size(); d++) {
    int64_t Astart = A.info.offsets[d];
    int64_t Aend = Astart + A.info.sizes[d];
    int64_t Bstart = B.info.offsets[d];
    int64_t Bend = Bstart + B.info.sizes[d];
    if (Aend <= Bstart || Bend <= Astart)
      return false;
  }
  return true;
}

bool DatablockAnalysis::mayDepend(Node &prod, Node &cons, bool &isDirect,
                                  bool &isLoopDependent,
                                  DominanceInfo &domInfo) {
  /// Verify if the nodes belong to the same EDT.
  if (prod.edtParent != cons.edtParent)
    return false;

  /// Exclude nodes with the same EDT user.
  if (prod.edtUser == cons.edtUser)
    return false;

  /// Only consider writer producer and reader consumer.
  if (!isWriter(prod) || !isReader(cons))
    return false;

  /// Check if nodes are different
  auto compResult = compare(prod, cons);
  if (compResult == NodeComp::Different)
    return false;

  /// There is a direct dependency if
  isDirect = true ? (compResult == NodeComp::Equal) : false;

  /// Check loop usage: if both nodes are dependent on the same loop
  if (prod.isLoopDependent && cons.isLoopDependent) {
    /// Iterate over corresponding offsets.
    for (size_t i = 0, e = prod.offsets.size(); i < e; ++i) {
      auto prodLoops = getLoopsFromOffset(prod.offsets[i]);
      auto consLoops = getLoopsFromOffset(cons.offsets[i]);
      for (auto &pLoop : prodLoops) {
        if (consLoops.contains(pLoop)) {
          isLoopDependent = true;
          break;
        }
      }

      /// No need to keep looking if a common loop is found.
      if (isLoopDependent)
        break;
    }
  }

  /// Finally, if producer dominates consumer, report a dependency.
  return domInfo.dominates(prod.op.getOperation(), cons.op.getOperation());
}

bool DatablockAnalysis::baseMayAlias(Node &A, Node &B) {
  if (A.aliases.contains(B.id))
    return true;

  for (auto &eA : A.effects) {
    for (auto &eB : B.effects) {
      if (mayAlias(eA, eB)) {
        A.aliases.insert(B.id);
        B.aliases.insert(A.id);
        return true;
      }
    }
  }
  return false;
}

DatablockAnalysis::NodeComp DatablockAnalysis::compare(Node &A, Node &B) {
  /// If it is already known that the nodes are equivalent, return true.
  if (A.duplicates.contains(B.id) || B.duplicates.contains(A.id))
    return NodeComp::Equal;

  /// Compute it, otherwise.
  if (!baseMayAlias(A, B))
    return NodeComp::Different;

  /// Compare offsets
  if (A.offsets.size() != B.offsets.size())
    return NodeComp::BaseAlias;

  if (!std::equal(A.offsets.begin(), A.offsets.end(), B.offsets.begin()))
    return NodeComp::BaseAlias;

  /// If static info is not available, assume they are equivalent.
  if (A.info.valid != B.info.valid)
    return NodeComp::BaseAlias;
  if (A.info.valid) {
    for (unsigned i = 0; i < A.info.offsets.size(); ++i) {
      if (A.info.offsets[i] != B.info.offsets[i] ||
          A.info.sizes[i] != B.info.sizes[i] ||
          A.info.strides[i] != B.info.strides[i])
        return NodeComp::BaseAlias;
    }
  }

  /// Cache the result.
  A.duplicates.insert(B.id);
  B.duplicates.insert(A.id);
  return NodeComp::Equal;
}

void DatablockAnalysis::buildAdjacency(Graph &graph, DominanceInfo &domInfo) {
  auto graphSize = graph.nodes.size();
  for (unsigned i = 0; i < graphSize; i++) {
    for (unsigned j = 0; j < graphSize; j++) {
      if (i == j)
        continue;
      Node &A = graph.nodes[i];
      Node &B = graph.nodes[j];
      bool isDirect = true, isLoopDependent = false;
      if (mayDepend(A, B, isDirect, isLoopDependent, domInfo))
        graph.edges.push_back({A.id, B.id, isDirect, isLoopDependent});
    }
  }
}

/// Analysis
void DatablockAnalysis::analyzeLoops(
    Region &region, DenseMap<Value, llvm::SmallVector<scf::ForOp, 4>> &valMap) {
  /// It traverses from the supplied induction variable (iv) through its uses.
  auto bfsForward = [&](scf::ForOp fop) {
    DenseSet<Value> visited;
    SmallVector<Value, 8> queue;
    Value iv = fop.getInductionVar();
    visited.insert(iv);
    queue.push_back(iv);
    while (!queue.empty()) {
      Value cur = queue.pop_back_val();
      for (auto &use : cur.getUses()) {
        Operation *owner = use.getOwner();
        /// Ignore
        for (Value res : owner->getResults()) {
          if (visited.insert(res).second)
            queue.push_back(res);
        }
      }
    }

    /// Insert the induction variable into the map.
    for (auto &val : visited)
      valMap[val].push_back(fop);
  };

  /// Analyze the induction variables of all loops and mark the ops that depend
  /// on them.
  llvm::SmallVector<scf::ForOp, 8> loops;
  region.walk([&](scf::ForOp fop) { loops.push_back(fop); });
  for (auto fop : loops)
    bfsForward(fop);
}

/// Node collection
void DatablockAnalysis::collectNodes(Region &region, Graph &graph) {
  unsigned nextID = 0;
  /// Collect loops and variables that depend on them.
  DenseMap<Value, llvm::SmallVector<scf::ForOp, 4>> loopValsMap;
  analyzeLoops(region, loopValsMap);

  /// Collect datablock nodes from the top-level region of a function.
  region.walk([&](arts::DataBlockOp dbOp) {
    /// Set node information.
    Node node;
    node.id = nextID++;
    node.op = dbOp;
    node.mode = dbOp.getMode();
    node.baseMemref = dbOp.getBase();
    node.offsets = dbOp.getOffsets();
    node.sizes = dbOp.getSizes();
    node.strides = dbOp.getStrides();
    setSubviewInfo(node);

    /// Collect memory effects
    if (auto memEff = dyn_cast<MemoryEffectOpInterface>(dbOp.getOperation()))
      memEff.getEffects(node.effects);

    /// Add base is datablock attribute.
    if (auto baseOp = dyn_cast_or_null<arts::DataBlockOp>(
            node.baseMemref.getDefiningOp())) {
      dbOp->setAttr("baseIsDb", UnitAttr::get(dbOp.getContext()));
      node.baseIsDb = true;
    } else {
      node.baseIsDb = false;
    }

    /// Set the isLoad flag.
    node.isLoad = dbOp->hasAttr("isLoad");

    /// Analyze uses of the datablock.
    unsigned numUses = 0;
    for (auto &use : dbOp->getUses()) {
      Operation *userOp = use.getOwner();
      ++numUses;
      /// Set the EDT user of the datablock.
      if (userOp->getBlock() == dbOp->getBlock() && isa<arts::EdtOp>(userOp)) {
        assert(node.edtUser == nullptr && "Multiple EDT users");
        node.edtUser = cast<arts::EdtOp>(userOp);
        unsigned edtDepId = 0;
        for (auto dep : node.edtUser.getDependencies()) {
          if (dep.getDefiningOp() == dbOp)
            node.edtDepId = edtDepId;
          ++edtDepId;
        }
      }
      /// Set the region users of the datablock.
      if (auto *r = userOp->getParentRegion())
        node.userRegions.insert(r);
    }
    node.useCount = numUses;

    /// Try to get the EDT parent of the datablock.
    if (auto parentOp = dbOp->getParentOfType<arts::EdtOp>())
      node.edtParent = parentOp;

    /// Insert the loop values into the offset map.
    for (auto offVal : node.offsets) {
      if (loopValsMap.count(offVal)) {
        node.isLoopDependent = true;
        auto &loopVals = loopValsMap[offVal];
        offsetMap[offVal].insert(loopVals.begin(), loopVals.end());
      }
    }

    /// Add the node to the graph.
    graph.nodes.push_back(std::move(node));
    nodeMap[dbOp] = graph.nodes.back();
  });
}

void DatablockAnalysis::setSubviewInfo(Node &node) {
  /// Validate that offsets, sizes, and strides arrays have the same length.
  if (node.offsets.size() != node.sizes.size() ||
      node.offsets.size() != node.strides.size())
    return;

  const auto rank = node.offsets.size();
  SubviewInfo &sb = node.info;
  sb.offsets.resize(rank);
  sb.sizes.resize(rank);
  sb.strides.resize(rank);

  for (size_t i = 0; i < rank; i++) {
    const int64_t o = tryParseIndexConstant(node.offsets[i]);
    const int64_t s = tryParseIndexConstant(node.sizes[i]);
    const int64_t st = tryParseIndexConstant(node.strides[i]);
    if (o < 0 || s < 0 || st < 0)
      return;
    sb.offsets[i] = o;
    sb.sizes[i] = s;
    sb.strides[i] = st;
  }
  sb.valid = true;
}

/// Statistics
void DatablockAnalysis::computeStatistics(Graph &graph) {
  if (graph.nodes.empty())
    return;
  unsigned totalUses = 0;
  unsigned totalRegions = 0;
  for (auto &node : graph.nodes) {
    totalUses += node.useCount;
    totalRegions += node.userRegions.size();
  }
  double avgUses = (double)totalUses / graph.nodes.size();
  double avgRegions = (double)totalRegions / graph.nodes.size();
  LLVM_DEBUG(DBGS() << "Average use count per datablock: " << avgUses << "\n"
                    << "Average region frequency per datablock: " << avgRegions
                    << "\n");
}

/// Other
void DatablockAnalysis::printGraph(Graph &graph) {
  std::string info;
  llvm::raw_string_ostream os(info);
  os << "Nodes:\n";
  for (auto &n : graph.nodes) {
    os << "  #" << n.id << " " << n.mode << "\n";
    os << "    " << n.op << "\n";
    os << "       base=" << n.baseMemref << "\n";
    if (n.info.valid) {
      os << "       info=[";
      for (unsigned d = 0; d < n.info.offsets.size(); d++) {
        os << n.info.offsets[d] << ".."
           << (n.info.offsets[d] + n.info.sizes[d]);
        if (d + 1 < n.info.offsets.size())
          os << ", ";
      }
      os << "]";
    } else {
      os << "       info=(unknown)";
    }
    os << " isLoopDependent=" << (n.isLoopDependent ? "true" : "false");
    os << " useCount=" << n.useCount;
    os << " usedInRegions=" << n.userRegions.size();
    os << " baseIsDb=" << (n.baseIsDb ? "true" : "false");
    os << " isLoad=" << (n.isLoad ? "true" : "false");
    os << " edtDepId=" << n.edtDepId << "\n";
  }
  os << "Edges:\n";
  for (auto &e : graph.edges)
    os << "  #" << e.producerID << " -> #" << e.consumerID << " ("
       << (e.isDirect ? "direct" : "indirect")
       << (e.isLoopDependent ? ", loop dependent" : "") << ")\n";
  os << "Total nodes: " << graph.nodes.size() << "\n";
  LLVM_DEBUG(dbgs() << os.str());
}

/// Optimizations
void DatablockAnalysis::fuseAdjacentNodes(Graph &graph) {
  llvm::SmallDenseSet<unsigned> fusedIDs;
  for (unsigned i = 0; i < graph.nodes.size(); i++) {
    for (unsigned j = i + 1; j < graph.nodes.size(); j++) {
      if (fusedIDs.contains(j))
        continue;
      Node &A = graph.nodes[i];
      Node &B = graph.nodes[j];
      if (A.mode != B.mode || A.baseMemref != B.baseMemref)
        continue;
      if (!A.info.valid || !B.info.valid ||
          A.info.offsets.size() != B.info.offsets.size())
        continue;
      bool fusible = false;
      unsigned fuseDim = 0;
      unsigned rank = A.info.offsets.size();
      for (unsigned d = 0; d < rank; d++) {
        if (A.info.offsets[d] + A.info.sizes[d] == B.info.offsets[d]) {
          bool equalOtherDims = true;
          for (unsigned k = 0; k < rank; k++) {
            if (k == d)
              continue;
            if (A.info.offsets[k] != B.info.offsets[k] ||
                A.info.sizes[k] != B.info.sizes[k]) {
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
        auto &A_mut = graph.nodes[i];
        A_mut.info.sizes[fuseDim] += B.info.sizes[fuseDim];
        A_mut.useCount += B.useCount;
        for (Region *r : B.userRegions)
          A_mut.userRegions.insert(r);
        // for (Operation *op : B.topUsers)
        //   A_mut.topUsers.push_back(op);
        fusedIDs.insert(j);
        LLVM_DEBUG(DBGS() << "Fused node #" << j << " into node #" << i
                          << " along dimension " << fuseDim << "\n");
      }
    }
  }
  SmallVector<Node, 8> newNodes;
  for (unsigned i = 0; i < graph.nodes.size(); i++) {
    if (!fusedIDs.contains(i))
      newNodes.push_back(graph.nodes[i]);
  }
  graph.nodes = newNodes;
}

void DatablockAnalysis::detectOutOnlyNodes(Graph &graph) {
  // llvm::SmallDenseSet<unsigned> outOnlyIDs;
  // for (unsigned i = 0; i < graph.nodes.size(); i++) {
  //   // Node &node = graph.nodes[i];
  //   // if (isWriter(node) && node.topUsers.empty()) {
  //   //   outOnlyIDs.insert(i);
  //   //   LLVM_DEBUG(DBGS() << "No consumer from #" << i
  //   //                     << " => out-only => removable\n");
  //   // }
  // }
  // llvm::SmallVector<Node, 8> newNodes;
  // for (unsigned i = 0; i < graph.nodes.size(); i++) {
  //   if (!outOnlyIDs.contains(i))
  //     newNodes.push_back(graph.nodes[i]);
  // }
  // graph.nodes = newNodes;
}

void DatablockAnalysis::deduplicateNodes(Graph &graph) {
  llvm::SmallDenseSet<unsigned> duplicateIDs;
  for (unsigned i = 0; i < graph.nodes.size(); i++) {
    for (unsigned j = i + 1; j < graph.nodes.size(); j++) {
      if (compare(graph.nodes[i], graph.nodes[j]) == NodeComp::Equal) {
        duplicateIDs.insert(j);
        // graph.nodes[j].op->replaceAllUsesWith(graph.nodes[i].op.getResult());
        // graph.nodes[j].op.erase();
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

} // namespace arts
} // namespace mlir
