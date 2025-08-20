//===----------------------------------------------------------------------===//
// Db/DbAnalysis.cpp - Implementation of the DbAnalysis class
//===----------------------------------------------------------------------===//

#include "arts/Analysis/Db/DbAnalysis.h"
#include "arts/Analysis/Db/DbDataFlowAnalysis.h"
#include "arts/Analysis/Db/DbAliasAnalysis.h"
#include "arts/Analysis/LoopAnalysis.h"
#include "arts/Analysis/Graphs/Db/DbGraph.h"
#include "arts/Analysis/Graphs/Db/DbNode.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Interfaces/InferTypeOpInterface.h"
#include "llvm/Support/Debug.h"

#include "arts/Utils/ArtsDebug.h"
ARTS_DEBUG_SETUP(db_analysis);

using namespace mlir;
using namespace mlir::arts;

DbAnalysis::DbAnalysis(Operation *module) : module(module), solver() {
  ARTS_INFO("Initializing DbAnalysis for module");
  loopAnalysis = std::make_unique<LoopAnalysis>(module);
  dbAliasAnalysis = std::make_unique<DbAliasAnalysis>(this);
  
  // Create the dataflow analysis with the solver
  dbDataFlowAnalysis = std::make_unique<DbDataFlowAnalysis>(solver);
  
  // Load the dataflow analysis into the solver
  (void)solver.load<DbDataFlowAnalysis>();

  // Add baseline data flow analyses to solver
  // solver.load<dataflow::DeadCodeAnalysis>();
  // solver.load<dataflow::SparseConstantPropagation>();
}

DbAnalysis::~DbAnalysis() { ARTS_INFO("Destroying DbAnalysis"); }

DbGraph *DbAnalysis::getOrCreateGraph(func::FuncOp func) {
  ARTS_INFO("Getting or creating DbGraph for function: " << func.getName());
  auto it = functionGraphMap.find(func);
  if (it != functionGraphMap.end()) {
    ARTS_INFO("Found existing DbGraph.");
    return it->second.get();
  }

  ARTS_INFO("Creating new DbGraph for function: " << func.getName());
  auto newGraph = std::make_unique<DbGraph>(func, this);
  
  // Initialize the dataflow analysis with the graph and analysis context
  dbDataFlowAnalysis->initialize(newGraph.get(), this);
  
  // Build nodes and dependencies (runs dataflow internally)
  newGraph->build();

  DbGraph *graphPtr = newGraph.get();
  functionGraphMap[func] = std::move(newGraph);
  return graphPtr;
}

// ---- Future/placeholder: slice info and overlap (constants only) ----
DbAnalysis::SliceInfo DbAnalysis::computeSliceInfo(arts::DbAcquireOp acquire) {
  SliceInfo info;
  auto sizes = acquire.getSizes();
  auto offsets = acquire.getOffsets();
  info.sizes.reserve(sizes.size());
  info.offsets.reserve(offsets.size());
  bool hasUnknown = false;
  for (Value v : offsets) {
    if (auto c = v.getDefiningOp<mlir::arith::ConstantIndexOp>())
      info.offsets.push_back(c.value());
    else { info.offsets.push_back(INT64_MIN); hasUnknown = true; }
  }
  uint64_t totalElems = 1;
  for (Value v : sizes) {
    if (auto c = v.getDefiningOp<mlir::arith::ConstantIndexOp>()) {
      info.sizes.push_back(c.value());
      totalElems *= (uint64_t)std::max<int64_t>(c.value(), 1);
    } else { info.sizes.push_back(INT64_MIN); hasUnknown = true; }
  }
  if (!hasUnknown) {
    // Try element type size from result memref
    if (auto memTy = dyn_cast<MemRefType>(acquire.getView().getType())) {
      if (auto intTy = dyn_cast<IntegerType>(memTy.getElementType()))
        info.estimatedBytes = (intTy.getWidth() / 8u) * totalElems;
      else if (auto fTy = dyn_cast<FloatType>(memTy.getElementType()))
        info.estimatedBytes = (fTy.getWidth() / 8u) * totalElems;
    }
  }
  return info;
}

DbAnalysis::OverlapKind DbAnalysis::estimateOverlap(arts::DbAcquireOp a,
                                                    arts::DbAcquireOp b) {
  SliceInfo sa = computeSliceInfo(a);
  SliceInfo sb = computeSliceInfo(b);
  size_t r = std::min(sa.sizes.size(), sb.sizes.size());
  bool anyUnknown = false;
  bool disjoint = false;
  bool equal = (sa.sizes == sb.sizes) && (sa.offsets == sb.offsets);
  for (size_t i = 0; i < r; ++i) {
    int64_t ao = sa.offsets[i], asz = sa.sizes[i];
    int64_t bo = sb.offsets[i], bsz = sb.sizes[i];
    if (ao == INT64_MIN || asz == INT64_MIN || bo == INT64_MIN || bsz == INT64_MIN) {
      anyUnknown = true; continue;
    }
    int64_t a0 = ao, a1 = ao + asz;
    int64_t b0 = bo, b1 = bo + bsz;
    bool overlap = !(a1 <= b0 || b1 <= a0);
    if (!overlap) { disjoint = true; break; }
  }
  if (disjoint) return OverlapKind::Disjoint;
  if (equal) return OverlapKind::Full;
  if (anyUnknown) return OverlapKind::Unknown;
  return OverlapKind::Partial;
}

bool DbAnalysis::invalidateGraph(func::FuncOp func) {
  ARTS_INFO("Invalidating DbGraph for function: " << func.getName());
  auto it = functionGraphMap.find(func);
  if (it != functionGraphMap.end()) {
    functionGraphMap.erase(it);
  ARTS_INFO("DbGraph invalidated successfully.");
    return true;
  }
  ARTS_WARN("DbGraph not found, could not invalidate.");
  return false;
}

void DbAnalysis::print(func::FuncOp func) {
  ARTS_INFO("Printing DbGraph for function: " << func.getName());
  DbGraph *graph = getOrCreateGraph(func);
  if (graph) {
    graph->print(ARTS_DBGS());
  } else {
    ARTS_ERROR("Could not get or create DbGraph for printing function "
               << func.getName());
  }
}

// DISABLED: Shape inference method
/*
SmallVector<int64_t> DbAnalysis::getComputedDimSizes(const NodeBase &node) {
  Operation *op = node.getOp();
  SmallVector<int64_t> sizes;

  // Base case for alloc: Assume DbAllocOp has ShapedType
  if (auto shapedType = dyn_cast<ShapedType>(op->getResult(0).getType())) {
    sizes = SmallVector<int64_t>(shapedType.getShape().begin(),
shapedType.getShape().end());
  }

  // For acquire/release, inherit from parent alloc
  if (isa<DbAcquireNode>(node) || isa<DbReleaseNode>(node)) {
    auto parent = isa<DbAcquireNode>(node) ?
dyn_cast<DbAcquireNode>(&node)->getParent() :
dyn_cast<DbReleaseNode>(&node)->getParent(); if (parent) { return
getComputedDimSizes(*parent);
    }
  }

  // Infer if dynamic
  if (auto inferOp = dyn_cast<InferShapedTypeOpInterface>(op)) {
    SmallVector<ShapedTypeComponents> inferredShapes;
    if (succeeded(inferOp.inferReturnTypeComponents(op->getContext(),
op->getLoc(), op->getOperands(), op->getAttrDictionary(),
op->getPropertiesStorage(), op->getRegions(), inferredShapes))) { if
(!inferredShapes.empty()) { sizes = inferredShapes[0].getDims();
      }
    }
  }

  // If still unknown, use -1 for dynamic
  if (sizes.empty()) {
    sizes.assign(1, -1);  // Default to unknown
  }

  return sizes;
}
*/

// DISABLED: Dimension analysis method
/*
SmallVector<DimensionAnalysis> DbAnalysis::getDimensionAnalysis(const NodeBase
&node) { SmallVector<DimensionAnalysis> analysis;

  // Placeholder logic: Analyze patterns based on op attributes or nested ops
  // Assume ops have "pattern" attr or derive from loops
  Operation *op = node.getOp();
  SmallVector<int64_t> sizes = getComputedDimSizes(node);

  for (size_t i = 0; i < sizes.size(); ++i) {
    DimensionAnalysis dim;
    dim.overallPattern.pattern = ComplexExpr::Constant;  // Default
    // TODO: Analyze nested loops or attrs for sequential/strided/blocked
patterns. analysis.push_back(dim);
  }

  // For acquire/release, add access-specific patterns (e.g., read/write)
  if (isa<DbAcquireNode>(node)) {
    // e.g., set read pattern
  } else if (isa<DbReleaseNode>(node)) {
    // e.g., set write pattern
  }

  return analysis;
}
*/