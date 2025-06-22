///==========================================================================
/// File: DbAliasAnalysis.cpp
/// Datablock alias analysis with memory region overlap detection.
///==========================================================================

#include "arts/Analysis/Db/DbAliasAnalysis.h"
#include "arts/Analysis/Db/DbInfo.h"
#include "arts/Analysis/Db/Graph/DbNode.h"
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
static std::optional<int64_t> getConstantAsInt(const SymbolicExpr &expr) {
  if (expr.kind == SymbolicExpr::Kind::Constant) {
    return expr.constant;
  }
  return std::nullopt;
}
} // namespace

///===----------------------------------------------------------------------===///
/// DbAliasAnalysis Implementation
///===----------------------------------------------------------------------===///

DbAliasAnalysis::DbAliasAnalysis(DbAnalysis *analysis) : analysis(analysis) {
  assert(analysis && "Analysis cannot be null");
}

bool DbAliasAnalysis::mayAlias(DbInfo &A, DbInfo &B,
                               const std::string &indent) {
  auto APtr = A.isAlloc() ? A.getResult() : A.getPtr();
  auto BPtr = B.isAlloc() ? B.getResult() : B.getPtr();

  LLVM_DEBUG(DBGS_INDENT(indent) << "Analyzing alias between DB #" << A.getId()
                                 << " and DB #" << B.getId() << "\n");

  /// Same value alias with itself
  if (APtr == BPtr) {
    LLVM_DEBUG(dbgs() << indent << "  -> Same pointer, must alias\n");
    return true;
  }

  /// Check cache
  auto key = makeOrderedPair(APtr, BPtr);
  auto it = aliasCache.find(key);
  if (it != aliasCache.end()) {
    LLVM_DEBUG(dbgs() << indent << "  -> Cache hit: "
                      << (it->second ? "may alias" : "no alias") << "\n");
    return it->second;
  }

  bool result;

  if (A.isAlloc() && B.isAlloc()) {
    LLVM_DEBUG(dbgs() << indent
                      << "  -> Both are allocations. Checking effects.\n");
    SmallVector<MemoryEffects::EffectInstance, 4> effectsA = A.getEffects();
    SmallVector<MemoryEffects::EffectInstance, 4> effectsB = B.getEffects();
    result = mayAlias(effectsA, effectsB, indent + "    ");
  } else if (A.isDep() && B.isDep()) {
    LLVM_DEBUG(dbgs() << indent << "  -> Both are depes.\n");
    auto *parentA = A.getParent();
    auto *parentB = B.getParent();

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
        result = analyzeMemoryRegionOverlap(A, B, indent + "    ");
      }
    } else {
      LLVM_DEBUG(dbgs() << indent << "    -> Could not determine parents. "
                        << "Falling back to effects.\n");
      SmallVector<MemoryEffects::EffectInstance, 4> effectsA = A.getEffects();
      SmallVector<MemoryEffects::EffectInstance, 4> effectsB = B.getEffects();
      result = mayAlias(effectsA, effectsB, indent + "    ");
    }
  } else {
    DbInfo &alloc = A.isAlloc() ? A : B;
    DbInfo &dep = A.isDep() ? A : B;
    LLVM_DEBUG(dbgs() << indent << "  -> Mixed alloc #" << alloc.getId()
                      << " and dep #" << dep.getId() << ".\n");

    auto *depParent = dep.getParent();
    if (depParent) {
      LLVM_DEBUG(dbgs() << indent << "    -> Dep has parent #"
                        << depParent->getId() << ". Comparing with alloc.\n");
      result = mayAlias(alloc, *depParent, indent + "    ");
    } else {
      LLVM_DEBUG(dbgs() << indent << "    -> Dep has no parent. "
                        << "Falling back to effects.\n");
      SmallVector<MemoryEffects::EffectInstance, 4> effectsA = A.getEffects();
      SmallVector<MemoryEffects::EffectInstance, 4> effectsB = B.getEffects();
      result = mayAlias(effectsA, effectsB, indent + "    ");
    }
  }

  /// Cache and return the result
  aliasCache[key] = result;
  LLVM_DEBUG(dbgs() << indent << "  -> Final result: "
                    << (result ? "may alias" : "no alias") << "\n");
  return result;
}

bool DbAliasAnalysis::analyzeMemoryRegionOverlap(DbInfo &A, DbInfo &B,
                                                 const std::string &indent) {
  LLVM_DEBUG(DBGS_INDENT(indent) << "Analyzing memory region overlap\n");

  /// Get dimension analysis for both datablocks
  const auto &dimAnalysisA = A.getDimensionAnalysis();
  const auto &dimAnalysisB = B.getDimensionAnalysis();
  const auto &dimSizesA = A.getComputedDimSizes();
  const auto &dimSizesB = B.getComputedDimSizes();

  /// If dimensions don't match, they could still overlap if one is a subset
  size_t minDims = std::min(dimAnalysisA.size(), dimAnalysisB.size());

  LLVM_DEBUG(dbgs() << indent << "  Dimensions: A=" << dimAnalysisA.size()
                    << ", B=" << dimAnalysisB.size() << ", analyzing first "
                    << minDims << "\n");

  /// Check each dimension for potential overlap
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
  LLVM_DEBUG(dbgs() << indent
                    << "  Pattern A: " << dimA.overallPattern.getPatternName()
                    << ", Pattern B: " << dimB.overallPattern.getPatternName()
                    << "\n");

  const auto &patternA = dimA.overallPattern;
  const auto &patternB = dimB.overallPattern;

  /// Handle constant patterns - most precise analysis
  if (patternA.pattern == ComplexExpr::Pattern::Constant &&
      patternB.pattern == ComplexExpr::Pattern::Constant) {
    LLVM_DEBUG(dbgs() << indent << "  -> Using constant overlap analysis\n");
    return analyzeConstantOverlap(patternA, patternB, indent + "  ");
  }

  /// Handle sequential patterns
  if (patternA.pattern == ComplexExpr::Pattern::Sequential ||
      patternB.pattern == ComplexExpr::Pattern::Sequential) {
    LLVM_DEBUG(dbgs() << indent << "  -> Using sequential overlap analysis\n");
    return analyzeSequentialOverlap(patternA, patternB, sizeA, sizeB,
                                    indent + "  ");
  }

  /// Handle strided patterns
  if (patternA.pattern == ComplexExpr::Pattern::Strided ||
      patternB.pattern == ComplexExpr::Pattern::Strided) {
    LLVM_DEBUG(dbgs() << indent << "  -> Using strided overlap analysis\n");
    return analyzeStridedOverlap(patternA, patternB, sizeA, sizeB,
                                 indent + "  ");
  }

  /// Handle blocked patterns
  if (patternA.pattern == ComplexExpr::Pattern::Blocked ||
      patternB.pattern == ComplexExpr::Pattern::Blocked) {
    LLVM_DEBUG(dbgs() << indent << "  -> Using blocked overlap analysis\n");
    return analyzeBlockedOverlap(patternA, patternB, sizeA, sizeB,
                                 indent + "  ");
  }

  /// For complex or indirect patterns, use conservative bounds analysis
  if (patternA.hasValidBounds && patternB.hasValidBounds) {
    LLVM_DEBUG(dbgs() << indent << "  -> Using bounds overlap analysis\n");
    return analyzeBoundsOverlap(patternA, patternB, indent + "  ");
  }

  LLVM_DEBUG(dbgs() << indent << "  -> Complex patterns without bounds - "
                    << "assuming overlap\n");
  /// Conservative: assume overlap for complex patterns without bounds
  return true;
}

bool DbAliasAnalysis::analyzeConstantOverlap(const ComplexExpr &exprA,
                                             const ComplexExpr &exprB,
                                             const std::string &indent) {
  LLVM_DEBUG(DBGS_INDENT(indent) << "Constant overlap analysis\n");

  if (exprA.baseExpr.kind == SymbolicExpr::Kind::Constant &&
      exprB.baseExpr.kind == SymbolicExpr::Kind::Constant) {
    bool overlap = (exprA.baseExpr.constant == exprB.baseExpr.constant);
    LLVM_DEBUG(dbgs() << indent << "  Constants " << exprA.baseExpr.constant
                      << " vs " << exprB.baseExpr.constant << " -> "
                      << (overlap ? "overlap" : "no overlap") << "\n");
    return overlap;
  }

  LLVM_DEBUG(dbgs() << indent
                    << "  Not both constant - conservative overlap\n");
  /// Conservative if not both constant
  return true;
}

bool DbAliasAnalysis::analyzeSequentialOverlap(const ComplexExpr &exprA,
                                               const ComplexExpr &exprB,
                                               int64_t sizeA, int64_t sizeB,
                                               const std::string &indent) {
  LLVM_DEBUG(DBGS_INDENT(indent) << "Sequential overlap analysis\n");

  if (exprA.hasValidBounds && exprB.hasValidBounds) {
    LLVM_DEBUG(dbgs() << indent << "  Both have valid bounds, delegating to "
                      << "bounds analysis\n");
    return analyzeBoundsOverlap(exprA, exprB, indent + "  ");
  }

  LLVM_DEBUG(dbgs() << indent << "  Insufficient bound information - "
                    << "assuming overlap\n");
  /// Conservative if bounds are not constant
  return true;
}

bool DbAliasAnalysis::analyzeStridedOverlap(const ComplexExpr &exprA,
                                            const ComplexExpr &exprB,
                                            int64_t sizeA, int64_t sizeB,
                                            const std::string &indent) {
  LLVM_DEBUG(DBGS_INDENT(indent) << "Strided overlap analysis\n");

  const auto &baseA = exprA.baseExpr;
  const auto &baseB = exprB.baseExpr;
  const auto &strideA_expr = exprA.strideExpr;
  const auto &strideB_expr = exprB.strideExpr;

  if (strideA_expr.kind != SymbolicExpr::Kind::Constant ||
      strideB_expr.kind != SymbolicExpr::Kind::Constant) {
    LLVM_DEBUG(dbgs() << indent
                      << "  Non-constant stride - assuming overlap\n");
    return true;
  }

  int64_t strideA = strideA_expr.constant;
  int64_t strideB = strideB_expr.constant;
  int64_t gcdStrides = std::gcd(std::abs(strideA), std::abs(strideB));

  LLVM_DEBUG(dbgs() << indent << "  Stride A: " << strideA << ", Stride B: "
                    << strideB << ", GCD: " << gcdStrides << "\n");

  if (gcdStrides == 0) {
    LLVM_DEBUG(dbgs() << indent
                      << "  Zero stride detected - assuming overlap\n");
    return true;
  }

  if (baseA.kind == SymbolicExpr::Kind::Constant &&
      baseB.kind == SymbolicExpr::Kind::Constant) {
    int64_t diff = baseB.constant - baseA.constant;
    bool overlap = (diff % gcdStrides == 0);
    LLVM_DEBUG(dbgs() << indent << "  Constant bases: A=" << baseA.constant
                      << ", B=" << baseB.constant << ", diff=" << diff << " -> "
                      << (overlap ? "overlap" : "no overlap") << "\n");
    return overlap;
  }

  if (baseA.kind == SymbolicExpr::Kind::Affine &&
      baseB.kind == SymbolicExpr::Kind::Affine && baseA.base == baseB.base &&
      baseA.multiplier == 1 && baseB.multiplier == 1) {
    int64_t diff = baseB.offset - baseA.offset;
    bool overlap = (diff % gcdStrides == 0);
    LLVM_DEBUG(dbgs() << indent << "  Symbolic bases with same base value: "
                      << "offsetA=" << baseA.offset
                      << ", offsetB=" << baseB.offset << ", diff=" << diff
                      << ", gcd=" << gcdStrides << " -> "
                      << (overlap ? "overlap" : "no overlap") << "\n");
    return overlap;
  }

  LLVM_DEBUG(dbgs() << indent << "  Complex or different symbolic bases - "
                    << "assuming overlap\n");
  /// Conservative for non-constant strides
  return true;
}

bool DbAliasAnalysis::analyzeBlockedOverlap(const ComplexExpr &exprA,
                                            const ComplexExpr &exprB,
                                            int64_t sizeA, int64_t sizeB,
                                            const std::string &indent) {
  LLVM_DEBUG(DBGS_INDENT(indent) << "Blocked overlap analysis\n");

  const auto &baseA = exprA.baseExpr;
  const auto &baseB = exprB.baseExpr;
  const auto &blockSizeA_expr = exprA.blockSize;
  const auto &blockSizeB_expr = exprB.blockSize;

  if (blockSizeA_expr.kind != SymbolicExpr::Kind::Constant ||
      blockSizeB_expr.kind != SymbolicExpr::Kind::Constant) {
    LLVM_DEBUG(dbgs() << indent << "  Non-constant block size - "
                      << "assuming overlap\n");
    return true;
  }

  int64_t blockSizeA = blockSizeA_expr.constant;
  int64_t blockSizeB = blockSizeB_expr.constant;

  LLVM_DEBUG(dbgs() << indent << "  Block sizes: A=" << blockSizeA
                    << ", B=" << blockSizeB << "\n");

  if (baseA.kind == SymbolicExpr::Kind::Constant &&
      baseB.kind == SymbolicExpr::Kind::Constant) {
    int64_t bA = baseA.constant;
    int64_t bB = baseB.constant;
    bool overlap = !(bA + blockSizeA <= bB || bB + blockSizeB <= bA);
    LLVM_DEBUG(dbgs() << indent << "  Constant blocks: A[" << bA << ", "
                      << (bA + blockSizeA) << "), B[" << bB << ", "
                      << (bB + blockSizeB) << ") -> "
                      << (overlap ? "overlap" : "no overlap") << "\n");
    return overlap;
  }

  if (baseA.kind == SymbolicExpr::Kind::Affine &&
      baseB.kind == SymbolicExpr::Kind::Affine && baseA.base == baseB.base &&
      baseA.multiplier == 1 && baseB.multiplier == 1) {
    int64_t offsetA = baseA.offset;
    int64_t offsetB = baseB.offset;
    bool overlap =
        !(offsetA + blockSizeA <= offsetB || offsetB + blockSizeB <= offsetA);
    LLVM_DEBUG(dbgs() << indent << "  Symbolic blocks with same base: "
                      << "A[base+" << offsetA << ", base+"
                      << (offsetA + blockSizeA) << "), B[base+" << offsetB
                      << ", base+" << (offsetB + blockSizeB) << ") -> "
                      << (overlap ? "overlap" : "no overlap") << "\n");
    return overlap;
  }

  LLVM_DEBUG(dbgs() << indent << "  Non-constant or different symbolic base "
                    << "for block - assuming overlap\n");
  return true;
}

bool DbAliasAnalysis::analyzeBoundsOverlap(const ComplexExpr &exprA,
                                           const ComplexExpr &exprB,
                                           const std::string &indent) {
  LLVM_DEBUG(DBGS_INDENT(indent) << "Bounds overlap analysis\n");

  const auto &minA_expr = exprA.conservativeMin;
  const auto &maxA_expr = exprA.conservativeMax;
  const auto &minB_expr = exprB.conservativeMin;
  const auto &maxB_expr = exprB.conservativeMax;

  auto minA_c = getConstantAsInt(minA_expr);
  auto maxA_c = getConstantAsInt(maxA_expr);
  auto minB_c = getConstantAsInt(minB_expr);
  auto maxB_c = getConstantAsInt(maxB_expr);

  if (minA_c && maxA_c && minB_c && maxB_c) {
    bool overlap = !(*maxA_c < *minB_c || *maxB_c < *minA_c);
    LLVM_DEBUG(dbgs() << indent << "  Constant bounds: A[" << *minA_c << ", "
                      << *maxA_c << "], B[" << *minB_c << ", " << *maxB_c
                      << "] -> " << (overlap ? "overlap" : "no overlap")
                      << "\n");
    return overlap;
  }

  if (minA_expr.kind == SymbolicExpr::Kind::Affine &&
      maxA_expr.kind == SymbolicExpr::Kind::Affine &&
      minB_expr.kind == SymbolicExpr::Kind::Affine &&
      maxB_expr.kind == SymbolicExpr::Kind::Affine && minA_expr.base &&
      minA_expr.base == minB_expr.base && minA_expr.base == maxA_expr.base &&
      minA_expr.base == maxB_expr.base && minA_expr.multiplier == 1 &&
      maxA_expr.multiplier == 1 && minB_expr.multiplier == 1 &&
      maxB_expr.multiplier == 1) {
    int64_t lowA = minA_expr.offset;
    int64_t highA = maxA_expr.offset;
    int64_t lowB = minB_expr.offset;
    int64_t highB = maxB_expr.offset;

    if (highA < lowB || highB < lowA) {
      LLVM_DEBUG(dbgs() << indent << "  Symbolic disjoint bounds for base "
                        << minA_expr.base << ": A[base+" << lowA << ", base+"
                        << highA << "], B[base+" << lowB << ", base+" << highB
                        << "] -> no overlap\n");
      return false;
    } else {
      LLVM_DEBUG(dbgs() << indent << "  Symbolic overlapping bounds for base "
                        << minA_expr.base << ": A[base+" << lowA << ", base+"
                        << highA << "], B[base+" << lowB << ", base+" << highB
                        << "] -> overlap\n");
      return true;
    }
  }

  LLVM_DEBUG(dbgs() << indent << "  Symbolic or unhandled bounds - "
                    << "assuming overlap\n");
  /// Conservative for symbolic bounds
  return true;
}

bool DbAliasAnalysis::mayAlias(
    SmallVector<MemoryEffects::EffectInstance, 4> &effectsA,
    SmallVector<MemoryEffects::EffectInstance, 4> &effectsB,
    const std::string &indent) {
  LLVM_DEBUG(DBGS_INDENT(indent) << "Analyzing memory effects overlap\n");
  LLVM_DEBUG(dbgs() << indent << "  Effects A: " << effectsA.size()
                    << ", Effects B: " << effectsB.size() << "\n");

  for (auto &eA : effectsA) {
    for (auto &eB : effectsB) {
      if (::mayAlias(eA, eB)) {
        LLVM_DEBUG(dbgs() << indent
                          << "  -> Found overlapping effects -> may alias\n");
        return true;
      }
    }
  }

  LLVM_DEBUG(dbgs() << indent
                    << "  -> No overlapping effects found -> no alias\n");
  return false;
}
