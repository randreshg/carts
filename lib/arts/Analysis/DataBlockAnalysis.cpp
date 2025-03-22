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
#include <utility>
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

//===----------------------------------------------------------------------===//
// DatablockNode
//===----------------------------------------------------------------------===//
void DatablockNode::collectUses() {
  unsigned numUses = 0;
  /// If the only user is an EDT, it is because the datablock hasn't been
  /// rewired yet.
  if (op->hasOneUse() && isa<arts::EdtOp>(op->use_begin()->getOwner())) {
    llvm::errs() << "Datablock " << op << " hasn't been rewired yet\n";
    assert(false && "Datablock not rewired");
    return;
  }

  /// Analyze uses
  {
    auto *dbBlock = op->getBlock();
    for (auto &use : op->getUses()) {
      ++numUses;

      /// Set EDT user and position.
      Operation *userOp = use.getOwner();
      if (userOp->getBlock() != dbBlock)
        continue;
      if (auto edtOp = dyn_cast<arts::EdtOp>(userOp)) {
        assert(!userEdt && "Multiple EDT users");
        userEdt = edtOp;
        auto deps = userEdt.getDependencies();
        auto it = std::find_if(deps.begin(), deps.end(), [this](auto dep) {
          return dep.getDefiningOp() == op;
        });
        userEdtPos = (it != deps.end()) ? std::distance(deps.begin(), it) : 0;
      }
    }
    useCount = numUses;
  }
}

//===----------------------------------------------------------------------===//
// DatablockGraph
//===----------------------------------------------------------------------===//
DatablockGraph::DatablockGraph(func::FuncOp func, DatablockAnalysis *DA)
    : func(func), DA(DA), entryDbNode(DA) {
  assert(func && "Function cannot be null");
  assert(DA && "DatablockAnalysis cannot be null");

  /// Create entry Node:
  {
    entryDbNode.id = 0;
    entryDbNode.op = nullptr;
    entryDbNode.ptr = nullptr;
  }

  collectNodes(func.getBody());
  if (nodes.empty())
    return;

  /// Build the datablock graph from the function body.
  build();
}

SetVector<unsigned> DatablockGraph::getProducers() const {
  SetVector<unsigned> producers;
  for (const auto &entry : edges)
    producers.insert(entry.first);
  return producers;
}

llvm::SmallDenseMap<unsigned, DatablockGraph::Edge::Type, 4>
DatablockGraph::getEdgesFor(unsigned producerID) {
  return edges[producerID];
}

bool DatablockGraph::addEdge(DatablockNode &prod, DatablockNode &cons,
                             Edge::Type ty) {
  /// If an edge from prod to cons already exists, do nothing.
  auto prodIt = edges.find(prod.id);
  if (prodIt != edges.end()) {
    if (prodIt->second.count(cons.id))
      return false;
  }
  /// Insert or update the consumer edge.
  edges[prod.id][cons.id] = ty;
  return true;
}

void DatablockGraph::build() {
  Environment env;
  processRegion(func.getBody(), env);
}

std::pair<Environment, bool> DatablockGraph::processRegion(Region &region,
                                                           Environment env) {
  Environment newEnv = env;
  bool changed = false;
  for (Operation &op : region.getOps()) {
    std::pair<Environment, bool> result;
    if (auto edtOp = dyn_cast<EdtOp>(&op))
      result = processEdt(edtOp, newEnv);
    else if (auto ifOp = dyn_cast<scf::IfOp>(&op))
      result = processIf(ifOp, newEnv);
    else if (auto forOp = dyn_cast<scf::ForOp>(&op))
      result = processFor(forOp, newEnv);
    else if (auto callOp = dyn_cast<func::CallOp>(&op))
      result = processCall(callOp, newEnv);
    else
      continue;

    /// Merge the new environment with the current one and check if it has
    /// changed.
    newEnv = mergeEnvironments(newEnv, result.first);
    changed = changed || result.second;
  }
  return {newEnv, changed};
}

std::pair<Environment, bool> DatablockGraph::processEdt(arts::EdtOp edtOp,
                                                        Environment env) {
  /// If the operation is an EDT, process its inputs and outputs.
  auto edtDeps = edtOp.getDependencies();
  bool changed = false;

  /// No dependencies, nothing to do.
  if (edtDeps.empty())
    return {env, false};

  Environment newEnv = env;
  SmallVector<DatablockNode, 4> dbIns, dbOuts;
  for (auto dep : edtDeps) {
    auto db = dyn_cast<DataBlockOp>(dep.getDefiningOp());
    assert(db && "Dependency must be a datablock");
    /// Get the datablock node from the environment and check its mode.
    auto &dbNode = getNode(db);
    if (dbNode.isWriter())
      dbOuts.push_back(dbNode);
    else if (dbNode.isReader())
      dbIns.push_back(dbNode);
  }

  /// Process inputs
  for (auto dbIn : dbIns) {
    auto prodDefs = findDefinition(dbIn, newEnv);
    /// No previous definition: add an edge from "entry" node.
    if (prodDefs.empty()) {
      addEdge(entryDbNode, dbIn, Edge::Type::Direct);
      continue;
    }

    /// Analyze potential dependencies for each definition.
    for (auto &prodDef : prodDefs) {
      /// No dependency: skip this datablock.
      bool isDirect = false;
      if (!DA->mayDepend(prodDef, dbIn, isDirect))
        continue;
      /// Record dependency
      addEdge(prodDef, dbIn,
              isDirect ? Edge::Type::Direct : Edge::Type::Indirect);
    }
  }

  /// Process outputs
  for (auto &dbOut : dbOuts) {
    /// Create a new instance for this output.
    auto prodDefs = findDefinition(dbOut, newEnv);
    if (prodDefs.empty()) {
      newEnv[dbOut.op] = dbOut;
      changed |= true;
      continue;
    }

    /// Analyze potential dependencies for each definition.
    for (auto &prodDef : prodDefs) {
      /// No dependency: skip this datablock.
      bool isDirect = false;
      if (!DA->mayDepend(prodDef, dbOut, isDirect))
        continue;
      /// Update the environment with the latest definition
      newEnv[prodDef.op] = dbOut;
      changed |= true;
    }
  }

  return {newEnv, changed};
}

std::pair<Environment, bool> DatablockGraph::processFor(scf::ForOp forOp,
                                                        Environment env) {
  /// If it is an scf.for, use fixed-point iteration.
  Environment loopEnv = env;
  std::pair<Environment, bool> bodyResult;
  bool changed = false;
  do {
    bodyResult = processRegion(forOp->getRegion(0), loopEnv);
    auto newLoopEnv = mergeEnvironments(env, bodyResult.first);
    /// Break if the environment has not changed after processing the body.
    changed = bodyResult.second || changed;
    if (!bodyResult.second)
      break;
    /// Update the loop environment for the next iteration.
    loopEnv = newLoopEnv;
  } while (true);
  return {loopEnv, changed};
}

std::pair<Environment, bool> DatablockGraph::processIf(scf::IfOp ifOp,
                                                       Environment env) {
  /// If it is an scf.if, process then and else regions separately, then
  /// merge.
  auto thenRes = processRegion(ifOp.getThenRegion(), env);
  auto elseRes = processRegion(ifOp.getElseRegion(), env);
  Environment merged = mergeEnvironments(thenRes.first, elseRes.first);
  bool changed = thenRes.second || elseRes.second;
  return {merged, changed};
}

std::pair<Environment, bool> DatablockGraph::processCall(func::CallOp callOp,
                                                         Environment env) {
  // TODO: Handle function calls
  return {env, false};
}

SmallVector<DatablockNode, 4>
DatablockGraph::findDefinition(DatablockNode dbNode, Environment env) {
  assert(dbNode.isReader() && "DatablockNode must be a reader");
  /// Find the definition of a datablock in the environment
  SmallVector<DatablockNode, 4> defs;
  for (auto &pair : env) {
    if (!DA->ptrMayAlias(pair.second, dbNode))
      continue;
    /// Be very pessimistic here, assuming that any aliasing means a potential
    /// dependency.
    defs.push_back(pair.second);
  }
  return defs;
}

Environment DatablockGraph::mergeEnvironments(const Environment &env1,
                                              const Environment &env2) {
  Environment mergedEnv = env1;
  for (auto &pair : env2) {
    auto dbOp = pair.first;
    auto dbNode = pair.second;
    if (mergedEnv.count(dbOp)) {
      LLVM_DEBUG(dbgs() << "Merging datablock " << dbOp << "\n");
    } else {
      /// Otherwise, add it to the merged environment.
      mergedEnv[dbOp] = dbNode;
    }
  }
  return mergedEnv;
}

void DatablockGraph::print() {
  std::string info;
  llvm::raw_string_ostream os(info);
  os << "Nodes:\n";
  for (auto &n : nodes) {
    os << "  #" << n.id << " " << n.mode << "\n";
    os << "    " << n.op << "\n";
    os << "    isLoopDependent=" << (n.isLoopDependent ? "true" : "false");
    os << " useCount=" << n.useCount;
    os << " hasPtrDb=" << (n.hasPtrDb ? "true" : "false");
    os << " userEdtPos=" << n.userEdtPos << "\n";
  }
  os << "Edges:\n";
  for (auto &e : edges) {
    auto &prodIt = e.second;
    for (auto &prod : prodIt) {
      auto &consIt = edges[prod.first];
      for (auto &cons : consIt) {
        os << "  #" << prod.first << " -> #" << cons.first << "\n";
      }
    }
    os << "Total nodes: " << nodes.size() << "\n";
    LLVM_DEBUG(dbgs() << os.str());
  }
}

void DatablockGraph::computeStatistics() {
  if (nodes.empty())
    return;
  unsigned totalUses = 0;
  unsigned totalRegions = 0;
  for (auto &node : nodes) {
    totalUses += node.useCount;
    // totalRegions += node.userRegions.size();
  }
  double avgUses = (double)totalUses / nodes.size();
  double avgRegions = (double)totalRegions / nodes.size();
  LLVM_DEBUG(DBGS() << "Average use count per datablock: " << avgUses << "\n"
                    << "Average region frequency per datablock: " << avgRegions
                    << "\n");
}

void DatablockGraph::collectNodes(Region &region) {
  /// Start IDs from 1 to avoid conflict with entry node (0).
  unsigned nextID = 1;

  /// Collect loops and variables that depend on them.
  DenseMap<Value, SmallVector<Operation *, 4>> loopValsMap;
  DA->analyzeLoops(region, loopValsMap);

  /// Collect datablock nodes from the top-level region of a function.
  region.walk<mlir::WalkOrder::PreOrder>([&](arts::DataBlockOp dbOp) {
    /// Set node information.
    DatablockNode node(DA);
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

    /// Add 'hasPtrDb' attribute if the base is a datablock.
    if (auto parentDb =
            dyn_cast_or_null<arts::DataBlockOp>(node.ptr.getDefiningOp())) {
      dbOp.setHasPtrDb();
      node.hasPtrDb = true;
      /// Collect memory effects from the parent datablock.
      node.parent = &nodeMap[parentDb];
      for (auto &e : node.parent->effects)
        node.effects.push_back(e);
    } else {
      node.hasPtrDb = false;
      /// Collect memory effects.
      if (auto memEff = dyn_cast<MemoryEffectOpInterface>(dbOp.getOperation()))
        memEff.getEffects(node.effects);
    }

    /// Add 'isSingle' attribute if the datablock has a single size of 1 or no
    /// size
    node.isSingle = false;
    if (node.sizes.empty()) {
      dbOp.setIsSingle();
      node.isSingle = true;
    } else if (node.sizes.size() == 1) {
      if (auto cstOp = node.sizes[0].getDefiningOp<arith::ConstantIndexOp>()) {
        if (cstOp.value() == 1) {
          dbOp.setIsSingle();
          node.isSingle = true;
        }
      }
    }

    /// Try to get the EDT parent of the datablock.
    if (auto parentOp = dbOp->getParentOfType<arts::EdtOp>())
      node.edtParent = parentOp;

    /// Mark the node as loop dependent if any of its indices depend on a
    /// loop.
    for (auto offVal : node.indices) {
      if (loopValsMap.count(offVal))
        node.isLoopDependent = true;
    }

    /// Collect and analyze uses of the datablock.
    node.collectUses();

    /// Add the node to the graph.
    nodes.push_back(std::move(node));
    nodeMap[dbOp] = nodes.back();
  });
}

void DatablockGraph::fuseAdjacentNodes() {}

void DatablockGraph::detectOutOnlyNodes() {}

void DatablockGraph::deduplicateNodes() {}

//===----------------------------------------------------------------------===//
// DatablockAnalysis
//===----------------------------------------------------------------------===//
DatablockAnalysis::DatablockAnalysis(Operation *module) {
  ModuleOp mod = cast<ModuleOp>(module);
  mod->walk([&](func::FuncOp func) { getOrCreateGraph(func); });
}

DatablockAnalysis::~DatablockAnalysis() {
  for (auto &pair : functionGraphMap)
    delete pair.second;
}

/// Public interface
void DatablockAnalysis::printGraph(func::FuncOp func) {
  LLVM_DEBUG(dbgs() << line);
  if (functionGraphMap.count(func)) {
    LLVM_DEBUG(DBGS() << "Printing graph for function: " << func.getName()
                      << "\n");
    functionGraphMap[func].print();
  } else
    llvm::errs() << "No graph for function: " << func.getName() << "\n";
}

DatablockGraph *DatablockAnalysis::getOrCreateGraph(func::FuncOp func) {
  if (functionGraphMap.count(func))
    return functionGraphMap[func];

  /// Checks if the function has a body, if not return null.
  if (func.getBody().empty())
    return nullptr;

  /// Create a new graph for the function and store it in the map.
  functionGraphMap[func] = new DatablockGraph(func, this);
  return functionGraphMap[func];
}

/// Returns true if `target` is reachable from `source` in the EDT CFG.
bool DatablockAnalysis::isReachable(Operation *source, Operation *target) {
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
bool DatablockAnalysis::mayDepend(DatablockNode &prod, DatablockNode &cons,
                                  bool &isDirect) {
  /// Verify if the nodes are different and belong to the same EDT parent.
  if ((&prod == &cons) || (prod.edtParent != cons.edtParent))
    return false;

  /// Only consider writer producer and reader consumer.
  if (!prod.isWriter() || !cons.isReader())
    return false;

  /// Check if nodes are different
  auto compResult = compare(prod, cons);
  if (compResult == DatablockNodeComp::Different)
    return false;

  /// There is a direct dependency if
  isDirect = true ? (compResult == DatablockNodeComp::Equal) : false;

  /// TODO: We can make a more complex analysis here, such as checking an
  /// affine iteration space, and verifying if the producer and consumer are
  /// in the same iteration space.
  return true;
}

bool DatablockAnalysis::ptrMayAlias(DatablockNode &A, DatablockNode &B) {
  if (A.aliases.contains(B.id))
    return true;

  for (auto &eA : A.effects) {
    for (auto &eB : B.effects) {
      if (mayAlias(eA, eB)) {
        // LLVM_DEBUG(dbgs() << "    - Datablocks may alias\n");
        A.aliases.insert(B.id);
        B.aliases.insert(A.id);
        return true;
      }
    }
  }
  return false;
}

bool DatablockAnalysis::ptrMayAlias(DatablockNode &A, Value val) {
  for (auto &eA : A.effects) {
    if (mayAlias(eA, val))
      return true;
  }
  return false;
}

DatablockNodeComp DatablockAnalysis::compare(DatablockNode &A,
                                             DatablockNode &B) {
  /// If it is already known that the nodes are equivalent, return true.
  if (A.duplicates.contains(B.id) || B.duplicates.contains(A.id))
    return DatablockNodeComp::Equal;

  /// Compute it, otherwise.
  if (!ptrMayAlias(A, B))
    return DatablockNodeComp::Different;

  /// Compare indices
  if (A.indices.size() != B.indices.size())
    return DatablockNodeComp::BaseAlias;

  if (!std::equal(A.indices.begin(), A.indices.end(), B.indices.begin()))
    return DatablockNodeComp::BaseAlias;

  /// Cache the result.
  A.duplicates.insert(B.id);
  B.duplicates.insert(A.id);
  return DatablockNodeComp::Equal;
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

} // namespace arts
} // namespace mlir
