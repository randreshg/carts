///==========================================================================///
/// File: DistributionFailClosed.cpp
///
/// SDE distribution fail-closed gate (A7). Refuses multi-worker lowering of
/// in-place self-read neighborhood stencils (Gauss-Seidel family) whose
/// loop-carried neighbor offsets would require an unimplemented SDE
/// wavefront/skew transform. Logic carved verbatim from the correctness-base
/// @782988ad1 DistributionPlanning pass.
///==========================================================================///

#include "carts/dialect/sde/Transforms/Passes.h"
namespace mlir::carts::sde {
#define GEN_PASS_DEF_DISTRIBUTIONFAILCLOSED
#include "carts/dialect/sde/Transforms/Passes.h.inc"
} // namespace mlir::carts::sde

#include "carts/dialect/sde/Analysis/SdeAnalysisUtils.h"
#include "carts/dialect/sde/Analysis/SuLoopAccessAnalysis.h"
#include "carts/dialect/sde/Transforms/effect/distribution/DistributionFailClosed.h"
#include "carts/dialect/sde/Utils/SDECostModel.h"
#include "carts/utils/ArrayAttrUtils.h"

#include "llvm/ADT/STLExtras.h"
#include "llvm/Support/raw_ostream.h"

#include <optional>
#include <string>

using namespace mlir;
using namespace mlir::carts;

namespace {

static bool hasLoopCarriedNeighborOffsets(sde::SdeSuIterateOp op) {
  std::optional<sde::SuNeighborhoodAccessInfo> neighborhood =
      sde::queryNeighborhoodAccessInfo(op);
  if (neighborhood) {
    for (auto [minOffset, maxOffset] :
         llvm::zip_equal(neighborhood->minOffsets, neighborhood->maxOffsets))
      if (minOffset < 0 || maxOffset > 0)
        return true;
    return false;
  }

  std::optional<SmallVector<int64_t, 4>> mins =
      readI64ArrayAttr(op.getAccessMinOffsetsAttr());
  std::optional<SmallVector<int64_t, 4>> maxs =
      readI64ArrayAttr(op.getAccessMaxOffsetsAttr());
  if (!mins || !maxs || mins->size() != maxs->size())
    return false;
  for (auto [minOffset, maxOffset] : llvm::zip_equal(*mins, *maxs))
    if (minOffset < 0 || maxOffset > 0)
      return true;
  return false;
}

static std::string formatI64Array(ArrayAttr attr) {
  std::string text;
  llvm::raw_string_ostream os(text);
  os << '[';
  if (auto values = readI64ArrayAttr(attr))
    llvm::interleaveComma(*values, os);
  os << ']';
  return os.str();
}

} // namespace

namespace mlir::carts::sde::distribution {

bool requiresInPlaceSelfRawWavefrontFailClosed(sde::SdeSuIterateOp op,
                                               sde::SDECostModel &costModel) {
  if (costModel.getLogicalWorkerCapacity() <= 1)
    return false;
  if (sde::queryInPlaceSafe(op))
    return false;
  auto classification = sde::queryStructuredClassification(op);
  if (!classification ||
      *classification != sde::SdeStructuredClassification::stencil)
    return false;
  if (!sde::queryInPlaceSharedState(op))
    return false;
  if (!hasLoopCarriedNeighborOffsets(op))
    return false;
  return true;
}

void emitStencilWavefrontFailClosed(sde::SdeSuIterateOp op) {
  op.emitOpError()
      << "in-place self-read stencil (Gauss-Seidel family) has loop-carried "
         "neighbor offsets min="
      << formatI64Array(op.getAccessMinOffsetsAttr())
      << " max=" << formatI64Array(op.getAccessMaxOffsetsAttr())
      << " on owner dims " << formatI64Array(op.getOwnerDimsAttr())
      << "; exposing legal parallelism requires an SDE wavefront/skew "
         "(loop-skewing) transform that is not implemented. Distributing in "
         "place without it would violate Gauss-Seidel ordering; preserving "
         "the order is serial, so the planner refuses multi-worker lowering. "
         "Implement the SDE wavefront/skew transform, "
         "prove the loop in-place-safe, or compile with a single logical "
         "worker.";
}

} // namespace mlir::carts::sde::distribution

using namespace mlir::carts::sde::distribution;

namespace {

struct DistributionFailClosedPass
    : public sde::impl::DistributionFailClosedBase<DistributionFailClosedPass> {
  explicit DistributionFailClosedPass(sde::SDECostModel *costModel = nullptr)
      : costModel(costModel) {}

  void runOnOperation() override {
    if (!costModel)
      return;

    bool failed = false;
    getOperation().walk([&](sde::SdeSuIterateOp op) {
      if (requiresInPlaceSelfRawWavefrontFailClosed(op, *costModel)) {
        emitStencilWavefrontFailClosed(op);
        failed = true;
      }
    });

    if (failed)
      signalPassFailure();
  }

private:
  sde::SDECostModel *costModel = nullptr;
};

} // namespace

namespace mlir::carts::sde {

std::unique_ptr<Pass>
createDistributionFailClosedPass(sde::SDECostModel *costModel) {
  return std::make_unique<DistributionFailClosedPass>(costModel);
}

} // namespace mlir::carts::sde
