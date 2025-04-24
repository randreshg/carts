///==========================================================================
/// File: DataBlockAnalysis.cpp
///==========================================================================

#include "arts/Analysis/DataBlockAnalysis.h"
/// Dialects
#include "arts/Analysis/LoopAnalysis.h"
#include "mlir/Dialect/Affine/Analysis/AffineStructures.h"
#include "mlir/Dialect/Affine/Analysis/LoopAnalysis.h"
#include "mlir/Dialect/Affine/Analysis/Utils.h"
#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/Operation.h"
#include "mlir/IR/Value.h"
#include "mlir/Support/LLVM.h"
#include "mlir/Transforms/DialectConversion.h"
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
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"
#include <csignal>
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
using namespace mlir::arts;

//===----------------------------------------------------------------------===//
// DatablockNode
//===----------------------------------------------------------------------===//
DatablockNode::DatablockNode(DatablockGraph *DG, arts::DataBlockOp dbOp)
    : op(dbOp), DG(DG), DA(DG->DA) {
  if (!op)
    return;
  collectInfo();
}

void DatablockNode::collectInfo() {
  id = DG->nodes.size();
  mode = op.getMode();
  ptr = op.getPtr();
  indices = op.getIndices();
  sizes = op.getSizes();
  elementType = op.getElementType();
  elementTypeSize = op.getElementTypeSize();
  resultType = op.getResult().getType();

  /// Add 'hasSingleSize' attribute if the datablock has a single size of 1 or
  /// no size
  hasSingleSize = false;
  if (sizes.empty()) {
    op.setHasSingleSize();
    hasSingleSize = true;
  } else if (sizes.size() == 1) {
    if (auto cstOp = sizes[0].getDefiningOp<arith::ConstantIndexOp>()) {
      if (cstOp.value() == 1) {
        op.setHasSingleSize();
        hasSingleSize = true;
      }
    }
  }

  /// Add 'hasPtrDb' attribute if the base is a datablock.
  if (auto parentDb =
          dyn_cast_or_null<arts::DataBlockOp>(ptr.getDefiningOp())) {
    op.setHasPtrDb();
    hasPtrDb = true;
    parent = DG->getNode(parentDb);
    for (auto &eff : parent->effects)
      effects.push_back(eff);

    hasGuid = (!parent->hasSingleSize || indices.empty());
    if (hasGuid)
      op.setHasGuid();
  } else {
    hasPtrDb = false;
    /// Collect memory effects if the parent is not a datablock.
    if (auto memEff = dyn_cast<MemoryEffectOpInterface>(op.getOperation()))
      memEff.getEffects(effects);
  }

  /// Try to get the EDT parent of the datablock.
  if (auto parentOp = op->getParentOfType<arts::EdtOp>())
    edtParent = parentOp;

  /// Collect and analyze uses of the datablock.
  collectUses();

  /// Compute the region of the datablock.
  // computeRegion();
}

void DatablockNode::collectUses() {
  unsigned numUses = 0;
  /// If the only user is an EDT, it is because the datablock hasn't been
  /// rewired yet.
  auto opResult = op.getResult();
  if (op->hasOneUse() && isa<arts::EdtOp>(op->use_begin()->getOwner())) {
    llvm::errs() << "Datablock " << op << " hasn't been rewired yet\n";
    assert(false && "Datablock not rewired");
    return;
  }

  /// Analyze uses
  auto *dbBlock = op->getBlock();
  for (auto &use : opResult.getUses()) {
    ++numUses;

    /// Set EDT user and position.
    Operation *userOp = use.getOwner();
    if (userOp->getBlock() != dbBlock)
      continue;
    if (auto edtOp = dyn_cast<arts::EdtOp>(userOp)) {
      assert(!edtUser && "Multiple EDT users");
      edtUser = edtOp;
      auto deps = edtUser.getDependencies();
      auto it = std::find_if(deps.begin(), deps.end(), [this](auto dep) {
        return dep.getDefiningOp() == op;
      });
      userEdtPos = (it != deps.end()) ? std::distance(deps.begin(), it) : 0;
    }

    /// Collect loads/stores
    if (isa<memref::LoadOp, memref::StoreOp, AffineReadOpInterface,
            AffineWriteOpInterface>(userOp))
      loadsAndStores.push_back(userOp);
  }
  useCount = numUses;
}

bool DatablockNode::computeRegion() {
  auto computeRegionInner = [&](Operation *useOp,
                                FlatLinearValueConstraints &cst) {
    /// Gather loops
    SmallVector<LoopInfo *, 4> loops;
    DA->loopAnalysis->collectEnclosingLoops(useOp, loops);
    unsigned depth = loops.size();

    /// Affine load/store
    if (isa<AffineReadOpInterface, AffineWriteOpInterface>(useOp)) {
      MemRefRegion region(useOp->getLoc());
      if (succeeded(region.compute(useOp, depth, nullptr, true))) {
        cst = region.cst;
        return true;
      }
      return false;
    }

    /// Generic memref load/store fallback
    Value ptr;
    if (auto ld = dyn_cast<memref::LoadOp>(useOp))
      ptr = ld.getMemRef();
    else if (auto st = dyn_cast<memref::StoreOp>(useOp))
      ptr = st.getMemRef();
    else
      return false;

    auto mt = ptr.getType().cast<MemRefType>();
    unsigned rank = mt.getRank();

    /// Start with no symbols/locals
    cst = FlatLinearValueConstraints(rank, /*numSymbols=*/0, /*numLocals=*/0);

    /// Append dimension variables matching memref rank
    SmallVector<Value> noVals;
    for (unsigned d = 0; d < rank; d++)
      cst.appendDimVar(noVals);

    /// static & dynamic bounds
    for (unsigned d = 0; d < rank; ++d) {
      /// 0 <= dim
      cst.addBound(presburger::BoundType::LB, d, 0);
      if (!mt.isDynamicDim(d)) {
        /// dim <= size-1
        cst.addBound(presburger::BoundType::UB, d, mt.getDimSize(d) - 1);
      } else {
        /// dynamic: use memref.dim to bound
        unsigned sym = cst.appendSymbolVar({sizes[d]});
        /// var <= sym-1  <=>  sym - var >= 1
        SmallVector<int64_t, 8> coeffs(cst.getNumCols(), 0);
        coeffs[sym] = 1;
        coeffs[d] = -1;
        coeffs[cst.getNumCols() - 1] = 1; // constant term
        cst.addInequality(coeffs);
      }
    }
    return true;
  };

  /// 1. For affine operations uses exact region analysis
  /// 2. For regular memref operations: Uses a union bounding box approach to
  ///    approximate the access regions
  FlatAffineValueConstraints aggCst;
  bool haveRegion = false;
  // auto builder = OpBuilder(op);

  for (Operation *user : loadsAndStores) {
    FlatAffineValueConstraints cst;
    if (computeRegionInner(user, cst)) {
      if (!haveRegion) {
        aggCst = std::move(cst);
        haveRegion = true;
      } else {
        if (!succeeded(aggCst.unionBoundingBox(cst)))
          llvm_unreachable("unionBoundingBox failed");
      }
    }
    LLVM_DEBUG(dbgs() << "DatablockNode::computeRegion: user: " << *user
                      << " region: ");
    LLVM_DEBUG(aggCst.print(dbgs()));
    if (!haveRegion)
      continue;

    /// Build `bounds` and `symbols`
    // SmallVector<Attribute> boundsArr, symArr;
    // SmallVector<Value> values;
    // aggCst.getValues(0, aggCst.getNumSymbolVars(), &values);
    // for (Value sym : values)
    //   symArr.push_back(SymbolRefAttr::get(sym.getDefiningOp()));
    // for (unsigned d = 0; d < aggCst.getNumDimVars(); ++d) {
    //   auto lb = aggCst.getConstantBound(presburger::BoundType::LB, d);
    //   auto ub = aggCst.getConstantBound(presburger::BoundType::UB, d);
    //   Attribute lbA =
    //       lb ? builder.getIntegerAttr(builder.getIndexType(), *lb) :
    //       symArr.front();
    //   Attribute ubA =
    //       ub ? IntegerAttr::get(builder.getIndexType(), *ub) :
    //       symArr.front();
    //   boundsArr.push_back(builder.getArrayAttr({lbA, ubA}));
    // }

    // // Per‐loop info
    // SmallVector<Attribute> loopInfoArr;
    // bool anyReduction = false;
    // SmallVector<LoopInfo, 4> loops;
    // collectEnclosingLoops(db, loops);

    // for (auto &info : loops) {
    //   bool parallel = false;
    //   std::optional<int64_t> trip;
    //   bool invariant = true;
    //   bool strided = false;

    //   // Invariance: if any access depends on this loop's IV, mark false
    //   for (Operation *user : users) {
    //     if (!isa<AffineReadOpInterface, AffineWriteOpInterface>(user))
    //       continue;
    //     if (info.isAffine && !affine::isInvariantAccess(info.affine, user))
    //       invariant = false;
    //   }

    //   // Build loop dict
    //   // SmallVector<NamedAttribute> fields;
    //   // fields.push_back(
    //   //     builder.getNamedAttr("depth",
    //   //     builder.getI64IntegerAttr(info.depth)));
    //   // fields.push_back(
    //   //     builder.getNamedAttr("parallel",
    //   builder.getBoolAttr(parallel)));
    //   // fields.push_back(
    //   //     builder.getNamedAttr("invariant",
    //   builder.getBoolAttr(invariant)));
    //   // fields.push_back(
    //   //     builder.getNamedAttr("strided", builder.getBoolAttr(strided)));
    //   // if (trip)
    //   //   fields.push_back(builder.getNamedAttr(
    //   //       "trip_count", builder.getI64IntegerAttr(*trip)));
    //   // else
    //   //   fields.push_back(
    //   //       builder.getNamedAttr("trip_count",
    //   builder.getStringAttr("?")));

    //   // loopInfoArr.push_back(builder.getDictionaryAttr(fields));
    // }

    // Attach DomainAttr (version=2, kind="exact")
    // auto domain = arts::DomainAttr::get(
    //     builder.getArrayAttr(boundsArr), builder.getArrayAttr(symArr),
    //     builder.getArrayAttr(loopInfoArr), builder.getBoolAttr(anyReduction),
    //     builder.getStringAttr("exact"), builder.getI64IntegerAttr(2));
    // db->setAttr("arts.domain", domain);
  }
  return true;
}

//===----------------------------------------------------------------------===//
// DatablockGraph
//===----------------------------------------------------------------------===//
DatablockGraph::DatablockGraph(func::FuncOp func, DatablockAnalysis *DA)
    : func(func), DA(DA) {
  assert(func && "Function cannot be null");
  assert(DA && "DatablockAnalysis cannot be null");

  /// Create entry Node:
  {
    entryDbNode = new DatablockNode(this, nullptr);
    entryDbNode->id = 0;
    entryDbNode->ptr = nullptr;
    nodes.push_back(entryDbNode);
  }

  collectNodes(func.getBody());
  if (nodes.empty())
    return;

  /// Build the datablock graph from the function body.
  build();
}

SetVector<unsigned> DatablockGraph::getProducers() const {
  SetVector<unsigned> producers;
  for (auto entry : edges)
    producers.insert(entry.first);
  return producers;
}

SetVector<unsigned> DatablockGraph::getConsumers(unsigned producerID) {
  return edges[producerID];
}

bool DatablockGraph::addEdge(DatablockNode &prod, DatablockNode &cons) {
  /// If an edge from prod to cons already exists, do nothing.
  auto prodIt = edges.find(prod.id);
  if (prodIt != edges.end()) {
    if (prodIt->second.count(cons.id))
      return false;
  }
  edges[prod.id].insert(cons.id);
  return true;
}

static int analysisDepth = 0;
struct IndentScope {
  IndentScope() { ++analysisDepth; }
  ~IndentScope() { --analysisDepth; }
};

void DatablockGraph::build() {
  LLVM_DEBUG(dbgs() << std::string(analysisDepth * 2, ' ')
                    << "- Building datablock graph for function: "
                    << func.getName() << "\n");
  /// Process the function body region with the initial environment.
  Environment env;
  processRegion(func.getBody(), env);
  LLVM_DEBUG(dbgs() << std::string(analysisDepth * 2, ' ')
                    << "Finished building datablock graph for function: "
                    << func.getName() << "\n");
}

std::pair<Environment, bool> DatablockGraph::processRegion(Region &region,
                                                           Environment &env) {
  Environment newEnv = env;
  bool changed = false;
  for (Operation &op : region.getOps()) {
    IndentScope scope;
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

    /// Merge the new environment with the current one.
    newEnv = mergeEnvironments(newEnv, result.first);
    changed = changed || result.second;
  }
  LLVM_DEBUG(dbgs() << std::string(analysisDepth * 2, ' ')
                    << " - Finished processing region. Environment changed: "
                    << (changed ? "true" : "false") << "\n");
  return {newEnv, changed};
}

std::pair<Environment, bool> DatablockGraph::processEdt(arts::EdtOp edtOp,
                                                        Environment &env) {
  static int edtCount = 0;
  LLVM_DEBUG(dbgs() << std::string(analysisDepth * 2, ' ')
                    << "- Processing EDT #" << edtCount++ << "\n");
  /// Process the inputs and outputs of the EDT.
  auto edtDeps = edtOp.getDependencies();
  bool changed = false;
  Environment newEnv = env;

  /// Handle input dependencies.
  if (!edtDeps.empty()) {
    LLVM_DEBUG({
      dbgs() << std::string(analysisDepth * 2, ' ')
             << " Initial environment: {";
      for (auto entry : newEnv) {
        dbgs() << "  #" << getNode(entry.first)->id << " -> #"
               << entry.second->id << ",";
      }
      dbgs() << "}\n";
    });

    /// Reserve dbIns and dbOuts for input and output datablocks.
    SmallVector<DatablockNode *, 4> dbIns, dbOuts;
    dbIns.reserve(edtDeps.size());
    dbOuts.reserve(edtDeps.size());

    /// Iterate over the dependencies to fill dbIns and dbOuts.
    for (auto dep : edtDeps) {
      auto db = dyn_cast<DataBlockOp>(dep.getDefiningOp());
      assert(db && "Dependency must be a datablock");
      // Retrieve the datablock node and categorize based on mode.
      auto *dbNode = getNode(db);
      if (dbNode->isWriter())
        dbOuts.push_back(dbNode);
      if (dbNode->isReader())
        dbIns.push_back(dbNode);
    }

    /// Process input datablocks.
    LLVM_DEBUG(dbgs() << std::string(analysisDepth * 2, ' ')
                      << "  Processing EDT inputs\n");
    for (auto *dbIn : dbIns) {
      LLVM_DEBUG(dbgs() << std::string(analysisDepth * 2, ' ')
                        << "  - Examining DB #" << dbIn->id << " as input\n");
      auto prodDefs = findDefinition(*dbIn, newEnv);
      if (prodDefs.empty()) {
        LLVM_DEBUG(dbgs() << std::string(analysisDepth * 2, ' ')
                          << "    - No previous definition for DB #" << dbIn->id
                          << ", add edge from entry node\n");
        addEdge(*entryDbNode, *dbIn);
        continue;
      }
      LLVM_DEBUG(dbgs() << std::string(analysisDepth * 2, ' ') << "Found "
                        << prodDefs.size() << " definitions for DB #"
                        << dbIn->id << "\n");
      /// Analyze dependencies from producer definitions.
      for (auto &prodDef : prodDefs) {
        bool isDirect = false;
        if (!DA->mayDepend(*prodDef, *dbIn, isDirect))
          continue;
        LLVM_DEBUG(dbgs() << std::string(analysisDepth * 2, ' ')
                          << "Adding edge from DB #" << prodDef->id
                          << " to DB #" << dbIn->id << "\n");
        bool res = addEdge(*prodDef, *dbIn);
        if (res) {
          LLVM_DEBUG(dbgs() << std::string(analysisDepth * 2, ' ')
                            << "Edge added successfully\n");
        } else {
          LLVM_DEBUG(dbgs() << std::string(analysisDepth * 2, ' ')
                            << "Edge already exists, skipping\n");
        }
      }
    }

    /// Process output datablocks.
    LLVM_DEBUG(dbgs() << std::string(analysisDepth * 2, ' ')
                      << "  Processing EDT outputs\n");
    for (auto *dbOut : dbOuts) {
      LLVM_DEBUG(dbgs() << std::string(analysisDepth * 2, ' ')
                        << "  - Examining DB #" << dbOut->id << " as output\n");
      auto prodDefs = findDefinition(*dbOut, newEnv);
      if (prodDefs.empty()) {
        LLVM_DEBUG(dbgs() << std::string(analysisDepth * 2, ' ')
                          << "    - No previous definition for DB #"
                          << dbOut->id
                          << ", updating environment with new definition\n");
        newEnv[dbOut->op] = dbOut;
        changed |= true;
        continue;
      }
      for (auto &prodDef : prodDefs) {
        bool isDirect = false;
        if (!DA->mayDepend(*prodDef, *dbOut, isDirect))
          continue;
        LLVM_DEBUG(dbgs() << std::string(analysisDepth * 2, ' ')
                          << "Updating environment: DB #" << dbOut->id
                          << " now defined from DB #" << prodDef->id << "\n");
        newEnv[prodDef->op] = dbOut;
        changed |= true;
      }
    }
  }

  /// Process the EDT body region.
  LLVM_DEBUG(dbgs() << std::string(analysisDepth * 2, ' ')
                    << "  Processing EDT body region\n");
  {
    IndentScope bodyScope;
    Environment newEdtEnv;
    processRegion(edtOp.getBody(), newEdtEnv);
    // TODO: Integrate parent/child environment relationships if needed.
  }

  LLVM_DEBUG(dbgs() << std::string(analysisDepth * 2, ' ')
                    << "Finished processing EDT. Environment changed: "
                    << (changed ? "true" : "false") << "\n");
  return {newEnv, changed};
}

std::pair<Environment, bool> DatablockGraph::processFor(scf::ForOp forOp,
                                                        Environment &env) {
  LLVM_DEBUG(dbgs() << std::string(analysisDepth * 2, ' ')
                    << "- Processing ForOp loop\n");
  Environment loopEnv = env;
  bool overallChanged = false;
  size_t prevEdgeCount = edges.size();
  unsigned iterationCount = 0;
  constexpr unsigned maxIterations = 5;

  // Iterate until a fixed-point is reached or maximum iterations are hit.
  while (true) {
    ++iterationCount;
    // Process the loop body region.
    auto bodyResult = processRegion(forOp->getRegion(0), loopEnv);
    Environment newLoopEnv = mergeEnvironments(loopEnv, bodyResult.first);
    overallChanged |= bodyResult.second;
    size_t currEdgeCount = edges.size();

    // Log the current iteration and edge count.
    LLVM_DEBUG(dbgs() << std::string(analysisDepth * 2, ' ') << "  - Iteration "
                      << iterationCount << " completed: edges=" << currEdgeCount
                      << "\n");

    /// Break if no changes were made and the edge count is stable.
    if (!bodyResult.second && (currEdgeCount == prevEdgeCount))
      break;

    /// If maximum iterations reached without new edges, exit loop.
    if (iterationCount >= maxIterations && (currEdgeCount == prevEdgeCount))
      break;

    // Prepare for next iteration.
    loopEnv = std::move(newLoopEnv);
    prevEdgeCount = currEdgeCount;
    LLVM_DEBUG(
        dbgs() << std::string(analysisDepth * 2, ' ')
               << "  - Loop environment updated, iterating fixed-point...\n");
  }
  LLVM_DEBUG(dbgs() << std::string(analysisDepth * 2, ' ')
                    << "- Finished processing ForOp loop\n");
  return {loopEnv, overallChanged};
}

std::pair<Environment, bool> DatablockGraph::processIf(scf::IfOp ifOp,
                                                       Environment &env) {
  LLVM_DEBUG(dbgs() << std::string(analysisDepth * 2, ' ')
                    << "- Processing IfOp with then and else regions\n");
  auto thenRes = processRegion(ifOp.getThenRegion(), env);
  auto elseRes = processRegion(ifOp.getElseRegion(), env);
  Environment merged = mergeEnvironments(thenRes.first, elseRes.first);
  bool changed = thenRes.second || elseRes.second;
  LLVM_DEBUG(dbgs() << std::string(analysisDepth * 2, ' ')
                    << "  - IfOp regions merged. Environment changed: "
                    << (changed ? "true" : "false") << "\n");
  return {merged, changed};
}

std::pair<Environment, bool> DatablockGraph::processCall(func::CallOp callOp,
                                                         Environment &env) {
  LLVM_DEBUG(dbgs() << std::string(analysisDepth * 2, ' ')
                    << "- Processing CallOp (ignoring for now)\n");
  // TODO: Expand call handling logic for inter-procedural analysis
  return {env, false};
}

SmallVector<DatablockNode *, 4>
DatablockGraph::findDefinition(DatablockNode &dbNode, Environment &env) {
  IndentScope scope;
  LLVM_DEBUG(dbgs() << std::string(analysisDepth * 2, ' ')
                    << "  Searching for definitions for DB #" << dbNode.id
                    << "\n");
  SmallVector<DatablockNode *, 4> defs;
  for (auto pair : env) {
    if (!DA->ptrMayAlias(*pair.second, dbNode))
      continue;
    LLVM_DEBUG(dbgs() << std::string(analysisDepth * 2, ' ')
                      << "  - Potential definition found: DB #"
                      << pair.second->id << "\n");
    /// Pessimistically assume alias implies potential dependency.
    defs.push_back(pair.second);
  }
  return defs;
}

Environment DatablockGraph::mergeEnvironments(const Environment &env1,
                                              const Environment &env2) {
  LLVM_DEBUG(dbgs() << std::string(analysisDepth * 2, ' ')
                    << "  Merging environments\n");
  Environment mergedEnv = env1;
  for (auto &pair : env2) {
    auto dbOp = pair.first;
    auto dbNode = pair.second;
    if (mergedEnv.count(dbOp)) {
      LLVM_DEBUG(dbgs() << std::string(analysisDepth * 2, ' ')
                        << "  - DB already exists in merged environment: "
                        << dbNode->id << "\n");
      auto node = getNode(dbOp);
      /// If they belong to the same parent
      if (dbNode->edtParent == node->edtParent) {
        LLVM_DEBUG(dbgs() << std::string(analysisDepth * 2, ' ')
                          << "    - Same EDT parent, "
                          << "updating definition\n");
        mergedEnv[dbOp] = dbNode;
      }
    } else {
      // Add new definition into the merged environment.
      LLVM_DEBUG(dbgs() << std::string(analysisDepth * 2, ' ')
                        << "  - Adding DB #" << dbNode->id
                        << " to merged environment\n");
      mergedEnv[dbOp] = dbNode;
    }
  }
  /// Debug final merged environment.
  LLVM_DEBUG({
    dbgs() << std::string(analysisDepth * 2, ' ')
           << "  - Final merged environment: {";
    for (auto &pair : mergedEnv) {
      dbgs() << " #" << getNode(pair.first)->id << " -> #" << pair.second->id
             << ",";
    }
    dbgs() << "}\n";
  });
  return mergedEnv;
}

void DatablockGraph::print() {
  std::string info;
  llvm::raw_string_ostream os(info);
  os << "Nodes:\n";
  for (auto *node : nodes) {
    /// Skip the entry node
    if (node == entryDbNode)
      continue;

    /// Print the node information
    auto &n = *node;
    os << "  #" << n.id << " " << n.mode << "\n";
    os << "    " << n.op << "\n";
    os << " useCount=" << n.useCount;
    os << " hasPtrDb=" << (n.hasPtrDb ? "true" : "false");
    os << " userEdtPos=" << n.userEdtPos;
    os << (n.parent ? " parent=#" + std::to_string(n.parent->id) : "") << "\n";
  }
  os << "Edges:\n";
  if (edges.empty()) {
    os << "  No edges\n";
    LLVM_DEBUG(dbgs() << os.str());
    return;
  }

  /// Print the edges for each producer node.
  for (auto &entry : edges) {
    unsigned producer = entry.first;
    for (auto consumer : entry.second) {
      os << "  #" << producer << " -> #" << consumer << "\n";
    }
  }
  os << "Total nodes: " << nodes.size() << "\n";
  LLVM_DEBUG(dbgs() << os.str());
}

void DatablockGraph::computeStatistics() {
  if (nodes.empty())
    return;
  unsigned totalUses = 0;
  unsigned totalRegions = 0;
  for (auto *node : nodes) {
    totalUses += node->useCount;
    // totalRegions += node.userRegions.size();
  }
  double avgUses = (double)totalUses / nodes.size();
  double avgRegions = (double)totalRegions / nodes.size();
  LLVM_DEBUG(DBGS() << "Average use count per datablock: " << avgUses << "\n"
                    << "Average region frequency per datablock: " << avgRegions
                    << "\n");
}

void DatablockGraph::collectNodes(Region &region) {
  /// Collect datablock nodes from the top-level region of a function.
  unsigned estimatedCount = 0;
  region.walk<mlir::WalkOrder::PreOrder>(
      [&](arts::DataBlockOp) { ++estimatedCount; });
  nodes.reserve(estimatedCount);
  region.walk<mlir::WalkOrder::PreOrder>([&](arts::DataBlockOp dbOp) {
    LLVM_DEBUG(dbgs() << "Datablock node #" << nodes.size() << ": " << dbOp
                      << "\n");
    /// Add the node to the graph.
    DatablockNode *dbNode = new DatablockNode(this, dbOp);
    assert(dbNode && "Failed to allocate DatablockNode");
    nodes.push_back(dbNode);
    nodeMap[dbOp] = dbNode;
  });
}

void DatablockGraph::fuseAdjacentNodes() {}

void DatablockGraph::detectOutOnlyNodes() {}

void DatablockGraph::deduplicateNodes() {}

bool DatablockGraph::isOnlyDependentOnEntry(DatablockNode &node) {
  for (auto &entry : edges) {
    if (entry.first == entryDbNode->id)
      continue;
    if (entry.first == node.id)
      continue;
    if (entry.second.count(node.id))
      return false;
  }
  return true;
}

//===----------------------------------------------------------------------===//
// DatablockAnalysis
//===----------------------------------------------------------------------===//
DatablockAnalysis::DatablockAnalysis(Operation *module) {
  ModuleOp mod = cast<ModuleOp>(module);
  loopAnalysis = new LoopAnalysis(mod);
  mod->walk([&](func::FuncOp func) { getOrCreateGraph(func); });
}

DatablockAnalysis::~DatablockAnalysis() {
  // LLVM_DEBUG(DBGS() << "Deleting DatablockAnalysis\n");
  for (auto &pair : functionGraphMap)
    delete pair.second;
  functionGraphMap.clear();
  delete loopAnalysis;
}

/// Public interface
void DatablockAnalysis::printGraph(func::FuncOp func) {
  LLVM_DEBUG(dbgs() << line);
  if (functionGraphMap.count(func)) {
    LLVM_DEBUG(DBGS() << "Printing graph for function: " << func.getName()
                      << "\n");
    functionGraphMap[func]->print();
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

/// Dependency analysis.
bool DatablockAnalysis::mayDepend(DatablockNode &prod, DatablockNode &cons,
                                  bool &isDirect) {
  /// Verify if they belong to the same EDT parent.
  if (prod.edtParent != cons.edtParent)
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
