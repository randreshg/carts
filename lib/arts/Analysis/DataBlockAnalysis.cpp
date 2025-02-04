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
///      datablocks whose offsets depend on a loop IV as usedInLoop.
///   6. Builds a dependency graph (read-after-write edges) based on numeric
///      i, dominance (skipped if usedInLoop is true), and
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
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/Operation.h"
#include "mlir/Support/LLVM.h"
#include "polygeist/Ops.h"
/// Arts
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
#define DEBUG_TYPE "datablock-analysis"
#define line "-----------------------------------------\n"
#define dbgs() (llvm::dbgs())
#define DBGS() (dbgs() << "[" DEBUG_TYPE "] ")
/// Others
#include <optional>

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
  analyzeLoops(func.getBody(), graph);
  buildAdjacency(graph, domInfo);
  // deduplicateNodes(graph);
  return graph;
}

void DatablockAnalysis::printGraph(func::FuncOp func) {
  LLVM_DEBUG(dbgs() << line);
  LLVM_DEBUG(DBGS() <<  "Printing graph for function: " << func.getName()
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

SmallVector<unsigned, 4> DatablockAnalysis::getNodes(Value dbOffset) {
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

bool DatablockAnalysis::mayDepend(Node &prod, Node &cons,
                                  DominanceInfo &domInfo) {
  /// Check if a producer is a writer and a consumer is a reader.
  if (!isWriter(prod) || !isReader(cons))
    return false;

  /// Check if are equivalents or may alias.
  if (!(areEquivalent(prod, cons) || baseMayAlias(prod, cons)))
    return false;

  /// Check if a producer node dominates a consumer node.
  if (domInfo.dominates(prod.op.getOperation(), cons.op.getOperation()))
    return true;

  /// If not dominated, but both are used in a loop, consider them dependent.
  if (prod.usedInLoop && cons.usedInLoop)
    return true;

  /// Otherwise, it is safe to assume no dependency.
  return false;
}

bool DatablockAnalysis::baseMayAlias(Node &A, Node &B) {
  for (auto &eA : A.effects) {
    for (auto &eB : B.effects) {
      if (mayAlias(eA, eB))
        return true;
    }
  }
  return false;
}

bool DatablockAnalysis::areEquivalent(Node &A, Node &B) {
  /// If it is already known that the nodes are equivalent, return true.
  if (A.duplicates.contains(B.id) || B.duplicates.contains(A.id))
    return true;

  /// Compute it, otherwise.
  if (A.baseMemref != B.baseMemref)
    return false;
  if (A.info.valid != B.info.valid)
    return false;
  if (A.info.valid) {
    if (A.info.offsets.size() != B.info.offsets.size())
      return false;
    for (unsigned i = 0; i < A.info.offsets.size(); ++i) {
      if (A.info.offsets[i] != B.info.offsets[i] ||
          A.info.sizes[i] != B.info.sizes[i] ||
          A.info.strides[i] != B.info.strides[i])
        return false;
    }
  }

  /// Cache the result.
  A.duplicates.insert(B.id);
  B.duplicates.insert(A.id);
  return true;
}

void DatablockAnalysis::buildAdjacency(Graph &graph, DominanceInfo &domInfo) {
  auto graphSize = graph.nodes.size();
  for (unsigned i = 0; i < graphSize; i++) {
    for (unsigned j = 0; j < graphSize; j++) {
      if (i == j)
        continue;
      Node &A = graph.nodes[i];
      Node &B = graph.nodes[j];
      if (mayDepend(A, B, domInfo))
        graph.edges.push_back({A.id, B.id});
    }
  }
}

/// Analysis
void DatablockAnalysis::analyzeLoops(Region &region, Graph &graph) {
  /// It traverses from the supplied induction variable (iv) through its uses.
  /// For each reached value that is found in the offset map (offsetMap),
  /// the corresponding datablock node is marked as usedInLoop.
  auto bfsForward = [&](Value iv) {
    DenseSet<Value> visited;
    SmallVector<Value, 8> queue;
    visited.insert(iv);
    queue.push_back(iv);
    while (!queue.empty()) {
      Value cur = queue.pop_back_val();
      if (auto it = offsetMap.find(cur); it != offsetMap.end()) {
        for (unsigned nodeID : it->second)
          graph.nodes[nodeID].usedInLoop = true;
      }
      for (auto &use : cur.getUses()) {
        Operation *owner = use.getOwner();
        for (Value res : owner->getResults()) {
          if (visited.insert(res).second)
            queue.push_back(res);
        }
      }
    }
  };

  /// Analyze the induction variables of all loops to mark datablocks
  /// whose offsets depend on a loop induction variable.
  llvm::SmallVector<scf::ForOp, 8> loops;
  region.walk([&](scf::ForOp fop) { loops.push_back(fop); });
  for (auto fop : loops)
    bfsForward(fop.getInductionVar());
}

/// Node collection
void DatablockAnalysis::collectNodes(Region &region, Graph &graph) {
  unsigned nextID = 0;
  /// Collect datablock nodes from the top-level region of a function.
  region.walk([&](arts::DataBlockOp dbOp) {
    /// Set node information.
    Node node;
    node.id = nextID++;
    node.op = dbOp;
    auto modeAttr = dbOp->getAttrOfType<StringAttr>("mode");
    assert(modeAttr && "mode attribute not found");
    node.mode = modeAttr.str();
    node.baseMemref = dbOp.getBase();
    node.info = parseSubviewInfo(dbOp);
    if (auto memEff = dyn_cast<MemoryEffectOpInterface>(dbOp.getOperation()))
      memEff.getEffects(node.effects);

    /// Analyze uses of the datablock.
    unsigned numUses = 0;
    for (auto &use : dbOp->getUses()) {
      Operation *userOp = use.getOwner();
      ++numUses;
      /// Set the EDT user of the datablock.
      if (userOp->getBlock() == dbOp->getBlock() && isa<arts::EdtOp>(userOp)) {
        assert(node.edtUser == nullptr && "Multiple EDT users");
        node.edtUser = cast<arts::EdtOp>(userOp);
      }
      /// Set the region users of the datablock.
      if (auto *r = userOp->getParentRegion())
        node.userRegions.insert(r);
    }
    node.useCount = numUses;

    /// Try to get the EDT parent of the datablock.
    if (auto parentOp = dbOp->getParentOfType<arts::EdtOp>())
      node.edtParent = parentOp;

    /// Add the node to the graph.
    graph.nodes.push_back(std::move(node));

    /// Add the node to the offset map.
    unsigned idx = graph.nodes.size() - 1;
    for (Value offVal : dbOp.getOffsets())
      offsetMap[offVal].push_back(idx);
  });
}

std::optional<int64_t> DatablockAnalysis::computeConstant(Value val) {
  /// Compute a constant integer value from a Value.
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
      return (*lhs) * (*rhs);
  }
  return std::nullopt;
}

int64_t DatablockAnalysis::tryParseIndexConstant(Value val) {
  /// Try to parse an index constant from a Value.
  if (auto c = computeConstant(val))
    return *c;
  ValueOrInt voi(val);
  if (!voi.isValue)
    return voi.i_val;
  return -1;
}

DatablockAnalysis::SubviewInfo
DatablockAnalysis::parseSubviewInfo(arts::DataBlockOp dbOp) {
  /// Try to parse the subview information for a datablock operation,
  /// if it is known at compile time.
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
    if (o < 0 || s < 0 || st < 0)
      return sb;
    sb.offsets[i] = o;
    sb.sizes[i] = s;
    sb.strides[i] = st;
  }
  sb.valid = true;
  return sb;
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
    os << "    base=" << n.baseMemref << "\n";
    if (n.info.valid) {
      os << "    info=[";
      for (unsigned d = 0; d < n.info.offsets.size(); d++) {
        os << n.info.offsets[d] << ".."
           << (n.info.offsets[d] + n.info.sizes[d]);
        if (d + 1 < n.info.offsets.size())
          os << ", ";
      }
      os << "]";
    } else {
      os << "    info=(unknown)";
    }
    os << " usedInLoop=" << (n.usedInLoop ? "true" : "false");
    os << " useCount=" << n.useCount;
    os << " usedInRegions=" << n.userRegions.size() << "\n";
    // os << " edtUser=" << n.edtUser << "\n";
  }
  os << "Edges:\n";
  for (auto &e : graph.edges)
    os << "  #" << e.producerID << " -> #" << e.consumerID << "\n";
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
  llvm::SmallDenseSet<unsigned> outOnlyIDs;
  for (unsigned i = 0; i < graph.nodes.size(); i++) {
    Node &node = graph.nodes[i];
    // if (isWriter(node) && node.topUsers.empty()) {
    //   outOnlyIDs.insert(i);
    //   LLVM_DEBUG(DBGS() << "No consumer from #" << i
    //                     << " => out-only => removable\n");
    // }
  }
  llvm::SmallVector<Node, 8> newNodes;
  for (unsigned i = 0; i < graph.nodes.size(); i++) {
    if (!outOnlyIDs.contains(i))
      newNodes.push_back(graph.nodes[i]);
  }
  graph.nodes = newNodes;
}

void DatablockAnalysis::deduplicateNodes(Graph &graph) {
  llvm::SmallDenseSet<unsigned> duplicateIDs;
  for (unsigned i = 0; i < graph.nodes.size(); i++) {
    for (unsigned j = i + 1; j < graph.nodes.size(); j++) {
      if (areEquivalent(graph.nodes[i], graph.nodes[j])) {
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
