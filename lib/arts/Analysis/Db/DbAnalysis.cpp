//===----------------------------------------------------------------------===//
// Db/DbAnalysis.cpp - Implementation of the DbAnalysis class
//===----------------------------------------------------------------------===//

#include "arts/Analysis/Db/DbAnalysis.h"
#include "arts/Analysis/Db/DbDataFlowAnalysis.h"
#include "arts/Analysis/Graphs/Db/DbGraph.h"
#include "arts/Analysis/Graphs/Db/DbNode.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/InferTypeOpInterface.h"
#include "llvm/Support/Debug.h"

#define DEBUG_TYPE "db-analysis"
#define dbgs() llvm::dbgs()
#define DBGS() (dbgs() << "[" DEBUG_TYPE "] ")

using namespace mlir;
using namespace mlir::arts;

DbAnalysis::DbAnalysis(Operation *module) : module(module), solver() {
  LLVM_DEBUG(DBGS() << "Initializing DbAnalysis for module\n");
  loopAnalysis = std::make_unique<LoopAnalysis>(module);
  dbAliasAnalysis = std::make_unique<DbAliasAnalysis>(this);

  // Add data flow analyses to solver
  solver.load<dataflow::DeadCodeAnalysis>();
  solver.load<dataflow::SparseConstantPropagation>();
}

DbAnalysis::~DbAnalysis() { LLVM_DEBUG(DBGS() << "Destroying DbAnalysis\n"); }

DbGraph *DbAnalysis::getOrCreateGraph(func::FuncOp func) {
  LLVM_DEBUG(dbgs() << "Getting or creating DbGraph for function: "
                    << func.getName() << "\n");
  auto it = functionGraphMap.find(func);
  if (it != functionGraphMap.end()) {
    LLVM_DEBUG(dbgs() << "Found existing DbGraph.\n");
    return it->second.get();
  }

  LLVM_DEBUG(dbgs() << "Creating new DbGraph for function: " << func.getName()
                    << "\n");
  auto newGraph = std::make_unique<DbGraph>(func, this);

  // Run data flow analysis to populate the graph
  DbDataFlowAnalysis dataFlow(solver, newGraph.get(), this);
  dataFlow.analyze();

  DbGraph *graphPtr = newGraph.get();
  functionGraphMap[func] = std::move(newGraph);
  return graphPtr;
}

bool DbAnalysis::invalidateGraph(func::FuncOp func) {
  LLVM_DEBUG(dbgs() << "Invalidating DbGraph for function: " << func.getName()
                    << "\n");
  auto it = functionGraphMap.find(func);
  if (it != functionGraphMap.end()) {
    functionGraphMap.erase(it);
    LLVM_DEBUG(dbgs() << "DbGraph invalidated successfully.\n");
    return true;
  }
  LLVM_DEBUG(dbgs() << "DbGraph not found, could not invalidate.\n");
  return false;
}

void DbAnalysis::print(func::FuncOp func) {
  LLVM_DEBUG(dbgs() << "Printing DbGraph for function: " << func.getName()
                    << "\n");
  DbGraph *graph = getOrCreateGraph(func);
  if (graph) {
    graph->print(dbgs());
  } else {
    LLVM_DEBUG(dbgs() << "Error: Could not get or create DbGraph for printing "
                      << "function " << func.getName() << "\n");
  }
}

SmallVector<int64_t> DbAnalysis::getComputedDimSizes(const NodeBase &node) {
  Operation *op = node.getOp();
  SmallVector<int64_t> sizes;

  // Base case for alloc: Assume DbAllocOp has ShapedType
  if (auto shapedType = dyn_cast<ShapedType>(op->getResult(0).getType())) {
    sizes = SmallVector<int64_t>(shapedType.getShape().begin(), shapedType.getShape().end());
  }

  // For acquire/release, inherit from parent alloc
  if (isa<DbAcquireNode>(node) || isa<DbReleaseNode>(node)) {
    auto parent = isa<DbAcquireNode>(node) ? dyn_cast<DbAcquireNode>(&node)->getParent() : dyn_cast<DbReleaseNode>(&node)->getParent();
    if (parent) {
      return getComputedDimSizes(*parent);
    }
  }

  // Infer if dynamic
  if (auto inferOp = dyn_cast<InferShapedTypeOpInterface>(op)) {
    SmallVector<ShapedTypeComponents> inferredShapes;
    if (succeeded(inferOp.inferReturnTypeComponents(op->getContext(), op->getLoc(), op->getOperands(),
                                                    op->getAttrDictionary(), op->getPropertiesStorage(),
                                                    op->getRegions(), inferredShapes))) {
      if (!inferredShapes.empty()) {
        sizes = inferredShapes[0].getDims();
      }
    }
  }

  // If still unknown, use -1 for dynamic
  if (sizes.empty()) {
    sizes.assign(1, -1);  // Default to unknown
  }

  return sizes;
}

SmallVector<DimensionAnalysis> DbAnalysis::getDimensionAnalysis(const NodeBase &node) {
  SmallVector<DimensionAnalysis> analysis;

  // Placeholder logic: Analyze patterns based on op attributes or nested ops
  // Assume ops have "pattern" attr or derive from loops
  Operation *op = node.getOp();
  SmallVector<int64_t> sizes = getComputedDimSizes(node);

  for (size_t i = 0; i < sizes.size(); ++i) {
    DimensionAnalysis dim;
    dim.overallPattern.pattern = ComplexExpr::Constant;  // Default
    // TODO: Analyze nested loops or attrs for sequential/strided/blocked patterns.
    analysis.push_back(dim);
  }

  // For acquire/release, add access-specific patterns (e.g., read/write)
  if (isa<DbAcquireNode>(node)) {
    // e.g., set read pattern
  } else if (isa<DbReleaseNode>(node)) {
    // e.g., set write pattern
  }

  return analysis;
}