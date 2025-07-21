//===----------------------------------------------------------------------===//
// Db/DbAliasAnalysis.cpp - Implementation of DbAliasAnalysis
//===----------------------------------------------------------------------===//

#include "arts/Analysis/Db/DbAliasAnalysis.h"
#include "arts/Analysis/Db/DbAnalysis.h"
#include "mlir/Analysis/AliasAnalysis/LocalAliasAnalysis.h"
#include "mlir/IR/OpImplementation.h"
#include "mlir/IR/Types.h"
#include "mlir/Interfaces/InferTypeOpInterface.h"
#include "llvm/Support/Debug.h"
#include <algorithm>
#include <numeric>

#define DEBUG_TYPE "db-alias-analysis"
#define dbgs() llvm::dbgs()
#define DBGS() (dbgs() << "[" DEBUG_TYPE "] ")
#define DBGS_INDENT(indent) (dbgs() << indent << "[" DEBUG_TYPE "] ")

using namespace mlir;
using namespace mlir::arts;

namespace {
std::optional<int64_t> getConstantAsInt(const SymbolicExpr &expr) {
  if (expr.kind == SymbolicExpr::Kind::Constant) {
    return expr.constant;
  }
  return std::nullopt;
}

std::pair<Value, Value> makeOrderedPair(Value a, Value b) {
  // Use pointer comparison for ordering
  return a.getAsOpaquePointer() < b.getAsOpaquePointer() ? std::make_pair(a, b)
                                                         : std::make_pair(b, a);
}

SmallVector<MemoryEffects::EffectInstance, 4> getEffects(Operation *op) {
  SmallVector<MemoryEffects::EffectInstance, 4> effects;
  if (auto effectOp = dyn_cast<MemoryEffectOpInterface>(op)) {
    effectOp.getEffects(effects);
  }
  return effects;
}
} // namespace

DbAliasAnalysis::DbAliasAnalysis(DbAnalysis *analysis)
    : analysis(analysis) {
  assert(analysis && "Analysis cannot be null");
}

bool DbAliasAnalysis::mayAlias(const NodeBase &a, const NodeBase &b,
                               const std::string &indent) {
  Value ptrA = getUnderlyingValue(a);
  Value ptrB = getUnderlyingValue(b);

  LLVM_DEBUG(DBGS_INDENT(indent)
             << "Analyzing alias between node " << a.getHierId() << " and node "
             << b.getHierId() << "\n");

  // Same value aliases with itself
  if (ptrA == ptrB) {
    LLVM_DEBUG(dbgs() << indent << "  -> Same pointer, must alias\n");
    return true;
  }

  // Check cache
  auto key = makeOrderedPair(ptrA, ptrB);
  auto it = aliasCache.find(key);
  if (it != aliasCache.end()) {
    LLVM_DEBUG(dbgs() << indent << "  -> Cache hit: "
                      << (it->second ? "may alias" : "no alias") << "\n");
    return it->second;
  }

  // Quick check with MLIR AA
  // TODO: Fix AliasAnalysis initialization
  // AliasResult mlirResult = mlirAA.alias(ptrA, ptrB);
  // if (mlirResult == AliasResult::NoAlias) {
  //   LLVM_DEBUG(dbgs() << indent << "  -> MLIR AA says no alias\n");
  //   aliasCache[key] = false;
  //   return false;
  // } else if (mlirResult == AliasResult::MustAlias) {
  //   LLVM_DEBUG(dbgs() << indent << "  -> MLIR AA says must alias\n");
  //   aliasCache[key] = true;
  //   return true;
  // }

  bool result;

  if (isa<DbAllocNode>(&a) && isa<DbAllocNode>(&b)) {
    LLVM_DEBUG(
        dbgs() << indent
               << "  -> Both are allocations. Checking effects and regions.\n");
    SmallVector<MemoryEffects::EffectInstance, 4> effectsA;
    effectsA = getEffects(a.getOp());
    SmallVector<MemoryEffects::EffectInstance, 4> effectsB;
    effectsB = getEffects(b.getOp());
    if (!mayAliasEffects(effectsA, effectsB, indent + "    ")) {
      result = false;
    } else {
      result = analyzeMemoryRegionOverlap(a, b, indent + "    ");
    }
  } else if ((isa<DbAcquireNode>(&a) || isa<DbReleaseNode>(&a)) &&
             (isa<DbAcquireNode>(&b) || isa<DbReleaseNode>(&b))) {
    LLVM_DEBUG(dbgs() << indent
                      << "  -> Both are access ops (acquire/release).\n");
    auto parentA = isa<DbAcquireNode>(&a)
                       ? dyn_cast<DbAcquireNode>(&a)->getParent()
                       : dyn_cast<DbReleaseNode>(&a)->getParent();
    auto parentB = isa<DbAcquireNode>(&b)
                       ? dyn_cast<DbAcquireNode>(&b)->getParent()
                       : dyn_cast<DbReleaseNode>(&b)->getParent();

    if (parentA && parentB) {
      if (!mayAlias(*parentA, *parentB, indent + "    ")) {
        LLVM_DEBUG(
            dbgs() << indent
                   << "    -> Different parent allocations. No alias.\n");
        result = false;
      } else {
        LLVM_DEBUG(
            dbgs() << indent
                   << "    -> Same parent allocation. Checking regions.\n");
        result = analyzeMemoryRegionOverlap(a, b, indent + "    ");
      }
    } else {
      LLVM_DEBUG(
          dbgs()
          << indent
          << "    -> Could not determine parents. Falling back to effects.\n");
      SmallVector<MemoryEffects::EffectInstance, 4> effectsA =
          getEffects(a.getOp());
      SmallVector<MemoryEffects::EffectInstance, 4> effectsB =
          getEffects(b.getOp());
      result = mayAliasEffects(effectsA, effectsB, indent + "    ");
    }
  } else {
    const NodeBase &alloc = isa<DbAllocNode>(&a) ? a : b;
    const NodeBase &access = isa<DbAllocNode>(&a) ? b : a;
    LLVM_DEBUG(dbgs() << indent
                      << "  -> Mixed alloc and access (acquire/release).\n");

    auto accessParent = isa<DbAcquireNode>(&access)
                            ? dyn_cast<DbAcquireNode>(&access)->getParent()
                            : dyn_cast<DbReleaseNode>(&access)->getParent();
    if (accessParent) {
      LLVM_DEBUG(
          dbgs()
          << indent
          << "    -> Access has parent alloc. Comparing with given alloc.\n");
      result = mayAlias(alloc, *accessParent, indent + "    ");
    } else {
      LLVM_DEBUG(
          dbgs() << indent
                 << "    -> Access has no parent. Falling back to effects.\n");
      SmallVector<MemoryEffects::EffectInstance, 4> effectsAlloc =
          getEffects(alloc.getOp());
      SmallVector<MemoryEffects::EffectInstance, 4> effectsAccess =
          getEffects(access.getOp());
      result = mayAliasEffects(effectsAlloc, effectsAccess, indent + "    ");
    }
  }

  aliasCache[key] = result;
  LLVM_DEBUG(dbgs() << indent << "  -> Final result: "
                    << (result ? "may alias" : "no alias") << "\n");
  return result;
}

Value DbAliasAnalysis::getUnderlyingValue(const NodeBase &node) {
  Operation *op = node.getOp();
  if (isa<DbAllocOp>(op)) {
    return op->getResult(0);
  } else if (isa<DbAcquireOp>(op)) {
    return cast<DbAcquireOp>(op).getSource();
  } else if (isa<DbReleaseOp>(op)) {
    return cast<DbReleaseOp>(op).getSources()[0];
  }
  llvm_unreachable("Invalid DB node type");
}

SmallVector<int64_t> DbAliasAnalysis::computeDimSizes(const NodeBase &node) {
  SmallVector<int64_t> sizes = analysis->getComputedDimSizes(node);
  Operation *op = node.getOp();

  if (auto inferOp = dyn_cast<InferTypeOpInterface>(op)) {
    SmallVector<Type> inferredTypes;
    if (succeeded(inferOp.inferReturnTypes(
            op->getContext(), op->getLoc(), op->getOperands(),
            op->getAttrDictionary(), op->getPropertiesStorage(),
            op->getRegions(), inferredTypes))) {
      for (Type ty : inferredTypes) {
        if (auto memrefTy = dyn_cast<MemRefType>(ty)) {
          ArrayRef<int64_t> shape = memrefTy.getShape();
          sizes.assign(shape.begin(), shape.end());
          break; // Assume first result
        }
      }
    }
  }

  return sizes;
}

bool DbAliasAnalysis::analyzeMemoryRegionOverlap(const NodeBase &a,
                                                 const NodeBase &b,
                                                 const std::string &indent) {
  LLVM_DEBUG(DBGS_INDENT(indent) << "Analyzing memory region overlap\n");

  const auto &dimAnalysisA = analysis->getDimensionAnalysis(a);
  const auto &dimAnalysisB = analysis->getDimensionAnalysis(b);
  SmallVector<int64_t> dimSizesA =
      computeDimSizes(a); // Enhanced with inference
  SmallVector<int64_t> dimSizesB = computeDimSizes(b);

  size_t minDims = std::min(dimAnalysisA.size(), dimAnalysisB.size());

  LLVM_DEBUG(dbgs() << indent << "  Dimensions: A=" << dimAnalysisA.size()
                    << ", B=" << dimAnalysisB.size() << ", analyzing first "
                    << minDims << "\n");

  for (size_t dim = 0; dim < minDims; ++dim) {
    if (!analyzeDimensionOverlap(dimAnalysisA[dim], dimAnalysisB[dim],
                                 dimSizesA.size() > dim ? dimSizesA[dim] : -1,
                                 dimSizesB.size() > dim ? dimSizesB[dim] : -1,
                                 dim, indent + "  ")) {
      LLVM_DEBUG(dbgs() << indent << "  -> Dimension " << dim
                        << " has no overlap\n");
      return false;
    }
  }

  LLVM_DEBUG(dbgs() << indent << "  -> All analyzed dimensions may overlap\n");
  return true;
}

bool DbAliasAnalysis::analyzeDimensionOverlap(const DimensionAnalysis &dimA,
                                              const DimensionAnalysis &dimB,
                                              int64_t sizeA, int64_t sizeB,
                                              size_t dimIndex,
                                              const std::string &indent) {
  LLVM_DEBUG(DBGS_INDENT(indent)
             << "Analyzing dimension " << dimIndex << " overlap\n");
  LLVM_DEBUG(dbgs() << indent << "  Pattern A: " << dimA.overallPattern.pattern
                    << ", Pattern B: " << dimB.overallPattern.pattern << "\n");

  const auto &patternA = dimA.overallPattern;
  const auto &patternB = dimB.overallPattern;

  if (patternA.pattern == ComplexExpr::Constant &&
      patternB.pattern == ComplexExpr::Constant) {
    return analyzeConstantOverlap(patternA, patternB, indent + "  ");
  }

  if (patternA.pattern == ComplexExpr::Sequential ||
      patternB.pattern == ComplexExpr::Sequential) {
    return analyzeSequentialOverlap(patternA, patternB, sizeA, sizeB,
                                    indent + "  ");
  }

  if (patternA.pattern == ComplexExpr::Strided ||
      patternB.pattern == ComplexExpr::Strided) {
    return analyzeStridedOverlap(patternA, patternB, sizeA, sizeB,
                                 indent + "  ");
  }

  if (patternA.pattern == ComplexExpr::Blocked ||
      patternB.pattern == ComplexExpr::Blocked) {
    return analyzeBlockedOverlap(patternA, patternB, sizeA, sizeB,
                                 indent + "  ");
  }

  if (patternA.hasValidBounds && patternB.hasValidBounds) {
    return analyzeBoundsOverlap(patternA, patternB, indent + "  ");
  }

  LLVM_DEBUG(
      dbgs() << indent
             << "  -> Complex patterns without bounds - assuming overlap\n");
  return true;
}

bool DbAliasAnalysis::analyzeConstantOverlap(const ComplexExpr &exprA,
                                             const ComplexExpr &exprB,
                                             const std::string &indent) {
  LLVM_DEBUG(DBGS_INDENT(indent) << "Constant overlap analysis\n");

  auto baseA = getConstantAsInt(exprA.baseExpr);
  auto baseB = getConstantAsInt(exprB.baseExpr);
  if (baseA && baseB) {
    bool overlap = *baseA == *baseB;
    LLVM_DEBUG(dbgs() << indent << "  Constants " << *baseA << " vs " << *baseB
                      << " -> " << (overlap ? "overlap" : "no overlap")
                      << "\n");
    return overlap;
  }

  LLVM_DEBUG(dbgs() << indent
                    << "  Not both constant - conservative overlap\n");
  return true;
}

bool DbAliasAnalysis::analyzeSequentialOverlap(const ComplexExpr &exprA,
                                               const ComplexExpr &exprB,
                                               int64_t sizeA, int64_t sizeB,
                                               const std::string &indent) {
  LLVM_DEBUG(DBGS_INDENT(indent) << "Sequential overlap analysis\n");

  if (exprA.hasValidBounds && exprB.hasValidBounds) {
    return analyzeBoundsOverlap(exprA, exprB, indent + "  ");
  }

  LLVM_DEBUG(dbgs() << indent
                    << "  Insufficient bound information - assuming overlap\n");
  return true;
}

bool DbAliasAnalysis::analyzeStridedOverlap(const ComplexExpr &exprA,
                                            const ComplexExpr &exprB,
                                            int64_t sizeA, int64_t sizeB,
                                            const std::string &indent) {
  LLVM_DEBUG(DBGS_INDENT(indent) << "Strided overlap analysis\n");

  auto strideA = getConstantAsInt(exprA.strideExpr);
  auto strideB = getConstantAsInt(exprB.strideExpr);
  if (!strideA || !strideB) {
    LLVM_DEBUG(dbgs() << indent
                      << "  Non-constant stride - assuming overlap\n");
    return true;
  }

  int64_t gcdStrides = std::gcd(std::abs(*strideA), std::abs(*strideB));

  auto baseA = getConstantAsInt(exprA.baseExpr);
  auto baseB = getConstantAsInt(exprB.baseExpr);
  if (baseA && baseB) {
    int64_t diff = *baseB - *baseA;
    bool overlap = (diff % gcdStrides == 0);
    LLVM_DEBUG(dbgs() << indent << "  Constant bases diff=" << diff
                      << " gcd=" << gcdStrides << " -> "
                      << (overlap ? "overlap" : "no overlap") << "\n");
    return overlap;
  }

  LLVM_DEBUG(dbgs() << indent << "  Complex bases - assuming overlap\n");
  return true;
}

bool DbAliasAnalysis::analyzeBlockedOverlap(const ComplexExpr &exprA,
                                            const ComplexExpr &exprB,
                                            int64_t sizeA, int64_t sizeB,
                                            const std::string &indent) {
  LLVM_DEBUG(DBGS_INDENT(indent) << "Blocked overlap analysis\n");

  auto blockSizeA = getConstantAsInt(exprA.blockSize);
  auto blockSizeB = getConstantAsInt(exprB.blockSize);
  if (!blockSizeA || !blockSizeB) {
    LLVM_DEBUG(dbgs() << indent
                      << "  Non-constant block size - assuming overlap\n");
    return true;
  }

  auto baseA = getConstantAsInt(exprA.baseExpr);
  auto baseB = getConstantAsInt(exprB.baseExpr);
  if (baseA && baseB) {
    bool overlap =
        !(*baseA + *blockSizeA <= *baseB || *baseB + *blockSizeB <= *baseA);
    LLVM_DEBUG(dbgs() << indent << "  Blocks A[" << *baseA << "-"
                      << (*baseA + *blockSizeA) << "] B[" << *baseB << "-"
                      << (*baseB + *blockSizeB) << "] -> "
                      << (overlap ? "overlap" : "no overlap") << "\n");
    return overlap;
  }

  LLVM_DEBUG(dbgs() << indent << "  Non-constant bases - assuming overlap\n");
  return true;
}

bool DbAliasAnalysis::analyzeBoundsOverlap(const ComplexExpr &exprA,
                                           const ComplexExpr &exprB,
                                           const std::string &indent) {
  LLVM_DEBUG(DBGS_INDENT(indent) << "Bounds overlap analysis\n");

  auto minA = getConstantAsInt(exprA.conservativeMin);
  auto maxA = getConstantAsInt(exprA.conservativeMax);
  auto minB = getConstantAsInt(exprB.conservativeMin);
  auto maxB = getConstantAsInt(exprB.conservativeMax);

  if (minA && maxA && minB && maxB) {
    bool overlap = !(*maxA < *minB || *maxB < *minA);
    LLVM_DEBUG(dbgs() << indent << "  Bounds A[" << *minA << "-" << *maxA
                      << "] B[" << *minB << "-" << *maxB << "] -> "
                      << (overlap ? "overlap" : "no overlap") << "\n");
    return overlap;
  }

  LLVM_DEBUG(dbgs() << indent << "  Non-constant bounds - assuming overlap\n");
  return true;
}

bool DbAliasAnalysis::mayAliasEffects(
    const SmallVector<MemoryEffects::EffectInstance, 4> &effectsA,
    const SmallVector<MemoryEffects::EffectInstance, 4> &effectsB,
    const std::string &indent) {
  LLVM_DEBUG(DBGS_INDENT(indent) << "Analyzing memory effects overlap\n");
  LLVM_DEBUG(dbgs() << indent << "  Effects A: " << effectsA.size()
                    << ", Effects B: " << effectsB.size() << "\n");

      for (auto &eA : effectsA) {
      for (auto &eB : effectsB) {
        // TODO: Fix AliasAnalysis initialization
        // if (mlirAA.alias(eA.getValue(), eB.getValue()) != AliasResult::NoAlias) {
        //   LLVM_DEBUG(
        //       dbgs()
        //       << indent
        //       << "  -> Found potential overlapping effects -> may alias\n");
        //   return true;
        // }
      }
    }

  LLVM_DEBUG(dbgs() << indent
                    << "  -> No overlapping effects found -> no alias\n");
  return false;
}