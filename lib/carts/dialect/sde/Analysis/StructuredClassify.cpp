///==========================================================================///
/// File: StructuredClassify.cpp
///
/// Structured-pattern classification for SDE scheduling-unit loops: derives
/// per-loop iterator types (parallel/reduction) from the output access maps and
/// classifies the nest as elementwise / stencil / reduction / matmul, plus the
/// rank-1 reduction-carrier subset check. Backs the public
/// analyzeSuLoopAccesses wrapper. Pattern-free: classification comes only from
/// affine maps and iterator types.
///==========================================================================///

#include "SuLoopAccessAnalysisDetail.h"

#include "carts/dialect/sde/Analysis/AffineIndexUtils.h"
#include "carts/dialect/sde/Analysis/SuLoopAccessAnalysis.h"

#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "llvm/ADT/SmallBitVector.h"

using namespace mlir;
using namespace mlir::carts;

namespace mlir::carts::sde {
namespace {

static bool hasExactDimUse(AffineMap map, const llvm::SmallBitVector &expected,
                           unsigned numDims) {
  return detail::getUsedDims(map, numDims) == expected;
}

static bool hasCanonicalMatmulAccessShape(
    ArrayRef<MemrefAccessEntry> reads, ArrayRef<AffineMap> outputMaps,
    ArrayRef<utils::IteratorType> iterTypes, unsigned numDims) {
  SmallVector<unsigned, 2> parallelDims;
  SmallVector<unsigned, 1> reductionDims;
  for (auto [dim, type] : llvm::enumerate(iterTypes)) {
    if (type == utils::IteratorType::parallel)
      parallelDims.push_back(dim);
    else
      reductionDims.push_back(dim);
  }

  if (parallelDims.size() != 2 || reductionDims.size() != 1 || numDims != 3)
    return false;

  llvm::SmallBitVector outputDims(numDims);
  outputDims.set(parallelDims[0]);
  outputDims.set(parallelDims[1]);
  bool hasMatmulOutput = llvm::any_of(outputMaps, [&](AffineMap map) {
    return hasExactDimUse(map, outputDims, numDims);
  });
  if (!hasMatmulOutput)
    return false;

  llvm::SmallBitVector lhsDims(numDims);
  lhsDims.set(parallelDims[0]);
  lhsDims.set(reductionDims[0]);
  llvm::SmallBitVector rhsDims(numDims);
  rhsDims.set(reductionDims[0]);
  rhsDims.set(parallelDims[1]);

  bool hasLhs = false;
  bool hasRhs = false;
  for (const MemrefAccessEntry &read : reads) {
    hasLhs |= hasExactDimUse(read.indexingMap, lhsDims, numDims);
    hasRhs |= hasExactDimUse(read.indexingMap, rhsDims, numDims);
  }
  return hasLhs && hasRhs;
}

static bool isConstantZeroIndexingMap(AffineMap indexingMap) {
  if (indexingMap.getNumResults() != 1)
    return false;

  auto constant = dyn_cast<AffineConstantExpr>(indexingMap.getResult(0));
  return constant && constant.getValue() == 0;
}

} // namespace

namespace detail {

void computeIteratorTypes(unsigned numDims, ArrayRef<AffineMap> outputMaps,
                          SmallVectorImpl<utils::IteratorType> &iterTypes) {
  llvm::SmallBitVector dimsInOutputs(numDims, false);
  for (AffineMap map : outputMaps) {
    for (unsigned i = 0; i < map.getNumResults(); ++i) {
      map.getResult(i).walk([&](AffineExpr expr) {
        if (auto dimExpr = dyn_cast<AffineDimExpr>(expr))
          dimsInOutputs.set(dimExpr.getPosition());
      });
    }
  }

  for (unsigned d = 0; d < numDims; ++d) {
    iterTypes.push_back(dimsInOutputs.test(d) ? utils::IteratorType::parallel
                                              : utils::IteratorType::reduction);
  }
}

llvm::SmallBitVector getUsedDims(AffineMap map, unsigned numDims) {
  llvm::SmallBitVector used(numDims);
  for (AffineExpr result : map.getResults())
    for (unsigned dim = 0; dim < numDims; ++dim)
      if (result.isFunctionOfDim(dim))
        used.set(dim);
  return used;
}

SdeStructuredClassification
classifyPattern(ArrayRef<MemrefAccessEntry> reads,
                ArrayRef<AffineMap> outputMaps,
                ArrayRef<utils::IteratorType> iterTypes, unsigned numDims) {
  unsigned numParallel = 0;
  unsigned numReduction = 0;
  for (utils::IteratorType t : iterTypes) {
    if (t == utils::IteratorType::parallel)
      ++numParallel;
    else
      ++numReduction;
  }

  if (numReduction == 0) {
    for (const auto &entry : reads) {
      if (hasConstantOffsets(entry.indexingMap))
        return SdeStructuredClassification::stencil;
    }
    return SdeStructuredClassification::elementwise;
  }

  if (numParallel == 2 && numReduction == 1 && numDims == 3 &&
      reads.size() >= 2 && !outputMaps.empty() &&
      hasCanonicalMatmulAccessShape(reads, outputMaps, iterTypes, numDims))
    return SdeStructuredClassification::matmul;

  return SdeStructuredClassification::reduction;
}

bool supportsReductionCarrierSubset(SdeSuIterateOp iterOp,
                                    const LoopNestInfo &nest,
                                    ArrayRef<MemrefAccessEntry> reads,
                                    ArrayRef<MemrefAccessEntry> writes) {
  if (iterOp.getReductionAccumulators().size() != 1 || nest.ivs.size() != 1)
    return false;
  if (writes.size() != 1)
    return false;

  Value reductionAccumulator = iterOp.getReductionAccumulators().front();
  auto reductionType = dyn_cast<MemRefType>(reductionAccumulator.getType());
  if (!reductionType || reductionType.getRank() != 1)
    return false;

  const MemrefAccessEntry &write = writes.front();
  if (write.memref != reductionAccumulator ||
      !isConstantZeroIndexingMap(write.indexingMap))
    return false;

  unsigned matchingReductionReads = 0;
  for (const MemrefAccessEntry &read : reads) {
    if (read.memref != reductionAccumulator)
      continue;
    if (!isConstantZeroIndexingMap(read.indexingMap))
      return false;
    ++matchingReductionReads;
  }

  return matchingReductionReads == 1;
}

} // namespace detail

} // namespace mlir::carts::sde
