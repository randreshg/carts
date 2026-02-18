///==========================================================================///
/// LoopNormalizer.cpp - Shared utilities for loop normalization patterns
///==========================================================================///

#include "arts/Transforms/Loop/LoopNormalizer.h"
#include "arts/Analysis/Db/DbPatternMatchers.h"
#include "arts/Utils/Metadata/LoopMetadata.h"
#include "arts/Utils/OperationAttributes.h"

using namespace mlir;
using namespace mlir::arts;

void arts::copyLoopAttributes(Operation *source, Operation *target) {
  if (!source || !target)
    return;
  if (auto idAttr = source->getAttr(AttrNames::Operation::ArtsId))
    target->setAttr(AttrNames::Operation::ArtsId, idAttr);
  if (auto loopAttr = source->getAttr(AttrNames::LoopMetadata::Name))
    target->setAttr(AttrNames::LoopMetadata::Name, loopAttr);
}

void arts::updateTripCountMetadata(Operation *loop, int64_t newTripCount) {
  if (!loop)
    return;
  auto attr =
      loop->getAttrOfType<LoopMetadataAttr>(AttrNames::LoopMetadata::Name);
  if (!attr)
    return;

  auto *ctx = loop->getContext();
  auto updated = LoopMetadataAttr::get(
      ctx, attr.getPotentiallyParallel(), attr.getHasReductions(),
      attr.getReductionKinds(),
      /*tripCount=*/IntegerAttr::get(IntegerType::get(ctx, 64), newTripCount),
      attr.getNestingLevel(), attr.getHasInterIterationDeps(),
      attr.getMemrefsWithLoopCarriedDeps(), attr.getParallelClassification(),
      attr.getLocationKey());
  loop->setAttr(AttrNames::LoopMetadata::Name, updated);
}

std::optional<int64_t> arts::getTriangularOffset(Value lb, Value outerIV) {
  return matchTriangularOffset(lb, outerIV);
}
