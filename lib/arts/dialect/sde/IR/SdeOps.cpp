///==========================================================================///
/// File: SdeOps.cpp
/// Defines SDE dialect operation helpers and verifiers.
///==========================================================================///

#include "arts/dialect/sde/IR/SdeDialect.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/IR/OpImplementation.h"

using namespace mlir;
using namespace mlir::arts::sde;

#define GET_OP_CLASSES
#include "arts/dialect/sde/IR/SdeOps.cpp.inc"

SmallVector<Region *> SdeSuIterateOp::getLoopRegions() { return {&getBody()}; }

std::optional<SmallVector<Value>> SdeSuIterateOp::getLoopInductionVars() {
  auto numIVs = getLowerBounds().size();
  if (getBody().getNumArguments() < numIVs)
    return std::nullopt;
  SmallVector<Value> ivs;
  for (unsigned i = 0; i < numIVs; ++i)
    ivs.push_back(getBody().getArgument(i));
  return ivs;
}

std::optional<SmallVector<OpFoldResult>> SdeSuIterateOp::getLoopLowerBounds() {
  return SmallVector<OpFoldResult>(getLowerBounds().begin(),
                                   getLowerBounds().end());
}

std::optional<SmallVector<OpFoldResult>> SdeSuIterateOp::getLoopUpperBounds() {
  return SmallVector<OpFoldResult>(getUpperBounds().begin(),
                                   getUpperBounds().end());
}

std::optional<SmallVector<OpFoldResult>> SdeSuIterateOp::getLoopSteps() {
  return SmallVector<OpFoldResult>(getSteps().begin(), getSteps().end());
}

LogicalResult SdeMuReductionDeclOp::verify() {
  if (auto identity = getIdentity()) {
    auto typedIdentity = dyn_cast<TypedAttr>(*identity);
    if (!typedIdentity)
      return emitOpError() << "expects identity to be a typed attribute";
    if (typedIdentity.getType() != getType())
      return emitOpError() << "expects identity type to match reduction type";
  }

  if (getReductionKind() != SdeReductionKind::custom)
    return success();

  if (getCombiner().empty())
    return emitOpError() << "expects a combiner region for custom reductions";

  Block &entry = getCombiner().front();
  if (entry.getNumArguments() != 2)
    return emitOpError()
           << "expects custom combiner region with two block arguments";
  if (entry.getArgumentTypes()[0] != getType() ||
      entry.getArgumentTypes()[1] != getType())
    return emitOpError()
           << "expects custom combiner arguments to match reduction type";

  auto yield = dyn_cast<SdeYieldOp>(entry.getTerminator());
  if (!yield || yield.getValues().size() != 1 ||
      yield.getValues().front().getType() != getType())
    return emitOpError() << "expects custom combiner to terminate with "
                            "arts_sde.yield of the reduction type";

  return success();
}
