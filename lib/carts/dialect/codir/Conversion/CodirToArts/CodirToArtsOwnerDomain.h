///==========================================================================///
/// File: CodirToArtsOwnerDomain.h
///
/// Owner dispatch-domain discovery and domain-base materialization helpers.
///==========================================================================///
#ifndef CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_OWNERDOMAIN_H
#define CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_OWNERDOMAIN_H

#include "CodirToArtsOwnerSliceContainment.h"

namespace {

static inline scf::ForOp findCodirOwnerDispatchLoop(codir::CodeletOp codelet) {
  scf::ForOp nearestLoop;
  for (Operation *parent = codelet ? codelet->getParentOp() : nullptr; parent;
       parent = parent->getParentOp()) {
    auto loop = dyn_cast<scf::ForOp>(parent);
    if (loop && !nearestLoop)
      nearestLoop = loop;
    if (loop && containsValue(codelet.getParams(), loop.getInductionVar()))
      return loop;
  }
  if (hasCodirTileOwnerSlicePlan(codelet))
    return nearestLoop;
  return {};
}

static inline scf::ForOp findCodirOwnerDispatchLoop(codir::CodeletOp codelet,
                                                    Value ownerParam) {
  for (Operation *parent = codelet ? codelet->getParentOp() : nullptr; parent;
       parent = parent->getParentOp()) {
    auto loop = dyn_cast<scf::ForOp>(parent);
    if (loop && loop.getInductionVar() == ownerParam)
      return loop;
  }
  return {};
}

static inline Value getCodirOwnerDomainLower(codir::CodeletOp codelet) {
  if (auto loop = findCodirOwnerDispatchLoop(codelet))
    return loop.getLowerBound();
  return {};
}

static inline bool dependsOnAncestorLoop(Value value,
                                         codir::CodeletOp codelet) {
  if (!value || !codelet)
    return false;
  for (Operation *parent = codelet->getParentOp(); parent;
       parent = parent->getParentOp()) {
    auto loop = dyn_cast<scf::ForOp>(parent);
    if (loop &&
        ::mlir::carts::ValueAnalysis::dependsOn(value, loop.getInductionVar()))
      return true;
  }
  return false;
}

static inline Value
deriveCodirOwnerDomainLowerFromParam(codir::CodeletOp codelet,
                                     Value ownerParam) {
  Value stripped = ::mlir::carts::ValueAnalysis::stripNumericCasts(ownerParam);
  auto add = stripped ? stripped.getDefiningOp<arith::AddIOp>() : nullptr;
  if (!add)
    return {};

  Value lhs = add.getLhs();
  Value rhs = add.getRhs();
  bool lhsDependsOnDispatch = dependsOnAncestorLoop(lhs, codelet);
  bool rhsDependsOnDispatch = dependsOnAncestorLoop(rhs, codelet);
  if (lhsDependsOnDispatch == rhsDependsOnDispatch)
    return {};
  return lhsDependsOnDispatch ? rhs : lhs;
}

static inline Value getCodirOwnerDomainLower(codir::CodeletOp codelet,
                                             Value ownerParam) {
  if (auto loop = findCodirOwnerDispatchLoop(codelet, ownerParam))
    return loop.getLowerBound();
  if (Value lower = deriveCodirOwnerDomainLowerFromParam(codelet, ownerParam))
    return lower;
  return getCodirOwnerDomainLower(codelet);
}

static inline Value getCodirOwnerDomainUpper(codir::CodeletOp codelet) {
  if (auto loop = findCodirOwnerDispatchLoop(codelet))
    return loop.getUpperBound();
  return {};
}

static inline Value materializePositiveDifferenceOrZero(OpBuilder &builder,
                                                        Location loc, Value end,
                                                        Value start) {
  Value zero = createZeroIndex(builder, loc);
  Value hasPositiveExtent = arith::CmpIOp::create(
      builder, loc, arith::CmpIPredicate::ugt, end, start);
  Value difference = arith::SubIOp::create(builder, loc, end, start);
  return arith::SelectOp::create(builder, loc, hasPositiveExtent, difference,
                                 zero);
}

static inline Value materializeCodirOwnerDomainBase(OpBuilder &builder,
                                                    Location loc,
                                                    codir::CodeletOp codelet) {
  if (Value lower = getCodirOwnerDomainLower(codelet))
    return lower;
  return createZeroIndex(builder, loc);
}

static inline Value materializeCodirOwnerDomainBase(OpBuilder &builder,
                                                    Location loc,
                                                    codir::CodeletOp codelet,
                                                    Value ownerParam) {
  if (Value lower = getCodirOwnerDomainLower(codelet, ownerParam))
    return lower;
  return createZeroIndex(builder, loc);
}

} // namespace

#endif // CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_OWNERDOMAIN_H
