///==========================================================================///
/// File: IndexRedistributor.cpp
///
/// Implementation of IndexRedistributor for index redistribution logic.
///==========================================================================///

#include "arts/Transforms/Datablock/IndexRedistributor.h"
#include "mlir/Dialect/Arith/IR/Arith.h"

using namespace mlir;
using namespace mlir::arts;

std::pair<SmallVector<Value>, SmallVector<Value>>
IndexRedistributor::redistribute(ValueRange loadStoreIndices, OpBuilder &builder,
                                 Location loc) const {
  SmallVector<Value> newOuter, newInner;
  auto zero = [&]() {
    return builder.create<arith::ConstantIndexOp>(loc, 0);
  };

  if (isChunked && chunkSize) {
    // CHUNKED: div/mod on first load/store index
    if (!loadStoreIndices.empty()) {
      Value idx = loadStoreIndices[0];
      newOuter.push_back(builder.create<arith::DivUIOp>(loc, idx, chunkSize));
      newInner.push_back(builder.create<arith::RemUIOp>(loc, idx, chunkSize));
      for (unsigned i = 1;
           i < loadStoreIndices.size() && newInner.size() < innerRank; ++i)
        newInner.push_back(loadStoreIndices[i]);
    }
  } else {
    // ELEMENT-WISE: redistribute indices from load/store to outer
    for (unsigned i = 0; i < outerRank; ++i) {
      if (i < loadStoreIndices.size())
        newOuter.push_back(loadStoreIndices[i]);
      else
        newOuter.push_back(zero());
    }
    for (unsigned i = 0; i < innerRank; ++i) {
      unsigned srcIdx = outerRank + i;
      if (srcIdx < loadStoreIndices.size())
        newInner.push_back(loadStoreIndices[srcIdx]);
      else
        newInner.push_back(zero());
    }
  }

  // Ensure non-empty
  if (newOuter.empty())
    newOuter.push_back(zero());
  if (newInner.empty())
    newInner.push_back(zero());

  return {newOuter, newInner};
}

std::pair<SmallVector<Value>, SmallVector<Value>>
IndexRedistributor::redistributeWithOffset(ValueRange loadStoreIndices,
                                           Value elemOffset, OpBuilder &builder,
                                           Location loc) const {
  SmallVector<Value> newOuter, newInner;
  auto zero = [&]() {
    return builder.create<arith::ConstantIndexOp>(loc, 0);
  };

  for (unsigned i = 0; i < outerRank; ++i) {
    if (i < loadStoreIndices.size()) {
      // Localize the global index to the acquired slice
      Value globalIdx = loadStoreIndices[i];
      Value localIdx =
          builder.create<arith::SubIOp>(loc, globalIdx, elemOffset);
      newOuter.push_back(localIdx);
    } else {
      newOuter.push_back(zero());
    }
  }
  for (unsigned i = 0; i < innerRank; ++i) {
    unsigned srcIdx = outerRank + i;
    if (srcIdx < loadStoreIndices.size())
      newInner.push_back(loadStoreIndices[srcIdx]);
    else
      newInner.push_back(zero());
  }

  // Ensure non-empty
  if (newOuter.empty())
    newOuter.push_back(zero());
  if (newInner.empty())
    newInner.push_back(zero());

  return {newOuter, newInner};
}
