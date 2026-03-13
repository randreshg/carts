///===----------------------------------------------------------------------===///
/// MemrefAnalyzer.cpp - Memory Reference Analysis Implementation
///===----------------------------------------------------------------------===///

#include "arts/analysis/metadata/MemrefAnalyzer.h"
#include "arts/analysis/metadata/DependenceAnalyzer.h"
#include "arts/analysis/metadata/MetadataManager.h"
#include "arts/analysis/value/ValueAnalysis.h"
#include "arts/utils/Utils.h"
#include "arts/utils/metadata/LocationMetadata.h"
#include "arts/utils/metadata/Metadata.h"
#include "mlir/Dialect/Affine/Analysis/AffineAnalysis.h"
#include "mlir/Dialect/Affine/Analysis/LoopAnalysis.h"
#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "llvm/ADT/DenseSet.h"
#include <algorithm>

#include "arts/utils/Debug.h"
ARTS_DEBUG_SETUP(memref_analyzer);

using namespace mlir;
using namespace mlir::arts;

namespace {
using DimPattern = MemrefMetadata::DimAccessPatternType;

DimPattern mergePattern(DimPattern current, DimPattern candidate) {
  if (static_cast<int64_t>(candidate) > static_cast<int64_t>(current))
    return candidate;
  return current;
}
} // namespace

///===----------------------------------------------------------------------===///
/// ReuseAnalyzer Implementation
///===----------------------------------------------------------------------===///

/// Collect all memory accesses in program order
SmallVector<Operation *>
ReuseAnalyzer::collectAccessesInOrder(Value memref, Operation *scopeOp) {
  SmallVector<Operation *> accesses;
  scopeOp->walk([&](Operation *op) {
    if (accessAnalyzer.getAccessedMemref(op) == memref)
      accesses.push_back(op);
  });

  return accesses;
}

/// Count unique memory references between two access operations
uint64_t ReuseAnalyzer::countUniqueMemrefsBetween(
    uint64_t startIdx, uint64_t endIdx,
    const SmallVector<Operation *> &accesses) {
  llvm::DenseSet<Value> uniqueMemrefs;

  for (size_t i = startIdx + 1; i < endIdx; ++i) {
    Operation *op = accesses[i];
    if (Value memref = accessAnalyzer.getAccessedMemref(op))
      uniqueMemrefs.insert(memref);
  }

  return uniqueMemrefs.size();
}

/// Compute stack distance between two accesses
/// Stack distance = number of unique memory locations accessed between reuses
uint64_t ReuseAnalyzer::computeStackDistance(
    Operation *access1, Operation *access2,
    const SmallVector<Operation *> &allAccesses) {

  /// Find indices of the two accesses
  auto it1 = std::find(allAccesses.begin(), allAccesses.end(), access1);
  auto it2 = std::find(allAccesses.begin(), allAccesses.end(), access2);

  if (it1 == allAccesses.end() || it2 == allAccesses.end())
    return 0;

  size_t idx1 = std::distance(allAccesses.begin(), it1);
  size_t idx2 = std::distance(allAccesses.begin(), it2);

  /// Ensure idx1 < idx2
  if (idx1 > idx2)
    std::swap(idx1, idx2);

  /// Stack distance = number of unique memory locations accessed between reuses
  return static_cast<int64_t>(
      countUniqueMemrefsBetween(idx1, idx2, allAccesses));
}

/// Compute reuse distance using stack distance algorithm
/// Returns average stack distance across all reuses
std::optional<uint64_t>
ReuseAnalyzer::computeReuseDistance(Value memref, Operation *scopeOp) {
  /// Collect all accesses to this memref in program order
  SmallVector<Operation *> accesses = collectAccessesInOrder(memref, scopeOp);

  /// No reuse if less than 2 accesses
  if (accesses.size() < 2)
    return std::nullopt;

  /// Collect all memory accesses in the scope
  SmallVector<Operation *> allAccesses;
  scopeOp->walk([&](Operation *op) {
    if (accessAnalyzer.isMemoryAccess(op))
      allAccesses.push_back(op);
  });

  /// Compute stack distance for each consecutive pair of accesses to same
  /// memref
  uint64_t totalStackDistance = 0, reuseCount = 0;

  for (size_t i = 1; i < accesses.size(); ++i) {
    Operation *prevAccess = accesses[i - 1];
    Operation *currAccess = accesses[i];

    uint64_t stackDist =
        computeStackDistance(prevAccess, currAccess, allAccesses);
    totalStackDistance += stackDist;
    ++reuseCount;
  }

  /// Return average stack distance
  return reuseCount > 0 ? (totalStackDistance / reuseCount) : 0;
}

/// Classify temporal locality based on reuse distance
/// Returns: Excellent (<10), Good (<100), Fair (<1000), Poor (>=1000)
TemporalLocalityLevel
ReuseAnalyzer::classifyTemporalLocality(uint64_t reuseDistance) {
  if (reuseDistance < 10)
    return TemporalLocalityLevel::Excellent;
  else if (reuseDistance < 100)
    return TemporalLocalityLevel::Good;
  else if (reuseDistance < 1000)
    return TemporalLocalityLevel::Fair;
  else
    return TemporalLocalityLevel::Poor;
}

///===----------------------------------------------------------------------===///
/// MemrefAnalyzer Implementation
///===----------------------------------------------------------------------===///
/// Count total accesses (reads + writes) to a memref
std::pair<uint64_t, uint64_t>
MemrefAnalyzer::countAccesses(Value memref, Operation *scopeOp) {
  uint64_t reads = 0, writes = 0;

  scopeOp->walk([&](Operation *op) {
    if (accessAnalyzer.getAccessedMemref(op) == memref) {
      if (isa<affine::AffineLoadOp, memref::LoadOp>(op))
        reads++;
      else if (isa<affine::AffineStoreOp, memref::StoreOp>(op))
        writes++;
    }
  });

  return {reads, writes};
}

/// Count read and write accesses separately
std::pair<uint64_t, uint64_t>
MemrefAnalyzer::countAccessTypes(Value memref, Operation *scopeOp) {
  return countAccesses(memref, scopeOp);
}

/// Compute allocation and deallocation locations
std::pair<ArtsId, ArtsId> MemrefAnalyzer::computeLifetime(Value memref,
                                                          Operation *scopeOp) {
  auto getId = [&](Operation *op) -> ArtsId {
    if (!op)
      return std::nullopt;
    return metadataManager.assignOperationId(op);
  };

  ArtsId allocId = getId(memref.getDefiningOp());
  ArtsId deallocId;
  for (Operation *user : memref.getUsers()) {
    if (isa<memref::DeallocOp>(user)) {
      deallocId = getId(user);
      break;
    }
  }
  return {allocId, deallocId};
}

/// Analyze access characteristics (affine vs non-affine)
void MemrefAnalyzer::analyzeAccessCharacteristics(Value memref,
                                                  MemrefMetadata *metadata,
                                                  Operation *scopeOp) {
  int64_t affineCount = 0, nonAffineCount = 0;

  scopeOp->walk([&](Operation *op) {
    if (accessAnalyzer.getAccessedMemref(op) == memref) {
      if (accessAnalyzer.isAffineAccess(op))
        affineCount++;
      else
        nonAffineCount++;
    }
  });

  metadata->allAccessesAffine = (affineCount > 0 && nonAffineCount == 0);
  metadata->hasNonAffineAccesses = (nonAffineCount > 0);
  metadata->dimAccessPatterns = computeDimAccessPatterns(memref, scopeOp);
  metadata->estimatedAccessBytes =
      computeEstimatedAccessBytes(memref, metadata);
}

/// Analyze uniform access pattern
bool MemrefAnalyzer::analyzeUniformAccess(Value memref, Operation *scopeOp) {
  std::optional<AffineMap> firstMap;

  bool isUniform = true;
  scopeOp->walk([&](Operation *op) {
    if (accessAnalyzer.getAccessedMemref(op) == memref) {
      if (auto affineMap = accessAnalyzer.extractAffineMap(op)) {
        if (!firstMap) {
          firstMap = affineMap;
        } else if (*firstMap != *affineMap) {
          isUniform = false;
          return WalkResult::interrupt();
        }
      }
    }
    return WalkResult::advance();
  });

  return isUniform;
}

/// Detect stencil access patterns by analyzing memory access offsets.
///
/// A stencil pattern is characterized by multiple distinct constant offsets
/// relative to an induction variable, such as:
///   - 1D stencil: A[i-1], A[i], A[i+1]  → offsets {-1, 0, 1}
///   - 2D stencil: B[i-1][j], B[i][j-1], B[i][j], B[i][j+1], B[i+1][j]
///
/// This function analyzes both affine and non-affine memory accesses to
/// extract constant offset patterns.
///
/// Returns: true if at least 2 distinct offsets are found (indicating a
///          stencil pattern), false otherwise
bool MemrefAnalyzer::hasStencilAccessPattern(Value memref, Operation *scopeOp) {
  ARTS_DEBUG(
      "hasStencilAccessPattern() - Starting pattern detection for memref");
  llvm::DenseSet<int64_t> constantOffsets;

  scopeOp->walk([&](Operation *op) {
    /// Skip operations that don't access our target memref
    if (accessAnalyzer.getAccessedMemref(op) != memref)
      return;

    /// Extract indices from the access operation
    SmallVector<Value> indices;
    if (auto load = dyn_cast<memref::LoadOp>(op)) {
      indices.append(load.getIndices().begin(), load.getIndices().end());
    } else if (auto store = dyn_cast<memref::StoreOp>(op)) {
      indices.append(store.getIndices().begin(), store.getIndices().end());
    } else if (auto affineLoad = dyn_cast<affine::AffineLoadOp>(op)) {
      /// For affine loads, extract constant offsets from the affine map.
      /// Handles patterns like: affine_map<(d0) -> (d0 + c)> where c is
      /// constant
      AffineMap map = affineLoad.getAffineMap();
      for (AffineExpr expr : map.getResults()) {
        /// Binary expression: look for additions with constant operands
        if (auto binExpr = expr.dyn_cast<AffineBinaryOpExpr>()) {
          if (binExpr.getKind() == AffineExprKind::Add) {
            if (auto rhsConst =
                    binExpr.getRHS().dyn_cast<AffineConstantExpr>()) {
              int64_t offset = rhsConst.getValue();
              ARTS_DEBUG("  Found affine load offset: " << offset);
              constantOffsets.insert(offset);
            }
          }
        } else if (expr.dyn_cast<AffineDimExpr>()) {
          /// Direct dimension access (offset = 0), e.g., affine_map<(d0) ->
          /// (d0)>
          ARTS_DEBUG("  Found direct dimension access (offset=0)");
          constantOffsets.insert(0);
        }
      }
      return;
    } else if (auto affineStore = dyn_cast<affine::AffineStoreOp>(op)) {
      AffineMap map = affineStore.getAffineMap();
      for (AffineExpr expr : map.getResults()) {
        if (auto binExpr = expr.dyn_cast<AffineBinaryOpExpr>()) {
          if (binExpr.getKind() == AffineExprKind::Add) {
            if (auto rhsConst =
                    binExpr.getRHS().dyn_cast<AffineConstantExpr>()) {
              int64_t offset = rhsConst.getValue();
              ARTS_DEBUG("  Found affine store offset: " << offset);
              constantOffsets.insert(offset);
            }
          }
        } else if (expr.dyn_cast<AffineDimExpr>()) {
          ARTS_DEBUG("  Found direct dimension access (offset=0)");
          constantOffsets.insert(0);
        }
      }
      return;
    } else {
      /// Not a memory access operation we recognize
      return;
    }

    /// For non-affine accesses, try to extract constant offsets from index
    /// expressions by pattern matching common forms: iv, iv+c, iv-c
    for (Value idx : indices) {
      /// Check if this is a simple induction variable (offset = 0)
      if (auto arg = idx.dyn_cast<BlockArgument>()) {
        Operation *parent = arg.getOwner()->getParentOp();
        if (isa<affine::AffineForOp, scf::ForOp>(parent)) {
          constantOffsets.insert(0);
          continue;
        }
      }

      /// Try to extract constant offset from expressions like:
      ///   - %idx = arith.addi %iv, %c1 : index   → offset = 1
      ///   - %idx = arith.subi %iv, %c1 : index   → offset = -1
      Operation *defOp = idx.getDefiningOp();
      if (!defOp)
        continue;

      if (auto addOp = dyn_cast<arith::AddIOp>(defOp)) {
        int64_t constVal = 0;
        /// Check both operands (addition is commutative)
        if (ValueAnalysis::getConstantIndex(addOp.getRhs(), constVal)) {
          constantOffsets.insert(constVal);
        } else if (ValueAnalysis::getConstantIndex(addOp.getLhs(), constVal)) {
          constantOffsets.insert(constVal);
        }
      } else if (auto subOp = dyn_cast<arith::SubIOp>(defOp)) {
        int64_t constVal = 0;
        /// Only check RHS for subtraction (not commutative)
        if (ValueAnalysis::getConstantIndex(subOp.getRhs(), constVal)) {
          /// Negate the constant for subtraction
          constantOffsets.insert(-constVal);
        }
      }
    }
  });

  /// A stencil pattern requires multiple distinct offsets (e.g., -1, 0, +1).
  /// Single offset or no offsets indicate uniform access, not stencil.
  ARTS_DEBUG("  Total distinct offsets found: " << constantOffsets.size());
  if (!constantOffsets.empty()) {
    ARTS_DEBUG_REGION({
      ARTS_DBGS() << "  Offsets: {";
      bool first = true;
      for (int64_t offset : constantOffsets) {
        if (!first)
          ARTS_DBGS() << ", ";
        ARTS_DBGS() << offset;
        first = false;
      }
      ARTS_DBGS() << "}\n";
    });
  }

  /// Threshold: at least 2 distinct offsets indicates stencil pattern
  bool isStencil = constantOffsets.size() >= 2;
  ARTS_DEBUG(
      "  hasStencilAccessPattern result: " << (isStencil ? "true" : "false"));
  return isStencil;
}

/// Classify dominant access pattern (sequential, strided, random)
AccessPatternType
MemrefAnalyzer::classifyAccessPattern(Value memref, MemrefMetadata *metadata,
                                      Operation *scopeOp) {
  ARTS_DEBUG("classifyAccessPattern() - Classifying access pattern");

  /// Check for stencil patterns first (must be before sequential check)
  if (hasStencilAccessPattern(memref, scopeOp)) {
    ARTS_DEBUG(
        "  Classification: Stencil (multiple distinct offsets detected)");
    return AccessPatternType::Stencil;
  }

  /// Check for stride-1 sequential access
  if (accessAnalyzer.hasStrideOneAccess(memref, scopeOp)) {
    ARTS_DEBUG("  Classification: Sequential (stride-1 access)");
    return AccessPatternType::Sequential;
  }

  /// Check for uniform strided access
  if (metadata->allAccessesAffine && *metadata->allAccessesAffine &&
      analyzeUniformAccess(memref, scopeOp)) {
    ARTS_DEBUG("  Classification: Strided (uniform affine access)");
    return AccessPatternType::Strided;
  }

  /// Check for gather/scatter patterns (non-affine or complex affine)
  if (metadata->hasNonAffineAccesses && *metadata->hasNonAffineAccesses) {
    ARTS_DEBUG("  Classification: GatherScatter (non-affine accesses)");
    return AccessPatternType::GatherScatter;
  }

  /// Default to random if pattern is unclear
  ARTS_DEBUG("  Classification: Random (fallback)");
  return AccessPatternType::Random;
}

/// Analyze a memory allocation operation and populate metadata
void MemrefAnalyzer::analyzeAllocation(Operation *allocOp,
                                       MemrefMetadata *metadata,
                                       Operation *scopeOp) {
  Value memref = allocOp->getResult(0);
  auto memrefType = memref.getType().cast<MemRefType>();

  /// Basic properties
  metadata->rank = memrefType.getRank();
  metadata->allocationId =
      LocationMetadata::fromLocation(allocOp->getLoc()).getKey().str();

  /// Compute total size in bytes
  if (memrefType.hasStaticShape()) {
    int64_t totalElements = memrefType.getNumElements();
    Type elemType = memrefType.getElementType();
    uint64_t elemBytes = arts::getElementTypeByteSize(elemType);
    if (elemBytes > 0)
      metadata->memoryFootprint =
          totalElements * static_cast<int64_t>(elemBytes);
  }

  /// Count accesses
  auto [reads, writes] = countAccesses(memref, scopeOp);
  metadata->accessStats.totalAccesses = reads + writes;
  metadata->accessStats.readCount = reads;
  metadata->accessStats.writeCount = writes;

  auto [firstId, lastId] = computeLifetime(memref, scopeOp);
  metadata->firstUseId = firstId;
  metadata->lastUseId = lastId;
  analyzeAccessCharacteristics(memref, metadata, scopeOp);
  metadata->hasUniformAccess = analyzeUniformAccess(memref, scopeOp);
  metadata->dominantAccessPattern =
      classifyAccessPattern(memref, metadata, scopeOp);
  metadata->reuseDistance = reuseAnalyzer.computeReuseDistance(memref, scopeOp);

  /// Classify temporal locality based on reuse distance
  if (metadata->reuseDistance)
    metadata->temporalLocality =
        reuseAnalyzer.classifyTemporalLocality(*metadata->reuseDistance);

  /// Classify spatial locality based on access pattern
  if (accessAnalyzer.hasStrideOneAccess(memref, scopeOp))
    metadata->spatialLocality = SpatialLocalityLevel::Excellent;
  else if (metadata->dominantAccessPattern == AccessPatternType::Sequential)
    metadata->spatialLocality = SpatialLocalityLevel::Excellent;
  else if (metadata->dominantAccessPattern == AccessPatternType::Stencil)
    metadata->spatialLocality = SpatialLocalityLevel::Good;
  else if (metadata->dominantAccessPattern == AccessPatternType::Strided)
    metadata->spatialLocality = SpatialLocalityLevel::Good;
  else
    metadata->spatialLocality = SpatialLocalityLevel::Poor;

  metadata->accessedInParallelLoop = isAccessedInsideParallelLoop(memref);
  metadata->hasLoopCarriedDeps = hasLoopCarriedDependencies(memref);
}

bool MemrefAnalyzer::isAccessedInsideParallelLoop(Value memref) const {
  for (Operation *user : memref.getUsers()) {
    for (Operation *parent = user->getParentOp(); parent;
         parent = parent->getParentOp()) {
      if (auto *loopMeta = metadataManager.getLoopMetadata(parent)) {
        if (loopMeta->potentiallyParallel)
          return true;
      }
    }
  }
  return false;
}

bool MemrefAnalyzer::hasLoopCarriedDependencies(Value memref) const {
  for (Operation *user : memref.getUsers()) {
    for (Operation *parent = user->getParentOp(); parent;
         parent = parent->getParentOp()) {
      /// Check if this is a loop operation we track
      if (metadataManager.getLoopMetadata(parent)) {
        /// Use per-memref dependency check instead of loop-level flag
        if (depAnalyzer.hasLoopCarriedDeps(parent, memref))
          return true;
      }
    }
  }
  return false;
}

SmallVector<MemrefMetadata::DimAccessPatternType>
MemrefAnalyzer::computeDimAccessPatterns(Value memref, Operation *scopeOp) {
  SmallVector<MemrefMetadata::DimAccessPatternType> patterns;
  auto type = memref.getType().dyn_cast<MemRefType>();
  if (!type)
    return patterns;

  unsigned rank = type.getRank();
  patterns.assign(rank, DimPattern::Unknown);

  auto classifyIndex =
      [&](Value idx, unsigned dim) -> MemrefMetadata::DimAccessPatternType {
    if (!idx)
      return DimPattern::Unknown;
    if (accessAnalyzer.isConstantIndexValue(idx))
      return DimPattern::Constant;
    if (dim + 1 == rank && accessAnalyzer.isUnitStrideLike(idx))
      return DimPattern::UnitStride;
    if (accessAnalyzer.isAffineIndex(idx))
      return DimPattern::Affine;
    return DimPattern::NonAffine;
  };

  auto updateDim = [&](unsigned dim,
                       MemrefMetadata::DimAccessPatternType candidate) {
    if (dim >= patterns.size())
      return;
    patterns[dim] = mergePattern(patterns[dim], candidate);
  };

  scopeOp->walk([&](Operation *op) {
    if (auto load = dyn_cast<memref::LoadOp>(op)) {
      if (load.getMemref() != memref)
        return;
      for (auto [dim, idx] : llvm::enumerate(load.getIndices()))
        updateDim(dim, classifyIndex(idx, dim));
    } else if (auto store = dyn_cast<memref::StoreOp>(op)) {
      if (store.getMemRef() != memref)
        return;
      for (auto [dim, idx] : llvm::enumerate(store.getIndices()))
        updateDim(dim, classifyIndex(idx, dim));
    } else if (auto affineLoad = dyn_cast<affine::AffineLoadOp>(op)) {
      if (affineLoad.getMemRef() != memref)
        return;
      for (unsigned dim = 0; dim < rank; ++dim)
        updateDim(dim, DimPattern::Affine);
    } else if (auto affineStore = dyn_cast<affine::AffineStoreOp>(op)) {
      if (affineStore.getMemRef() != memref)
        return;
      for (unsigned dim = 0; dim < rank; ++dim)
        updateDim(dim, DimPattern::Affine);
    }
  });

  return patterns;
}

std::optional<int64_t>
MemrefAnalyzer::computeEstimatedAccessBytes(Value memref,
                                            MemrefMetadata *metadata) {
  if (!metadata || !metadata->accessStats.totalAccesses)
    return std::nullopt;

  auto type = memref.getType().dyn_cast<MemRefType>();
  if (!type)
    return std::nullopt;

  uint64_t elemBytes = arts::getElementTypeByteSize(type.getElementType());
  if (!elemBytes)
    return std::nullopt;

  return static_cast<int64_t>(*metadata->accessStats.totalAccesses * elemBytes);
}
