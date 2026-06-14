///==========================================================================///
/// File: AffineIndexUtils.cpp
///==========================================================================///

#include "carts/dialect/sde/Analysis/AffineIndexUtils.h"
#include "carts/utils/ValueAnalysis.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/IR/AffineMap.h"

using namespace mlir;
using namespace mlir::carts;

namespace mlir::carts::sde {

std::optional<AffineExpr> tryGetAffineExpr(Value value, ArrayRef<Value> ivs,
                                             MLIRContext *ctx) {
  for (auto [idx, iv] : llvm::enumerate(ivs)) {
    if (value == iv)
      return getAffineDimExpr(idx, ctx);
  }

  auto *defOp = value.getDefiningOp();
  if (!defOp)
    return std::nullopt;

  if (auto cst = dyn_cast<arith::ConstantOp>(defOp)) {
    if (auto intAttr = dyn_cast<IntegerAttr>(cst.getValue()))
      return getAffineConstantExpr(intAttr.getInt(), ctx);
    return std::nullopt;
  }

  if (auto addOp = dyn_cast<arith::AddIOp>(defOp)) {
    auto lhs = tryGetAffineExpr(addOp.getLhs(), ivs, ctx);
    auto rhs = tryGetAffineExpr(addOp.getRhs(), ivs, ctx);
    if (lhs && rhs)
      return *lhs + *rhs;
  }

  if (auto subOp = dyn_cast<arith::SubIOp>(defOp)) {
    auto lhs = tryGetAffineExpr(subOp.getLhs(), ivs, ctx);
    auto rhs = tryGetAffineExpr(subOp.getRhs(), ivs, ctx);
    if (lhs && rhs)
      return *lhs - *rhs;
  }

  if (auto mulOp = dyn_cast<arith::MulIOp>(defOp)) {
    auto lhs = tryGetAffineExpr(mulOp.getLhs(), ivs, ctx);
    auto rhs = tryGetAffineExpr(mulOp.getRhs(), ivs, ctx);
    if (lhs && rhs &&
        (isa<AffineConstantExpr>(*lhs) || isa<AffineConstantExpr>(*rhs)))
      return *lhs * *rhs;
  }

  if (auto castOp = dyn_cast<arith::IndexCastOp>(defOp))
    return tryGetAffineExpr(castOp.getIn(), ivs, ctx);

  auto tryAffineBin = [&](Value lhsVal, Value rhsVal,
                          AffineExprKind kind) -> std::optional<AffineExpr> {
    auto lhs = tryGetAffineExpr(lhsVal, ivs, ctx);
    if (!lhs)
      return std::nullopt;
    int64_t rhsCst = 0;
    if (!ValueAnalysis::getConstantIndex(rhsVal, rhsCst) || rhsCst <= 0)
      return std::nullopt;
    auto rhsExpr = getAffineConstantExpr(rhsCst, ctx);
    switch (kind) {
    case AffineExprKind::FloorDiv:
      return (*lhs).floorDiv(rhsExpr);
    case AffineExprKind::Mod:
      return *lhs % rhsExpr;
    default:
      return std::nullopt;
    }
  };

  if (auto divOp = dyn_cast<arith::DivSIOp>(defOp))
    return tryAffineBin(divOp.getLhs(), divOp.getRhs(), AffineExprKind::FloorDiv);
  if (auto remOp = dyn_cast<arith::RemSIOp>(defOp))
    return tryAffineBin(remOp.getLhs(), remOp.getRhs(), AffineExprKind::Mod);
  if (auto divOp = dyn_cast<arith::DivUIOp>(defOp))
    return tryAffineBin(divOp.getLhs(), divOp.getRhs(), AffineExprKind::FloorDiv);
  if (auto remOp = dyn_cast<arith::RemUIOp>(defOp))
    return tryAffineBin(remOp.getLhs(), remOp.getRhs(), AffineExprKind::Mod);

  return std::nullopt;
}

std::optional<AffineDimOffset> extractDimOffset(AffineExpr expr) {
  if (auto dimExpr = dyn_cast<AffineDimExpr>(expr))
    return AffineDimOffset{dimExpr.getPosition(), 0};
  if (auto cstExpr = dyn_cast<AffineConstantExpr>(expr))
    return AffineDimOffset{std::nullopt, cstExpr.getValue()};

  auto binExpr = dyn_cast<AffineBinaryOpExpr>(expr);
  if (!binExpr)
    return std::nullopt;

  auto lhs = extractDimOffset(binExpr.getLHS());
  auto rhs = extractDimOffset(binExpr.getRHS());
  if (!lhs || !rhs)
    return std::nullopt;

  switch (binExpr.getKind()) {
  case AffineExprKind::Add:
    if (lhs->dim && rhs->dim)
      return std::nullopt;
    return AffineDimOffset{lhs->dim ? lhs->dim : rhs->dim,
                           lhs->offset + rhs->offset};
  case AffineExprKind::Mul: {
    if (lhs->dim && !rhs->dim)
      return AffineDimOffset{*lhs->dim, lhs->offset * rhs->offset};
    if (rhs->dim && !lhs->dim)
      return AffineDimOffset{*rhs->dim, rhs->offset * lhs->offset};
    return std::nullopt;
  }
  case AffineExprKind::FloorDiv:
  case AffineExprKind::Mod:
    if (!lhs->dim || rhs->dim || rhs->offset == 0)
      return std::nullopt;
    return AffineDimOffset{*lhs->dim, 0};
  default:
    return std::nullopt;
  }
}

bool hasConstantOffsets(AffineMap map) {
  for (AffineExpr result : map.getResults()) {
    auto dimOffset = extractDimOffset(result);
    if (dimOffset && dimOffset->dim && dimOffset->offset != 0)
      return true;
  }
  return false;
}

std::optional<int64_t> tryGetUnitNeighborhoodOffset(Value expr, Value iv) {
  MLIRContext *ctx = expr.getContext();
  std::optional<AffineExpr> affine = tryGetAffineExpr(expr, {iv}, ctx);
  if (!affine)
    return std::nullopt;
  *affine = simplifyAffineExpr(*affine, 1, 0);
  AffineExpr dim = getAffineDimExpr(0, ctx);
  if (*affine == dim)
    return 0;
  if (auto add = dyn_cast<AffineBinaryOpExpr>(*affine)) {
    if (add.getKind() != AffineExprKind::Add)
      return std::nullopt;
    if (add.getLHS() == dim) {
      if (auto cst = dyn_cast<AffineConstantExpr>(add.getRHS()))
        if (cst.getValue() >= -1 && cst.getValue() <= 1)
          return cst.getValue();
    }
    if (add.getRHS() == dim) {
      if (auto cst = dyn_cast<AffineConstantExpr>(add.getLHS()))
        if (cst.getValue() >= -1 && cst.getValue() <= 1)
          return -cst.getValue();
    }
  }
  return std::nullopt;
}

} // namespace mlir::carts::sde
