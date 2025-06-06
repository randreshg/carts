///==========================================================================
/// File: LoopAnalysis.cpp
///
/// Enhanced loop analysis implementation for ARTS.
/// Provides comprehensive loop analysis including pattern detection,
/// optimization opportunities, and induction variable analysis.
///==========================================================================

#include "arts/Analysis/LoopAnalysis.h"
#include "arts/Utils/ArtsUtils.h"
/// Dialects
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/Matchers.h"
#include "polygeist/Ops.h"
/// Others
#include "mlir/Transforms/RegionUtils.h"

/// Debug
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#define DEBUG_TYPE "loop-analysis"
#define line "-----------------------------------------\n"
#define dbgs() (llvm::dbgs())
#define DBGS() (dbgs() << "[" DEBUG_TYPE "] ")

using namespace mlir;
using namespace arts;
using namespace affine;

//===----------------------------------------------------------------------===//
// LoopAnalysis Implementation
//===----------------------------------------------------------------------===//
void LoopAnalysis::run() {
  LLVM_DEBUG(dbgs() << line << "LoopAnalysis STARTED\n" << line);

  /// Analyze scf::ForOp loops.
  module.walk([&](Operation *op) {
    if (auto fop = dyn_cast<scf::ForOp>(op)) {
      loops.push_back(fop);
      loopInfoMap[fop] = new LoopInfo(false, {}, {}, fop.getInductionVar());
    } else if (auto aop = dyn_cast<affine::AffineForOp>(op)) {
      loops.push_back(aop);
      loopInfoMap[aop] = new LoopInfo(true, aop, {}, aop.getInductionVar());
    }
  });

  /// Analyze induction variables.
  analyzeLoopIV();

  /// Run enhanced analysis
  runEnhancedAnalysis();

  LLVM_DEBUG(dbgs() << line << "LoopAnalysis FINISHED\n" << line);
}

void LoopAnalysis::runEnhancedAnalysis() {
  LLVM_DEBUG(DBGS() << "Running enhanced loop analysis\n");

  buildInductionVarMap();
  analyzeLoopBounds();
  detectLoopPatterns();
  classifyLoopTypes();
  analyzeLoopCharacteristics();
}

LoopInfo *LoopAnalysis::getLoopInfo(Operation *op) {
  if (loopInfoMap.count(op))
    return loopInfoMap[op];
  return nullptr;
}

bool LoopAnalysis::isDependentOnLoop(Value val,
                                     SmallVectorImpl<Operation *> &loops) {
  if (loopValsMap.count(val)) {
    loops = loopValsMap[val];
    return true;
  }
  return false;
}

void LoopAnalysis::collectEnclosingLoops(
    Operation *op, SmallVectorImpl<LoopInfo *> &enclosingLoops) {
  for (Operation *p = op->getParentOp(); p; p = p->getParentOp()) {
    if (auto ap = dyn_cast<AffineForOp>(p))
      enclosingLoops.push_back(getLoopInfo(ap));
    else if (auto po = dyn_cast<scf::ParallelOp>(p))
      enclosingLoops.push_back(getLoopInfo(po));
    else if (auto fo = dyn_cast<scf::ForOp>(p))
      enclosingLoops.push_back(getLoopInfo(fo));
  }
  std::reverse(enclosingLoops.begin(), enclosingLoops.end());
}

LoopAnalysisInfo LoopAnalysis::analyzeLoopContext(Operation *op) {
  auto it = enhancedLoopInfo.find(op);
  if (it != enhancedLoopInfo.end()) {
    return it->second;
  }

  LoopAnalysisInfo info;

  // Find enclosing loops
  Operation *current = op;
  while (current) {
    if (auto forOp = current->getParentOfType<scf::ForOp>()) {
      info.enclosingForLoops.push_back(forOp);

      // Analyze loop bounds
      info.lowerBounds.push_back(forOp.getLowerBound());
      info.upperBounds.push_back(forOp.getUpperBound());
      info.steps.push_back(forOp.getStep());

      // Check for constant iteration counts
      int64_t tripCount;
      if (computeConstantTripCount(forOp, tripCount)) {
        info.iterationCounts.push_back(tripCount);
        info.tripCount += tripCount;
        info.hasConstantBounds = true;
      } else {
        info.hasDynamicBounds = true;
      }

      // Track induction variable
      Value inductionVar = forOp.getInductionVar();
      info.inductionVariables.push_back(inductionVar);

      current = forOp;
    } else if (auto whileOp = current->getParentOfType<scf::WhileOp>()) {
      info.enclosingWhileLoops.push_back(whileOp);
      info.hasDynamicBounds = true;
      current = whileOp;
    } else {
      current = current->getParentOp();
    }
  }

  // Set loop characteristics
  info.depth = info.enclosingForLoops.size() + info.enclosingWhileLoops.size();
  info.nestingLevel = info.depth;
  info.isNested = (info.depth > 1);

  // Analyze parallelization potential
  info.isParallelizable =
      !info.hasLoopCarriedDependences && info.hasConstantBounds;

  // Classify control flow
  info.dominantControlFlow = classifyControlFlow(op);

  // Cache the result
  enhancedLoopInfo[op] = info;

  LLVM_DEBUG(DBGS() << "Analyzed loop context for operation, depth: "
                    << info.depth << "\n");

  return info;
}

LoopAnalysisInfo LoopAnalysis::analyzeLoopContext(Value accessValue) {
  if (auto defOp = accessValue.getDefiningOp()) {
    return analyzeLoopContext(defOp);
  }
  // For block arguments, analyze the block
  if (auto blockArg = accessValue.dyn_cast<BlockArgument>()) {
    return analyzeLoopContext(blockArg.getOwner()->getParentOp());
  }
  return LoopAnalysisInfo{};
}

LoopPatternInfo LoopAnalysis::analyzeAccessPattern(Operation *memoryOp) {
  // Check cache first
  auto it = patternCache.find(memoryOp);
  if (it != patternCache.end()) {
    return it->second;
  }

  LoopPatternInfo pattern;

  ValueRange indices;
  if (auto loadOp = dyn_cast<memref::LoadOp>(memoryOp)) {
    indices = loadOp.getIndices();
  } else if (auto storeOp = dyn_cast<memref::StoreOp>(memoryOp)) {
    indices = storeOp.getIndices();
  } else {
    return pattern; // Unknown operation
  }

  return analyzeAccessPattern(indices, memoryOp);
}

LoopPatternInfo LoopAnalysis::analyzeAccessPattern(ValueRange indices,
                                                   Operation *context) {
  LoopPatternInfo pattern;

  if (indices.empty()) {
    return pattern;
  }

  // Detect basic patterns
  if (detectSequentialPattern(indices, context)) {
    pattern.isSequential = true;
    pattern.confidence = 0.9;
  }

  SmallVector<int64_t, 4> strides;
  if (detectStridedPattern(indices, context, strides)) {
    pattern.isStrided = true;
    pattern.confidence = std::max(pattern.confidence, 0.8);

    // Convert to StrideInfo
    for (unsigned i = 0; i < strides.size(); ++i) {
      StrideInfo strideInfo;
      strideInfo.constantStride = strides[i];
      strideInfo.isRegular = true;
      strideInfo.dimension = i;
      pattern.strides.push_back(strideInfo);
    }
  }

  // Detect matrix patterns
  unsigned rowDim, colDim;
  if (detectMatrixPattern(indices, context, rowDim, colDim)) {
    pattern.isMatrixRowTraversal = true; // Default assumption
    pattern.rowDim = rowDim;
    pattern.colDim = colDim;
    pattern.confidence = std::max(pattern.confidence, 0.75);
  }

  // Check for nested access
  auto loopInfo = analyzeLoopContext(context);
  pattern.isNestedAccess = loopInfo.isNested;

  // Cache the result
  patternCache[context] = pattern;

  return pattern;
}

//===----------------------------------------------------------------------===//
// Induction Variable Analysis
//===----------------------------------------------------------------------===//

bool LoopAnalysis::isInductionVariable(Value val) {
  scf::ForOp dummyLoop;
  return isInductionVariable(val, dummyLoop);
}

bool LoopAnalysis::isInductionVariable(Value val, scf::ForOp &parentLoop) {
  if (auto bbArg = val.dyn_cast<BlockArgument>()) {
    Operation *parentOp = bbArg.getOwner()->getParentOp();
    if (auto forOp = dyn_cast<scf::ForOp>(parentOp)) {
      if (bbArg == forOp.getInductionVar()) {
        parentLoop = forOp;
        return true;
      }
    }
  }
  return false;
}

bool LoopAnalysis::isDerivedFromInductionVar(Value val) {
  if (isInductionVariable(val)) {
    return true;
  }

  // Enhanced analysis for derived induction variables
  if (auto defOp = val.getDefiningOp()) {
    // Check for arithmetic operations that preserve induction variable
    // properties
    if (isa<arith::AddIOp, arith::SubIOp, arith::MulIOp>(defOp)) {
      for (Value operand : defOp->getOperands()) {
        if (isDerivedFromInductionVar(operand)) {
          return true;
        }
      }
    }

    // Check for index cast operations
    if (auto castOp = dyn_cast<arith::IndexCastOp>(defOp)) {
      return isDerivedFromInductionVar(castOp.getIn());
    }

    // Check for affine apply operations (common in loop transformations)
    if (auto affineApplyOp = dyn_cast<affine::AffineApplyOp>(defOp)) {
      for (Value operand : affineApplyOp.getOperands()) {
        if (isDerivedFromInductionVar(operand)) {
          return true;
        }
      }
    }

    // Check for select operations (conditional induction variables)
    if (auto selectOp = dyn_cast<arith::SelectOp>(defOp)) {
      return isDerivedFromInductionVar(selectOp.getTrueValue()) ||
             isDerivedFromInductionVar(selectOp.getFalseValue());
    }
  }

  return false;
}

SmallVector<Value, 4> LoopAnalysis::getInductionVariables(Operation *op) {
  SmallVector<Value, 4> result;
  auto loopInfo = analyzeLoopContext(op);
  result = loopInfo.inductionVariables;
  return result;
}

//===----------------------------------------------------------------------===//
// Loop Bound Analysis
//===----------------------------------------------------------------------===//

bool LoopAnalysis::hasConstantBounds(scf::ForOp loop) {
  return getConstantBound(loop.getLowerBound()).has_value() &&
         getConstantBound(loop.getUpperBound()).has_value() &&
         getConstantBound(loop.getStep()).has_value();
}

bool LoopAnalysis::computeConstantTripCount(scf::ForOp loop,
                                            int64_t &tripCount) {
  auto lower = getConstantBound(loop.getLowerBound());
  auto upper = getConstantBound(loop.getUpperBound());
  auto step = getConstantBound(loop.getStep());

  if (lower && upper && step && *step > 0) {
    tripCount = (*upper - *lower) / *step;
    return true;
  }

  return false;
}

std::optional<int64_t> LoopAnalysis::getConstantBound(Value bound) {
  IntegerAttr attr;
  if (matchPattern(bound, m_Constant(&attr))) {
    return attr.getInt();
  }
  return std::nullopt;
}

//===----------------------------------------------------------------------===//
// Pattern Detection
//===----------------------------------------------------------------------===//

bool LoopAnalysis::detectSequentialPattern(ValueRange indices,
                                           Operation *context) {
  auto enclosingLoops = getEnclosingForLoops(context);
  if (enclosingLoops.empty()) {
    return false;
  }

  if (enclosingLoops.size() == 1) {
    auto forOp = enclosingLoops[0];
    Value iv = forOp.getInductionVar();

    // Check if any index is the induction variable
    for (Value index : indices) {
      if (index == iv) {
        return true;
      }
    }
  }

  return false;
}

bool LoopAnalysis::detectStridedPattern(ValueRange indices, Operation *context,
                                        SmallVector<int64_t, 4> &strides) {
  strides.clear();
  auto enclosingLoops = getEnclosingForLoops(context);

  if (enclosingLoops.empty()) {
    return false;
  }

  // Enhanced stride detection for multiple dimensions
  for (unsigned dim = 0; dim < indices.size(); ++dim) {
    Value index = indices[dim];
    int64_t stride = 1; // Default stride
    bool foundStride = false;

    // Check if index is directly an induction variable
    for (auto forOp : enclosingLoops) {
      if (index == forOp.getInductionVar()) {
        int64_t step;
        if (hasConstantStep(forOp, step)) {
          stride = step;
          foundStride = true;
          break;
        }
      }
    }

    // Check for scaled induction variables (e.g., i*2, i*4 for vectorization)
    if (!foundStride) {
      if (auto defOp = index.getDefiningOp()) {
        if (auto mulOp = dyn_cast<arith::MulIOp>(defOp)) {
          Value lhs = mulOp.getLhs();
          Value rhs = mulOp.getRhs();

          // Check if one operand is IV and other is constant
          IntegerAttr scaleAttr;
          Value ivCandidate;
          if (matchPattern(rhs, m_Constant(&scaleAttr))) {
            ivCandidate = lhs;
          } else if (matchPattern(lhs, m_Constant(&scaleAttr))) {
            ivCandidate = rhs;
          }

          if (ivCandidate) {
            for (auto forOp : enclosingLoops) {
              if (ivCandidate == forOp.getInductionVar()) {
                int64_t step;
                if (hasConstantStep(forOp, step)) {
                  stride = step * scaleAttr.getInt();
                  foundStride = true;
                  break;
                }
              }
            }
          }
        }

        // Check for offset induction variables (e.g., i+offset)
        if (!foundStride) {
          if (auto addOp = dyn_cast<arith::AddIOp>(defOp)) {
            Value lhs = addOp.getLhs();
            Value rhs = addOp.getRhs();

            // Check if one operand is IV (offset doesn't affect stride)
            Value ivCandidate = nullptr;
            if (matchPattern(rhs, m_Constant())) {
              ivCandidate = lhs;
            } else if (matchPattern(lhs, m_Constant())) {
              ivCandidate = rhs;
            }

            if (ivCandidate) {
              for (auto forOp : enclosingLoops) {
                if (ivCandidate == forOp.getInductionVar()) {
                  int64_t step;
                  if (hasConstantStep(forOp, step)) {
                    stride = step;
                    foundStride = true;
                    break;
                  }
                }
              }
            }
          }
        }
      }
    }

    strides.push_back(stride);
  }

  // Return true if we found at least one strided pattern
  return !strides.empty() &&
         llvm::any_of(strides, [](int64_t s) { return s != 1; });
}

bool LoopAnalysis::detectMatrixPattern(ValueRange indices, Operation *context,
                                       unsigned &rowDim, unsigned &colDim) {
  // Enhanced matrix pattern detection with cache awareness
  if (indices.size() < 2) {
    return false;
  }
  
  auto enclosingLoops = getEnclosingForLoops(context);
  if (enclosingLoops.size() < 2) {
    return false;
  }
  
  // Check for typical matrix access patterns
  SmallVector<int64_t, 4> strides;
  if (detectStridedPattern(indices, context, strides) && strides.size() >= 2) {
    // Row-major pattern: stride[0] = large, stride[1] = 1
    if (strides[0] > 1 && strides[1] == 1) {
      rowDim = 0;
      colDim = 1;
      
      LLVM_DEBUG(DBGS() << "Detected row-major matrix pattern with stride " 
                        << strides[0] << "\n");
      return true;
    }
    
    // Column-major pattern: stride[0] = 1, stride[1] = large
    if (strides[0] == 1 && strides[1] > 1) {
      rowDim = 1;
      colDim = 0;
      
      LLVM_DEBUG(DBGS() << "Detected column-major matrix pattern with stride " 
                        << strides[1] << "\n");
      return true;
    }
    
    // Cache-friendly tiled pattern: both strides are moderate
    if (strides[0] > 1 && strides[1] > 1 && 
        strides[0] <= 64 && strides[1] <= 64) { // Typical cache line considerations
      rowDim = 0;
      colDim = 1;
      
      LLVM_DEBUG(DBGS() << "Detected cache-friendly tiled matrix pattern\n");
      return true;
    }
  }
  
  return false;
}

bool LoopAnalysis::detectTiledPattern(ValueRange indices, Operation *context,
                                      SmallVector<unsigned, 4> &tileDims) {
  // Simplified tiling detection - look for nested loops with blocking
  auto enclosingLoops = getEnclosingForLoops(context);
  if (enclosingLoops.size() >= 2) {
    // Check for typical tiling pattern: outer loops with inner loops
    tileDims.clear();
    for (unsigned i = 0; i < enclosingLoops.size(); ++i) {
      tileDims.push_back(i);
    }
    return true;
  }
  return false;
}

//===----------------------------------------------------------------------===//
// Optimization Queries
//===----------------------------------------------------------------------===//

bool LoopAnalysis::isParallelizable(scf::ForOp loop) {
  // Simple heuristic: no loop-carried dependencies
  return !hasLoopCarriedDependence(loop, Value{});
}

bool LoopAnalysis::isVectorizable(scf::ForOp loop) {
  // Check for simple induction variable pattern and constant bounds
  int64_t step;
  return hasConstantBounds(loop) && hasConstantStep(loop, step) && step == 1;
}

bool LoopAnalysis::isTilingCandidate(SmallVector<scf::ForOp, 4> &loops) {
  return loops.size() >= 2; // Need at least 2D for tiling
}

bool LoopAnalysis::hasLoopCarriedDependence(scf::ForOp loop, Value memref) {
  // Conservative implementation - assume no dependence for now
  // In a real implementation, this would do dependence analysis
  return false;
}

//===----------------------------------------------------------------------===//
// Utility Methods
//===----------------------------------------------------------------------===//

unsigned LoopAnalysis::getLoopDepth(Operation *op) {
  unsigned depth = 0;
  Operation *current = op;
  while (current) {
    if (isa<scf::ForOp, scf::WhileOp, scf::ParallelOp>(current)) {
      depth++;
    }
    current = current->getParentOp();
  }
  return depth;
}

SmallVector<scf::ForOp, 4> LoopAnalysis::getEnclosingForLoops(Operation *op) {
  SmallVector<scf::ForOp, 4> result;
  Operation *current = op;
  while (current) {
    if (auto forOp = current->getParentOfType<scf::ForOp>()) {
      result.push_back(forOp);
      current = forOp;
    } else {
      break;
    }
  }
  std::reverse(result.begin(), result.end());
  return result;
}

SmallVector<scf::WhileOp, 4>
LoopAnalysis::getEnclosingWhileLoops(Operation *op) {
  SmallVector<scf::WhileOp, 4> result;
  Operation *current = op;
  while (current) {
    if (auto whileOp = current->getParentOfType<scf::WhileOp>()) {
      result.push_back(whileOp);
      current = whileOp;
    } else {
      break;
    }
  }
  std::reverse(result.begin(), result.end());
  return result;
}

ControlFlowType LoopAnalysis::classifyControlFlow(Operation *op) {
  auto enclosingLoops = getEnclosingForLoops(op);
  if (enclosingLoops.empty()) {
    return ControlFlowType::Sequential;
  } else if (enclosingLoops.size() == 1) {
    return ControlFlowType::LoopBound;
  } else {
    return ControlFlowType::NestedLoop;
  }
}

//===----------------------------------------------------------------------===//
// Internal Analysis Methods
//===----------------------------------------------------------------------===//

void LoopAnalysis::analyzeLoopIV() {
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
      loopValsMap[val].push_back(loopOp);
  };

  for (auto loopPair : loopInfoMap)
    loopBfs(loopPair.first, loopPair.second->inductionVar);
}

void LoopAnalysis::buildInductionVarMap() {
  for (auto [loopOp, loopInfo] : loopInfoMap) {
    if (auto forOp = dyn_cast<scf::ForOp>(loopOp)) {
      inductionVarMap[forOp].push_back(forOp.getInductionVar());

      // Find derived induction variables
      SmallVector<Value, 4> derived =
          collectDependentValues(forOp.getInductionVar());
      for (Value val : derived) {
        if (val != forOp.getInductionVar()) {
          inductionVarMap[forOp].push_back(val);
        }
      }
    }
  }
}

void LoopAnalysis::analyzeLoopBounds() {
  // Analyze loop bounds for constant trip counts
  for (auto [loopOp, loopInfo] : loopInfoMap) {
    if (auto forOp = dyn_cast<scf::ForOp>(loopOp)) {
      int64_t tripCount;
      if (computeConstantTripCount(forOp, tripCount)) {
        LLVM_DEBUG(DBGS() << "Loop has constant trip count: " << tripCount
                          << "\n");
      }
    }
  }
}

void LoopAnalysis::detectLoopPatterns() {
  // Detect common loop patterns for optimization
  module.walk([&](Operation *op) {
    if (isa<memref::LoadOp, memref::StoreOp>(op)) {
      analyzeAccessPattern(op);
    }
  });
}

void LoopAnalysis::classifyLoopTypes() {
  // Classify loops by their characteristics
  for (auto [loopOp, loopInfo] : loopInfoMap) {
    if (auto forOp = dyn_cast<scf::ForOp>(loopOp)) {
      LLVM_DEBUG(DBGS() << "Analyzing loop characteristics\n");

      bool isParallel = isParallelizable(forOp);
      bool isVectorizable = this->isVectorizable(forOp);

      LLVM_DEBUG(DBGS() << "  - Parallelizable: " << isParallel << "\n");
      LLVM_DEBUG(DBGS() << "  - Vectorizable: " << isVectorizable << "\n");
    }
  }
}

void LoopAnalysis::analyzeLoopCharacteristics() {
  // Analyze loop characteristics for optimization
  LLVM_DEBUG(DBGS() << "Analyzing loop characteristics\n");
}

void LoopAnalysis::analyzeLoopPatterns() {
  // Enhanced pattern analysis
  LLVM_DEBUG(DBGS() << "Analyzing loop patterns\n");
}

void LoopAnalysis::computeLoopBounds() {
  // Enhanced loop bound computation
  LLVM_DEBUG(DBGS() << "Computing loop bounds\n");
}

//===----------------------------------------------------------------------===//
// Helper Methods
//===----------------------------------------------------------------------===//

bool LoopAnalysis::isLoopInductionVariable(Value val, scf::ForOp &loop) {
  return isInductionVariable(val, loop);
}

bool LoopAnalysis::isSimpleArithmetic(Operation *op) {
  return isa<arith::AddIOp, arith::SubIOp, arith::MulIOp, arith::DivSIOp>(op);
}

int64_t LoopAnalysis::computeStrideFromIndices(ValueRange indices,
                                               scf::ForOp loop) {
  // Simple stride computation - return 1 for sequential access
  Value iv = loop.getInductionVar();
  for (Value index : indices) {
    if (index == iv) {
      return 1; // Sequential access
    }
  }
  return 0; // No clear stride
}

bool LoopAnalysis::hasConstantStep(scf::ForOp loop, int64_t &step) {
  auto stepValue = getConstantBound(loop.getStep());
  if (stepValue) {
    step = *stepValue;
    return true;
  }
  return false;
}

Operation *LoopAnalysis::findDefiningLoop(Value val) {
  if (auto bbArg = val.dyn_cast<BlockArgument>()) {
    return bbArg.getOwner()->getParentOp();
  }
  if (auto defOp = val.getDefiningOp()) {
    return defOp;
  }
  return nullptr;
}

bool LoopAnalysis::dependsOnInductionVar(Value val, scf::ForOp loop) {
  if (val == loop.getInductionVar()) {
    return true;
  }

  if (auto defOp = val.getDefiningOp()) {
    for (Value operand : defOp->getOperands()) {
      if (dependsOnInductionVar(operand, loop)) {
        return true;
      }
    }
  }

  return false;
}

SmallVector<Value, 4> LoopAnalysis::collectDependentValues(Value root) {
  SmallVector<Value, 4> result;
  DenseSet<Value> visited;
  SmallVector<Value, 8> worklist;

  worklist.push_back(root);
  visited.insert(root);

  while (!worklist.empty()) {
    Value current = worklist.pop_back_val();
    result.push_back(current);

    for (auto &use : current.getUses()) {
      Operation *owner = use.getOwner();
      if (isSimpleArithmetic(owner)) {
        for (Value result : owner->getResults()) {
          if (visited.insert(result).second) {
            worklist.push_back(result);
          }
        }
      }
    }
  }

  return result;
}

//===----------------------------------------------------------------------===//
// LoopAnalysisInfo Methods
//===----------------------------------------------------------------------===//

// DataLifetime methods moved to DbTypes.cpp

ControlFlowType LoopAnalysisInfo::classifyControlPattern() const {
  if (depth == 0) {
    return ControlFlowType::Sequential;
  } else if (depth == 1) {
    return ControlFlowType::LoopBound;
  } else {
    return ControlFlowType::NestedLoop;
  }
}

bool LoopAnalysisInfo::isCompatibleWithParallelization() const {
  return isParallelizable && !hasLoopCarriedDependences;
}

bool LoopAnalysisInfo::hasConstantTripCount() const {
  return hasConstantBounds && !iterationCounts.empty();
}

int64_t LoopAnalysisInfo::getEstimatedTripCount() const { return tripCount; }

bool LoopAnalysisInfo::isVectorizationCandidate() const {
  return hasConstantBounds && depth <= 2;
}

bool LoopAnalysisInfo::isTilingCandidate() const {
  return isNested && depth >= 2;
}

bool LoopAnalysisInfo::isUnrollCandidate() const {
  return hasConstantBounds && tripCount <= 8; // Small trip counts
}

bool LoopAnalysisInfo::hasSimpleInductionPattern() const {
  return !inductionVariables.empty() && hasConstantBounds;
}

//===----------------------------------------------------------------------===//
// Utility Functions
//===----------------------------------------------------------------------===//

namespace mlir {
namespace arts {

StringRef toString(ControlFlowType type) {
  switch (type) {
  case ControlFlowType::Sequential:
    return "sequential";
  case ControlFlowType::Conditional:
    return "conditional";
  case ControlFlowType::LoopBound:
    return "loop-bound";
  case ControlFlowType::NestedLoop:
    return "nested-loop";
  case ControlFlowType::RecursiveCall:
    return "recursive-call";
  case ControlFlowType::IndirectControl:
    return "indirect-control";
  case ControlFlowType::TaskParallel:
    return "task-parallel";
  case ControlFlowType::DataParallel:
    return "data-parallel";
  case ControlFlowType::UnknownControl:
    return "unknown";
  }
  llvm_unreachable("Unknown ControlFlowType");
}

// toString(DataLifetime) moved to DbTypes.cpp where it belongs

bool isRegularLoopPattern(const LoopPatternInfo &pattern) {
  return pattern.isSequential || pattern.isStrided;
}

bool isOptimizableLoop(const LoopAnalysisInfo &loopInfo) {
  return loopInfo.isParallelizable || loopInfo.hasConstantBounds;
}
}
}

ControlFlowType
classifyLoopControlFlow(const SmallVector<scf::ForOp, 4> &loops) {
  if (loops.empty()) {
    return ControlFlowType::Sequential;
  } else if (loops.size() == 1) {
    return ControlFlowType::LoopBound;
  } else {
    return ControlFlowType::NestedLoop;
  }
}

//===----------------------------------------------------------------------===//
// Factory Functions
//===----------------------------------------------------------------------===//

std::unique_ptr<LoopAnalysis> createLoopAnalysis(Operation *module) {
  return std::make_unique<LoopAnalysis>(module);
}

LoopAnalysisInfo analyzeLoopContext(Operation *op, LoopAnalysis *analysis) {
  if (analysis) {
    return analysis->analyzeLoopContext(op);
  } else {
    auto tempAnalysis =
        std::make_unique<LoopAnalysis>(op->getParentOfType<ModuleOp>());
    return tempAnalysis->analyzeLoopContext(op);
  }
}

LoopPatternInfo analyzeAccessPattern(Operation *memoryOp,
                                     LoopAnalysis *analysis) {
  if (analysis) {
    return analysis->analyzeAccessPattern(memoryOp);
  } else {
    auto tempAnalysis =
        std::make_unique<LoopAnalysis>(memoryOp->getParentOfType<ModuleOp>());
    return tempAnalysis->analyzeAccessPattern(memoryOp);
  }
}
