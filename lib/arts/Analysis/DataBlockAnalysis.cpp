///==========================================================================
/// File: DataBlockAnalysis.cpp
///==========================================================================

#include "arts/Analysis/DataBlockAnalysis.h"
/// Dialects
#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
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
using namespace mlir::scf;
using namespace mlir::affine;

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
  if (functionGraphMap.count(func)) {
    LLVM_DEBUG(DBGS() << "Printing graph for function: " << func.getName()
                      << "\n");
    printGraph(functionGraphMap[func]);
  } else
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

/// Returns true if `target` is reachable from `source` in the EDT CFG.
bool isReachable(Operation *source, Operation *target) {
  /// Early exit if either pointer is null or both are the same.
  if (!source || !target)
    return false;
  if (source == target)
    return true;

  /// If both operations are in the same block, check their order.
  if (source->getBlock() == target->getBlock())
    return source->isBeforeInBlock(target);

  /// Get the EDT parent for both operations. This acts as our upper limit.
  auto srcEdt = source->getParentOfType<EdtOp>();
  auto tgtEdt = target->getParentOfType<EdtOp>();
  if (srcEdt != tgtEdt)
    return false;

  /// Traverse up the parent chain simultaneously but stop once the EDT parent
  /// is reached.
  Operation *src = source;
  Operation *tgt = target;
  while (true) {
    if (src->getBlock() == tgt->getBlock())
      return src->isBeforeInBlock(tgt);
    if (src == srcEdt && tgt == tgtEdt)
      break;
    if (src->getParentOp() != srcEdt)
      src = src->getParentOp();
    if (tgt->getParentOp() != tgtEdt)
      tgt = tgt->getParentOp();
  }
  return false;
}

/// Dependency analysis.
bool DatablockAnalysis::mayDepend(Node &prod, Node &cons, bool &isDirect,
                                  bool &isLoopDependent,
                                  DominanceInfo &domInfo) {
  LLVM_DEBUG(DBGS() << "Checking dependency between\n"
                    << "  - " << prod.op << "\n  - " << cons.op << "\n");
  /// Verify if the nodes belong to the same EDT.
  if (prod.edtParent != cons.edtParent) {
    LLVM_DEBUG(dbgs() << "    - Different EDT parents\n");
    return false;
  }

  /// Exclude nodes with the same EDT user.
  if (prod.userEdt == cons.userEdt) {
    LLVM_DEBUG(dbgs() << "    - Same EDT user\n");
    return false;
  }

  /// Only consider writer producer and reader consumer.
  if (!isWriter(prod) || !isReader(cons)) {
    LLVM_DEBUG(dbgs() << "    - Not a writer or reader\n");
    return false;
  }

  /// Check if nodes are different
  auto compResult = compare(prod, cons);
  if (compResult == NodeComp::Different) {
    LLVM_DEBUG(dbgs() << "    - Different nodes\n");
    return false;
  }

  /// There is a direct dependency if
  isDirect = true ? (compResult == NodeComp::Equal) : false;

  /// If any of the nodes are loop dependent, mark the dependency as loop
  /// dependent.
  isLoopDependent = prod.isLoopDependent || cons.isLoopDependent;

  /// If producer dominates consumer, report a dependency.
  if (domInfo.dominates(prod.op.getOperation(), cons.op.getOperation())) {
    LLVM_DEBUG(dbgs() << "    - It is a dependency because of dominance\n");
    return true;
  }

  /// Finally, if the producer is loop dependent ant it reaches the consumer,
  /// report a dependency.
  if (isLoopDependent &&
      isReachable(prod.op.getOperation(), cons.op.getOperation())) {
    LLVM_DEBUG(dbgs() << "    - It is a dependency because of reachability\n");
    return true;
  }
  LLVM_DEBUG(dbgs() << "    - No dependency\n");
  return false;
}

bool DatablockAnalysis::ptrMayAlias(Node &A, Node &B) {
  if (A.aliases.contains(B.id))
    return true;

  for (auto &eA : A.effects) {
    for (auto &eB : B.effects) {
      if (mayAlias(eA, eB)) {
        LLVM_DEBUG(dbgs() << "    - Datablocks may alias\n");
        A.aliases.insert(B.id);
        B.aliases.insert(A.id);
        return true;
      }
    }
  }
  return false;
}

bool DatablockAnalysis::ptrMayAlias(Node &A, Value val) {
  for (auto &eA : A.effects) {
    if (mayAlias(eA, val))
      return true;
  }
  return false;
}

DatablockAnalysis::NodeComp DatablockAnalysis::compare(Node &A, Node &B) {
  /// If it is already known that the nodes are equivalent, return true.
  if (A.duplicates.contains(B.id) || B.duplicates.contains(A.id))
    return NodeComp::Equal;

  /// Compute it, otherwise.
  if (!ptrMayAlias(A, B))
    return NodeComp::Different;

  /// Compare indices
  if (A.indices.size() != B.indices.size())
    return NodeComp::BaseAlias;

  if (!std::equal(A.indices.begin(), A.indices.end(), B.indices.begin()))
    return NodeComp::BaseAlias;

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
      bool isDirect, isLoopDependent;
      if (mayDepend(A, B, isDirect, isLoopDependent, domInfo))
        graph.edges.push_back({A.id, B.id, isDirect, isLoopDependent});
    }
  }
}

/// Analysis
void DatablockAnalysis::analyzeLoops(
    Region &region, DenseMap<Value, SmallVector<Operation *, 4>> &valMap) {
  auto loopBfs = [&](Operation *loopOp, Value iv) {
    DenseSet<Value> visited;
    SmallVector<Value, 8> queue;
    visited.insert(iv);
    queue.push_back(iv);
    while (!queue.empty()) {
      Value cur = queue.pop_back_val();
      for (auto &use : cur.getUses()) {
        Operation *owner = use.getOwner();
        for (Value res : owner->getResults()) {
          if (visited.insert(res).second)
            queue.push_back(res);
        }
      }
    }
    for (auto &val : visited)
      valMap[val].push_back(loopOp);
  };

  /// Analyze scf::ForOp loops.
  SmallVector<scf::ForOp, 8> scfLoops;
  region.walk([&](scf::ForOp fop) { scfLoops.push_back(fop); });
  for (auto fop : scfLoops)
    loopBfs(fop, fop.getInductionVar());

  /// Analyze affine::AffineForOp loops.
  SmallVector<affine::AffineForOp, 8> affineLoops;
  region.walk([&](affine::AffineForOp aop) { affineLoops.push_back(aop); });
  for (auto aop : affineLoops)
    loopBfs(aop, aop.getInductionVar());
}

/// Node collection
void DatablockAnalysis::collectNodes(Region &region, Graph &graph) {
  unsigned nextID = 0;
  /// Collect loops and variables that depend on them.
  DenseMap<Value, SmallVector<Operation *, 4>> loopValsMap;
  analyzeLoops(region, loopValsMap);

  /// Collect datablock nodes from the top-level region of a function.
  region.walk<mlir::WalkOrder::PreOrder>([&](arts::DataBlockOp dbOp) {
    /// Set node information.
    Node node;
    node.id = nextID++;
    node.op = dbOp;
    node.mode = dbOp.getMode();
    node.ptr = dbOp.getPtr();
    node.indices = dbOp.getIndices();
    node.sizes = dbOp.getSizes();
    node.elementType = dbOp.getElementType();
    node.elementTypeSize =
        cast<arith::ConstantIndexOp>(dbOp.getElementTypeSize().getDefiningOp())
            .getValue()
            .cast<IntegerAttr>()
            .getInt();
    node.resultType = dbOp.getResult().getType();

    /// If affine map is present, set it.
    if (auto affineMap = dbOp.getAffineMap())
      node.affineMap = *affineMap;

    /// Add 'isPtrDb' attribute if the base is a datablock.
    if (auto parentDb =
            dyn_cast_or_null<arts::DataBlockOp>(node.ptr.getDefiningOp())) {
      dbOp.setIsPtrDbAttr();
      node.isPtrDb = true;
      /// Collect memory effects from the parent datablock.
      node.parent = &nodeMap[parentDb];
      for (auto &e : node.parent->effects)
        node.effects.push_back(e);
    } else {
      node.isPtrDb = false;
      /// Collect memory effects.
      if (auto memEff = dyn_cast<MemoryEffectOpInterface>(dbOp.getOperation()))
        memEff.getEffects(node.effects);
    }

    /// Add 'isSingle' attribute if the datablock has a single size of 1.
    node.isSingle = false;
    if (node.sizes.size() == 1) {
      if (auto cstOp = node.sizes[0].getDefiningOp<arith::ConstantIndexOp>()) {
        if (cstOp.value() == 1) {
          dbOp.setIsSingleAttr();
          node.isSingle = true;
        }
      }
    }

    /// Try to get the EDT parent of the datablock.
    if (auto parentOp = dbOp->getParentOfType<arts::EdtOp>())
      node.edtParent = parentOp;

    /// Mark the node as loop dependent if any of its indices depend on a loop.
    for (auto offVal : node.indices) {
      if (loopValsMap.count(offVal))
        node.isLoopDependent = true;
    }

    /// Collect and analyze uses of the datablock.
    collectUses(node);

    /// Add the node to the graph.
    graph.nodes.push_back(std::move(node));
    nodeMap[dbOp] = graph.nodes.back();
  });
}

/// Uses
void DatablockAnalysis::collectUses(Node &node) {
  unsigned numUses = 0;
  auto dbOp = node.op;

  /// If the only user is an EDT, it is because the datablock hasn't been
  /// rewired yet.
  if (dbOp->hasOneUse() && isa<arts::EdtOp>(dbOp->use_begin()->getOwner())) {
    llvm::errs() << "Datablock " << dbOp << " hasn't been rewired yet\n";
    assert(false && "Datablock not rewired");
    return;
  }

  /// Analyze uses
  {
    auto *dbBlock = dbOp->getBlock();
    for (auto &use : dbOp->getUses()) {
      ++numUses;

      /// Set EDT user and position.
      Operation *userOp = use.getOwner();
      if (userOp->getBlock() != dbBlock)
        continue;
      if (auto edtOp = dyn_cast<arts::EdtOp>(userOp)) {
        assert(!node.userEdt && "Multiple EDT users");
        node.userEdt = edtOp;
        auto deps = node.userEdt.getDependencies();
        auto it = std::find_if(deps.begin(), deps.end(), [dbOp](auto dep) {
          return dep.getDefiningOp() == dbOp;
        });
        node.userEdtPos =
            (it != deps.end()) ? std::distance(deps.begin(), it) : 0;
      }
    }
    node.useCount = numUses;
  }
}

/// Statistics
void DatablockAnalysis::computeStatistics(Graph &graph) {
  if (graph.nodes.empty())
    return;
  unsigned totalUses = 0;
  unsigned totalRegions = 0;
  for (auto &node : graph.nodes) {
    totalUses += node.useCount;
    // totalRegions += node.userRegions.size();
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
    // os << "    ptr=" << n.ptr << "\n";
    os << "    isLoopDependent=" << (n.isLoopDependent ? "true" : "false");
    os << " useCount=" << n.useCount;
    os << " isPtrDb=" << (n.isPtrDb ? "true" : "false");
    os << " userEdtPos=" << n.userEdtPos << "\n";
  }
  os << "Edges:\n";
  for (auto &e : graph.edges)
    os << "  #" << e.producerID << " -> #" << e.consumerID << " ("
       << (e.isDirect ? "direct" : "indirect")
       << (e.isLoopDependent ? ", loop dependent" : "") << ")\n";
  os << "Total nodes: " << graph.nodes.size() << "\n";
  LLVM_DEBUG(dbgs() << os.str());
}

/// Utils

/// Optimizations
void DatablockAnalysis::fuseAdjacentNodes(Graph &graph) {}

void DatablockAnalysis::detectOutOnlyNodes(Graph &graph) {}

void DatablockAnalysis::deduplicateNodes(Graph &graph) {}

} // namespace arts
} // namespace mlir
