///==========================================================================///
/// File: SdeStructuredSummaries.cpp
///
/// Stamp SDE-owned structured summaries before crossing into ARTS IR.
///==========================================================================///

#include "arts/dialect/sde/Analysis/StructuredOpAnalysis.h"
#include "arts/dialect/sde/Transforms/Passes.h"

namespace mlir::arts {
#define GEN_PASS_DEF_SDESTRUCTUREDSUMMARIES
#include "arts/dialect/sde/Transforms/Passes.h.inc"
} // namespace mlir::arts

#include "arts/utils/Debug.h"
ARTS_DEBUG_SETUP(sde_semantic_contracts);

using namespace mlir;
using namespace mlir::arts;

namespace {

static ArrayAttr buildI64ArrayAttr(MLIRContext *ctx, ArrayRef<int64_t> values) {
  SmallVector<Attribute> attrs;
  attrs.reserve(values.size());
  auto i64Type = IntegerType::get(ctx, 64);
  for (int64_t value : values)
    attrs.push_back(IntegerAttr::get(i64Type, value));
  return ArrayAttr::get(ctx, attrs);
}

struct SdeStructuredSummariesPass
    : public arts::impl::SdeStructuredSummariesBase<
          SdeStructuredSummariesPass> {
  void runOnOperation() override {
    getOperation().walk([&](sde::SdeSuIterateOp op) {
      std::optional<sde::StructuredLoopSummary> summary =
          sde::analyzeStructuredLoop(op);
      if (!summary)
        return;

      op.setStructuredClassificationAttr(
          sde::SdeStructuredClassificationAttr::get(&getContext(),
                                                    summary->classification));

      if (summary->classification ==
          sde::SdeStructuredClassification::stencil) {
        std::optional<sde::StructuredNeighborhoodInfo> neighborhoodSummary =
            sde::extractNeighborhoodSummary(*summary);
        if (!neighborhoodSummary)
          return;

        op.setAccessMinOffsetsAttr(buildI64ArrayAttr(
            op.getContext(), neighborhoodSummary->minOffsets));
        op.setAccessMaxOffsetsAttr(buildI64ArrayAttr(
            op.getContext(), neighborhoodSummary->maxOffsets));
        op.setOwnerDimsAttr(
            buildI64ArrayAttr(op.getContext(), neighborhoodSummary->ownerDims));
        op.setSpatialDimsAttr(buildI64ArrayAttr(
            op.getContext(), neighborhoodSummary->spatialDims));
        op.setWriteFootprintAttr(buildI64ArrayAttr(
            op.getContext(), neighborhoodSummary->writeFootprint));

        ARTS_DEBUG("stamped generic SDE structured summary on su_iterate");
        return;
      }

      ARTS_DEBUG("refreshed SDE structured classification on su_iterate");
    });
  }
};

} // namespace

namespace mlir::arts::sde {

std::unique_ptr<Pass> createSdeStructuredSummariesPass() {
  return std::make_unique<SdeStructuredSummariesPass>();
}

} // namespace mlir::arts::sde
