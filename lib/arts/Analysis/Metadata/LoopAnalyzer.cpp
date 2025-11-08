///===----------------------------------------------------------------------===///
// LoopAnalyzer.cpp - Loop Analysis Implementation
///===----------------------------------------------------------------------===///

#include "arts/Analysis/Metadata/LoopAnalyzer.h"
#include "arts/Utils/ArtsUtils.h"
#include "arts/Utils/Metadata/LocationMetadata.h"
#include "mlir/Dialect/Affine/Analysis/AffineAnalysis.h"
#include "mlir/Dialect/Affine/Analysis/LoopAnalysis.h"
#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "llvm/ADT/DenseSet.h"

using namespace mlir;
using namespace mlir::arts;

///===----------------------------------------------------------------------===///
// LoopAnalyzer Implementation
///===----------------------------------------------------------------------===///
void LoopAnalyzer::analyzeAffineLoop(affine::AffineForOp forOp,
                                     LoopMetadata *metadata) {
  /// Set location metadata
  metadata->locationMetadata =
      LocationMetadata::fromMLIRLocation(forOp.getLoc());

  /// Compute trip count from affine bounds
  if (forOp.hasConstantBounds()) {
    int64_t lb = forOp.getConstantLowerBound();
    int64_t ub = forOp.getConstantUpperBound();
    int64_t step = forOp.getStep();
    if (step > 0 && ub > lb)
      metadata->tripCount = (ub - lb + step - 1) / step;
  }

  /// Compute nesting level
  int64_t nestLevel = 0;
  Operation *parent = forOp->getParentOp();
  while (parent) {
    if (isa<affine::AffineForOp, scf::ForOp, scf::ParallelOp, scf::ForallOp>(
            parent)) {
      nestLevel++;
    }
    parent = parent->getParentOp();
  }
  metadata->nestingLevel = nestLevel;
  metadata->potentiallyParallel = isLoopParallel(forOp);
  analyzeMemoryAccesses(forOp, metadata);

  metadata->hasInterIterationDeps = depAnalyzer.hasInterIterationDeps(forOp);
  metadata->dataMovementPattern = classifyDataMovement(metadata);
  suggestPartitioning(metadata);
  computeMemoryFootprintPerIter(forOp, metadata);
  /// TODO: Add reduction detection when available
}

void LoopAnalyzer::analyzeSCFLoop(Operation *loopOp, LoopMetadata *metadata) {
  /// Set location metadata
  metadata->locationMetadata =
      LocationMetadata::fromMLIRLocation(loopOp->getLoc());

  /// Compute nesting level
  int64_t nestLevel = 0;
  Operation *parent = loopOp->getParentOp();
  while (parent) {
    if (isa<affine::AffineForOp, scf::ForOp, scf::ParallelOp, scf::ForallOp>(
            parent)) {
      nestLevel++;
    }
    parent = parent->getParentOp();
  }
  metadata->nestingLevel = nestLevel;

  /// For scf.parallel and scf.forall, assume potentially parallel
  if (isa<scf::ParallelOp, scf::ForallOp>(loopOp)) {
    metadata->potentiallyParallel = true;
  } else if (auto forOp = dyn_cast<scf::ForOp>(loopOp)) {
    /// Try to compute trip count for scf.for
    if (auto lbConst =
            forOp.getLowerBound().getDefiningOp<arith::ConstantOp>()) {
      if (auto ubConst =
              forOp.getUpperBound().getDefiningOp<arith::ConstantOp>()) {
        if (auto stepConst =
                forOp.getStep().getDefiningOp<arith::ConstantOp>()) {
          auto lbAttr = lbConst.getValue().dyn_cast<IntegerAttr>();
          auto ubAttr = ubConst.getValue().dyn_cast<IntegerAttr>();
          auto stepAttr = stepConst.getValue().dyn_cast<IntegerAttr>();

          if (lbAttr && ubAttr && stepAttr) {
            int64_t lb = lbAttr.getInt();
            int64_t ub = ubAttr.getInt();
            int64_t step = stepAttr.getInt();
            if (step > 0 && ub > lb) {
              metadata->tripCount = (ub - lb + step - 1) / step;
            }
          }
        }
      }
    }
  }

  analyzeMemoryAccesses(loopOp, metadata);
  metadata->hasInterIterationDeps = depAnalyzer.hasInterIterationDeps(loopOp);
  metadata->dataMovementPattern = classifyDataMovement(metadata);
  suggestPartitioning(metadata);
  computeMemoryFootprintPerIter(loopOp, metadata);
  /// TODO: Add reduction detection when available
}

void LoopAnalyzer::analyzeMemoryAccesses(Operation *loopOp,
                                         LoopMetadata *metadata) {
  int64_t reads = 0, writes = 0;
  bool hasUniformStride = true;
  bool hasGatherScatter = false;

  DenseSet<Value> accessedMemrefs;

  loopOp->walk([&](Operation *op) {
    if (accessAnalyzer.isMemoryAccess(op)) {
      if (isa<affine::AffineLoadOp, memref::LoadOp>(op)) {
        reads++;
      } else if (isa<affine::AffineStoreOp, memref::StoreOp>(op)) {
        writes++;
      }

      /// Track accessed memrefs
      if (Value memref = accessAnalyzer.getAccessedMemref(op)) {
        accessedMemrefs.insert(memref);

        /// Check for stride-1 access
        if (!accessAnalyzer.hasStrideOneAccess(memref, loopOp)) {
          hasUniformStride = false;
        }

        /// Check for gather/scatter (non-affine accesses)
        if (!accessAnalyzer.isAffineAccess(op)) {
          hasGatherScatter = true;
        }
      }
    }
  });

  metadata->readCount = reads;
  metadata->writeCount = writes;
  metadata->hasUniformStride = hasUniformStride;
  metadata->hasGatherScatter = hasGatherScatter;
}

LoopMetadata::DataMovement
LoopAnalyzer::classifyDataMovement(LoopMetadata *metadata) {
  /// Classify based on access patterns
  if (metadata->hasGatherScatter) {
    if (metadata->writeCount > metadata->readCount) 
      return LoopMetadata::DataMovement::Scatter;
    else
      return LoopMetadata::DataMovement::Gather;
  }

  if (metadata->hasUniformStride && *metadata->hasUniformStride)
    return LoopMetadata::DataMovement::Streaming;

  /// Check for tiled access (multiple memrefs with structured access)
  if (metadata->readCount > 0 && metadata->writeCount > 0)
    return LoopMetadata::DataMovement::Tiled;

  return LoopMetadata::DataMovement::Unknown;
}

void LoopAnalyzer::suggestPartitioning(LoopMetadata *metadata) {
  /// Don't suggest partitioning for sequential loops
  if (!metadata->potentiallyParallel) 
    return;

  /// Suggest partitioning based on trip count
  if (metadata->tripCount && *metadata->tripCount > 0) {
    int64_t tripCount = *metadata->tripCount;
    if (tripCount < 100) {
      /// Small loops: use block scheduling with small chunks
      metadata->suggestedPartitioning = LoopMetadata::Partitioning::Block;
      metadata->suggestedChunkSize = std::max<int64_t>(1, tripCount / 4);
    } else if (tripCount < 10000) {
      /// Medium loops: use block scheduling with moderate chunks
      metadata->suggestedPartitioning = LoopMetadata::Partitioning::Block;
      metadata->suggestedChunkSize = std::max<int64_t>(32, tripCount / 64);
    } else {
      /// Large loops: use dynamic scheduling for better load balancing
      metadata->suggestedPartitioning = LoopMetadata::Partitioning::Dynamic;
      metadata->suggestedChunkSize = std::max<int64_t>(64, tripCount / 128);
    }
  } else {
    /// Unknown trip count: suggest auto partitioning
    metadata->suggestedPartitioning = LoopMetadata::Partitioning::Auto;
  }
}

void LoopAnalyzer::computeMemoryFootprintPerIter(Operation *loopOp,
                                                 LoopMetadata *metadata) {
  int64_t footprint = 0;

  DenseSet<Value> uniqueMemrefs;
  loopOp->walk([&](Operation *op) {
    if (Value memref = accessAnalyzer.getAccessedMemref(op))
      uniqueMemrefs.insert(memref);
  });

  /// Sum up element sizes of all accessed memrefs
  for (Value memref : uniqueMemrefs) {
    if (auto memrefType = memref.getType().dyn_cast<MemRefType>()) {
      Type elemType = memrefType.getElementType();
      unsigned elemBits = elemType.getIntOrFloatBitWidth();
      int64_t elemBytes = (elemBits + 7) / 8;
      footprint += elemBytes;
    }
  }

  metadata->memoryFootprintPerIter = footprint;
}
