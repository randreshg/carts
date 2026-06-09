///==========================================================================///
/// File: CodirToArtsBlockLocalIndex.h
///
/// Block-local index and grouped block-window index materialization.
///==========================================================================///
#ifndef CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_BLOCKLOCALINDEX_H
#define CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_BLOCKLOCALINDEX_H

#include "CodirToArtsBlockWindowProof.h"
#include "carts/dialect/arts/IR/ArtsDialect.h"
#include "carts/dialect/codir/Utils/CodirConversionUtils.h"
#include "carts/utils/Utils.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "llvm/ADT/STLExtras.h"

namespace {

static inline Value subtractClampZero(OpBuilder &builder, Location loc,
                                      Value value, int64_t amount) {
  if (amount <= 0)
    return value;
  Value offset = createConstantIndex(builder, loc, amount);
  Value zero = createZeroIndex(builder, loc);
  Value canSubtract = arith::CmpIOp::create(
      builder, loc, arith::CmpIPredicate::uge, value, offset);
  Value shifted = arith::SubIOp::create(builder, loc, value, offset);
  return arith::SelectOp::create(builder, loc, canSubtract, shifted, zero);
}

struct PlannedBlockLocalAccessRewrite {
  Value localMemref;
  unsigned ownerDim = 0;
  Value ownerBase;
  Value localOrigin;
  int64_t lowerHalo = 0;
  Value groupedSourcePtr;
  unsigned ownerSlot = 0;
  int64_t blockSize = 1;
  int64_t groupBlockCount = 1;
  int64_t sourceDimExtent = ShapedType::kDynamic;
  bool grouped = false;
  bool allowFullWindowAccess = false;
};

static inline Value materializeBlockLocalOrigin(OpBuilder &builder,
                                                Location loc, Value ownerBase,
                                                Value ownerDomainBase,
                                                int64_t blockSize) {
  Value localOrigin = ownerBase;
  if (blockSize > 1) {
    if (!ownerDomainBase)
      ownerDomainBase = createZeroIndex(builder, loc);
    Value relativeBase =
        ::mlir::carts::ValueAnalysis::sameValue(ownerBase, ownerDomainBase)
            ? createZeroIndex(builder, loc)
            : arith::SubIOp::create(builder, loc, ownerBase, ownerDomainBase)
                  .getResult();
    Value blockSizeValue = createConstantIndex(builder, loc, blockSize);
    Value blockIndex =
        arith::DivUIOp::create(builder, loc, relativeBase, blockSizeValue);
    Value blockOffset =
        arith::MulIOp::create(builder, loc, blockIndex, blockSizeValue);
    localOrigin =
        arith::AddIOp::create(builder, loc, ownerDomainBase, blockOffset);
  }
  return localOrigin;
}

static inline FailureOr<Value>
materializeBlockLocalIndex(OpBuilder &builder, Location loc, Value index,
                           Value ownerBase, Value localOrigin,
                           int64_t lowerHalo) {
  if (!index || !ownerBase || !localOrigin)
    return failure();
  if (lowerHalo > 0) {
    Value halo = createConstantIndex(builder, loc, lowerHalo);
    Value zero = createZeroIndex(builder, loc);
    Value canSubtract = arith::CmpIOp::create(
        builder, loc, arith::CmpIPredicate::uge, localOrigin, halo);
    Value shifted = arith::SubIOp::create(builder, loc, localOrigin, halo);
    localOrigin =
        arith::SelectOp::create(builder, loc, canSubtract, shifted, zero);
  }
  if (::mlir::carts::ValueAnalysis::sameValue(index, localOrigin))
    return createZeroIndex(builder, loc);
  if (auto sub = index.getDefiningOp<arith::SubIOp>())
    if (::mlir::carts::ValueAnalysis::sameValue(sub.getRhs(), localOrigin))
      return index;
  if (!indexSelectsOwnerSlice(index, ownerBase))
    return failure();
  return arith::SubIOp::create(builder, loc, index, localOrigin).getResult();
}

static inline FailureOr<Value> materializeGroupedBlockLocalIndex(
    OpBuilder &builder, Location loc, Value index, Value ownerBase,
    int64_t lowerHalo, int64_t blockSize, int64_t groupBlockCount,
    int64_t sourceDimExtent, Value &relativeBlock,
    bool allowFullWindowAccess = false,
    const DenseMap<Value, Value> *sourceByBlockArgument = nullptr) {
  if (!index || !ownerBase || blockSize <= 0 || groupBlockCount <= 0)
    return failure();
  if (groupBlockCount > std::numeric_limits<int64_t>::max() / blockSize)
    return failure();
  if (!allowFullWindowAccess && !indexSelectsOwnerSlice(index, ownerBase))
    return failure();

  int64_t windowExtent = blockSize * groupBlockCount;
  BlockWindowProof proof{ownerBase, windowExtent, sourceByBlockArgument};
  auto pointStaysInWindow = [&](Value candidate) {
    return proof.pointStaysInWindow(candidate);
  };
  auto upperStaysInWindow = [&](Value candidate) {
    return proof.upperStaysInWindow(candidate);
  };

  bool provenInWindow = pointStaysInWindow(index);
  if (!provenInWindow) {
    if (auto blockArg = dyn_cast<BlockArgument>(index)) {
      auto loop =
          dyn_cast_or_null<scf::ForOp>(blockArg.getOwner()->getParentOp());
      if (loop && loop.getInductionVar() == index &&
          ::mlir::carts::ValueAnalysis::isConstantAtLeastOne(loop.getStep())) {
        provenInWindow = pointStaysInWindow(loop.getLowerBound()) &&
                         upperStaysInWindow(loop.getUpperBound());
      }
    }
  }
  // Full-window read acquires every backing block. A static source extent that
  // fits in that window proves every in-bounds source index selects one of
  // them.
  if (!provenInWindow && allowFullWindowAccess && sourceDimExtent >= 0 &&
      sourceDimExtent <= windowExtent)
    provenInWindow = true;
  if (!provenInWindow)
    return failure();

  Value blockSizeValue = createConstantIndex(builder, loc, blockSize);
  Value relativeIndex =
      ::mlir::carts::ValueAnalysis::sameValue(index, ownerBase)
          ? createZeroIndex(builder, loc)
          : arith::SubIOp::create(builder, loc, index, ownerBase).getResult();
  relativeBlock =
      arith::DivUIOp::create(builder, loc, relativeIndex, blockSizeValue);
  Value blockOffset =
      arith::MulIOp::create(builder, loc, relativeBlock, blockSizeValue);
  Value blockBase = arith::AddIOp::create(builder, loc, ownerBase, blockOffset);

  Value blockPayloadBase = blockBase;
  if (lowerHalo > 0)
    blockPayloadBase = subtractClampZero(builder, loc, blockBase, lowerHalo);

  if (::mlir::carts::ValueAnalysis::sameValue(index, blockPayloadBase))
    return createZeroIndex(builder, loc);
  return arith::SubIOp::create(builder, loc, index, blockPayloadBase)
      .getResult();
}

} // namespace

#endif // CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_BLOCKLOCALINDEX_H
