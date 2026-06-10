///==========================================================================///
/// File: CodirToArtsHostCopyNests.h
///
/// Recursive element-copy and block-payload copy nests.
///==========================================================================///
#ifndef CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_HOSTCOPYNESTS_H
#define CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_HOSTCOPYNESTS_H

#include "carts/utils/Utils.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/Location.h"
#include "mlir/IR/Value.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/SmallVector.h"

namespace {

using llvm::ArrayRef;
using llvm::SmallVector;
using llvm::SmallVectorImpl;
using mlir::Location;
using mlir::OpBuilder;
using mlir::Value;
using mlir::carts::createOneIndex;
using mlir::carts::createZeroIndex;
namespace arith = mlir::arith;
namespace memref = mlir::memref;
namespace scf = mlir::scf;

static inline void materializeHostBlockElementCopyNest(
    OpBuilder &builder, Location loc, Value hostView, Value blockPayload,
    ArrayRef<Value> copySizes, ArrayRef<Value> hostOffsets,
    ArrayRef<Value> blockOffsets, bool copyIntoBlock,
    SmallVectorImpl<Value> &indices) {
  unsigned dim = indices.size();
  if (dim == copySizes.size()) {
    SmallVector<Value> hostIndices;
    SmallVector<Value> blockIndices;
    hostIndices.reserve(indices.size());
    blockIndices.reserve(indices.size());
    for (auto [idx, value] : llvm::enumerate(indices)) {
      if (idx < blockOffsets.size() && blockOffsets[idx]) {
        blockIndices.push_back(
            arith::AddIOp::create(builder, loc, blockOffsets[idx], value));
      } else {
        blockIndices.push_back(value);
      }
      if (idx < hostOffsets.size() && hostOffsets[idx]) {
        hostIndices.push_back(
            arith::AddIOp::create(builder, loc, hostOffsets[idx], value));
        continue;
      }
      hostIndices.push_back(value);
    }

    if (copyIntoBlock) {
      Value loaded =
          memref::LoadOp::create(builder, loc, hostView, hostIndices);
      memref::StoreOp::create(builder, loc, loaded, blockPayload, blockIndices);
      return;
    }
    Value loaded =
        memref::LoadOp::create(builder, loc, blockPayload, blockIndices);
    memref::StoreOp::create(builder, loc, loaded, hostView, hostIndices);
    return;
  }

  Value zero = createZeroIndex(builder, loc);
  Value one = createOneIndex(builder, loc);
  auto loop = scf::ForOp::create(builder, loc, zero, copySizes[dim], one);

  OpBuilder::InsertionGuard guard(builder);
  builder.setInsertionPointToStart(loop.getBody());
  indices.push_back(loop.getInductionVar());
  materializeHostBlockElementCopyNest(builder, loc, hostView, blockPayload,
                                      copySizes, hostOffsets, blockOffsets,
                                      copyIntoBlock, indices);
  indices.pop_back();
}

static inline void
materializePerBlockCopyNest(OpBuilder &builder, Location loc, Value srcPayload,
                            Value dstPayload, ArrayRef<Value> copySizes,
                            SmallVectorImpl<Value> &indices) {
  unsigned dim = indices.size();
  if (dim == copySizes.size()) {
    Value loaded = memref::LoadOp::create(builder, loc, srcPayload, indices);
    memref::StoreOp::create(builder, loc, loaded, dstPayload, indices);
    return;
  }
  Value zero = createZeroIndex(builder, loc);
  Value one = createOneIndex(builder, loc);
  auto loop = scf::ForOp::create(builder, loc, zero, copySizes[dim], one);
  OpBuilder::InsertionGuard guard(builder);
  builder.setInsertionPointToStart(loop.getBody());
  indices.push_back(loop.getInductionVar());
  materializePerBlockCopyNest(builder, loc, srcPayload, dstPayload, copySizes,
                              indices);
  indices.pop_back();
}

static inline void materializePerBlockOffsetCopyNest(
    OpBuilder &builder, Location loc, Value srcPayload, Value dstPayload,
    ArrayRef<Value> copySizes, ArrayRef<Value> srcOffsets,
    ArrayRef<Value> dstOffsets, SmallVectorImpl<Value> &indices) {
  unsigned dim = indices.size();
  if (dim == copySizes.size()) {
    SmallVector<Value> srcIndices;
    SmallVector<Value> dstIndices;
    srcIndices.reserve(indices.size());
    dstIndices.reserve(indices.size());
    for (auto [idx, induction] : llvm::enumerate(indices)) {
      srcIndices.push_back(
          arith::AddIOp::create(builder, loc, srcOffsets[idx], induction));
      dstIndices.push_back(
          arith::AddIOp::create(builder, loc, dstOffsets[idx], induction));
    }
    Value loaded = memref::LoadOp::create(builder, loc, srcPayload, srcIndices);
    memref::StoreOp::create(builder, loc, loaded, dstPayload, dstIndices);
    return;
  }
  Value zero = createZeroIndex(builder, loc);
  Value one = createOneIndex(builder, loc);
  auto loop = scf::ForOp::create(builder, loc, zero, copySizes[dim], one);
  OpBuilder::InsertionGuard guard(builder);
  builder.setInsertionPointToStart(loop.getBody());
  indices.push_back(loop.getInductionVar());
  materializePerBlockOffsetCopyNest(builder, loc, srcPayload, dstPayload,
                                    copySizes, srcOffsets, dstOffsets, indices);
  indices.pop_back();
}

/// Build the per-element summing nest that settles one output block by reducing
/// the P per-tile partial payloads with `+=` (arith.addf) and writing the
/// result ONCE. This is the addf dual of materializePerBlockCopyNest: instead
/// of a single source copy, the leaf loads tile 0, accumulates tiles 1..P-1
/// with arith.addf, and stores once into the settled block. All payloads are
/// block payloads indexed identically (no coarse host offset).
static inline void materializePerBlockSumNest(OpBuilder &builder, Location loc,
                                              ArrayRef<Value> partialPayloads,
                                              Value dstPayload,
                                              ArrayRef<Value> copySizes,
                                              SmallVectorImpl<Value> &indices) {
  unsigned dim = indices.size();
  if (dim == copySizes.size()) {
    Value acc =
        memref::LoadOp::create(builder, loc, partialPayloads.front(), indices);
    for (size_t tile = 1; tile < partialPayloads.size(); ++tile) {
      Value next =
          memref::LoadOp::create(builder, loc, partialPayloads[tile], indices);
      acc = arith::AddFOp::create(builder, loc, acc, next);
    }
    memref::StoreOp::create(builder, loc, acc, dstPayload, indices);
    return;
  }
  Value zero = createZeroIndex(builder, loc);
  Value one = createOneIndex(builder, loc);
  auto loop = scf::ForOp::create(builder, loc, zero, copySizes[dim], one);
  OpBuilder::InsertionGuard guard(builder);
  builder.setInsertionPointToStart(loop.getBody());
  indices.push_back(loop.getInductionVar());
  materializePerBlockSumNest(builder, loc, partialPayloads, dstPayload,
                             copySizes, indices);
  indices.pop_back();
}

} // namespace

#endif // CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_HOSTCOPYNESTS_H
