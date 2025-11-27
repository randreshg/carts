///===----------------------------------------------------------------------===///
// MemrefAnalyzer.cpp - Memory Reference Analysis Implementation
///===----------------------------------------------------------------------===///

#include "arts/Analysis/Metadata/MemrefAnalyzer.h"
#include "arts/Analysis/Metadata/ArtsMetadataManager.h"
#include "arts/Utils/ArtsUtils.h"
#include "arts/Utils/Metadata/ArtsMetadata.h"
#include "arts/Utils/Metadata/LocationMetadata.h"
#include "mlir/Dialect/Affine/Analysis/AffineAnalysis.h"
#include "mlir/Dialect/Affine/Analysis/LoopAnalysis.h"
#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "llvm/ADT/DenseSet.h"
#include <algorithm>

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
// ReuseAnalyzer Implementation
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
// MemrefAnalyzer Implementation
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

/// Classify dominant access pattern (sequential, strided, random)
AccessPatternType
MemrefAnalyzer::classifyAccessPattern(Value memref, MemrefMetadata *metadata,
                                      Operation *scopeOp) {
  /// Check for stride-1 sequential access
  if (accessAnalyzer.hasStrideOneAccess(memref, scopeOp))
    return AccessPatternType::Sequential;

  /// Check for uniform strided access
  if (metadata->allAccessesAffine && *metadata->allAccessesAffine &&
      analyzeUniformAccess(memref, scopeOp))
    return AccessPatternType::Strided;

  /// Check for gather/scatter patterns (non-affine or complex affine)
  if (metadata->hasNonAffineAccesses && *metadata->hasNonAffineAccesses)
    return AccessPatternType::GatherScatter;

  /// Default to random if pattern is unclear
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

  if (metadata->accessStats.totalAccesses &&
      *metadata->accessStats.totalAccesses > 0) {
    metadata->accessStats.readWriteRatio =
        static_cast<double>(reads) / *metadata->accessStats.totalAccesses;
  }

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
  else if (metadata->dominantAccessPattern == AccessPatternType::Strided)
    metadata->spatialLocality = SpatialLocalityLevel::Good;
  else if (metadata->dominantAccessPattern == AccessPatternType::Sequential)
    metadata->spatialLocality = SpatialLocalityLevel::Excellent;
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
      if (auto *loopMeta = metadataManager.getLoopMetadata(parent)) {
        if (loopMeta->hasInterIterationDeps && *loopMeta->hasInterIterationDeps)
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
