///==========================================================================///
/// File: SdeScheduleRefinement.cpp
///
/// Cost-model-backed SDE schedule refinement. The current implementation keeps
/// the legality gate deliberately narrow: it only rewrites one-dimensional
/// constant-trip loops whose source schedule remained abstract (`auto` or
/// `runtime`), and selects the cheapest concrete SDE schedule with the active
/// cost model before chunk synthesis runs.
///==========================================================================///

#include "arts/dialect/sde/Transforms/Passes.h"
namespace mlir::arts {
#define GEN_PASS_DEF_SDESCHEDULEREFINEMENT
#include "arts/dialect/sde/Transforms/Passes.h.inc"
} // namespace mlir::arts

#include "arts/utils/ValueAnalysis.h"
#include "arts/utils/costs/SDECostModel.h"

#include "mlir/IR/BuiltinOps.h"

#include <array>

using namespace mlir;
using namespace mlir::arts;

namespace {

static bool isScheduleRefinementCandidate(sde::SdeScheduleKindAttr schedule) {
  if (!schedule)
    return false;
  return schedule.getValue() == sde::SdeScheduleKind::auto_ ||
         schedule.getValue() == sde::SdeScheduleKind::runtime;
}

static std::optional<int64_t> getConstantTripCount(sde::SdeSuIterateOp op) {
  if (op.getLowerBounds().size() != 1 || op.getUpperBounds().size() != 1 ||
      op.getSteps().size() != 1)
    return std::nullopt;

  int64_t lowerBound = 0;
  int64_t upperBound = 0;
  int64_t step = 0;
  if (!ValueAnalysis::getConstantIndex(op.getLowerBounds().front(),
                                       lowerBound) ||
      !ValueAnalysis::getConstantIndex(op.getUpperBounds().front(),
                                       upperBound) ||
      !ValueAnalysis::getConstantIndex(op.getSteps().front(), step) ||
      step <= 0)
    return std::nullopt;

  if (upperBound <= lowerBound)
    return int64_t{0};

  int64_t span = upperBound - lowerBound;
  return (span + step - 1) / step;
}

static sde::SdeScheduleKind selectSchedule(sde::SDECostModel &costModel,
                                           int64_t tripCount) {
  static constexpr std::array<sde::SdeScheduleKind, 3> kCandidateSchedules = {
      sde::SdeScheduleKind::static_, sde::SdeScheduleKind::guided,
      sde::SdeScheduleKind::dynamic};

  sde::SdeScheduleKind bestSchedule = kCandidateSchedules.front();
  double bestCost = costModel.getSchedulingOverhead(bestSchedule, tripCount);
  for (sde::SdeScheduleKind candidate :
       ArrayRef(kCandidateSchedules).drop_front()) {
    double candidateCost =
        costModel.getSchedulingOverhead(candidate, tripCount);
    if (candidateCost < bestCost) {
      bestSchedule = candidate;
      bestCost = candidateCost;
    }
  }
  return bestSchedule;
}

struct SdeScheduleRefinementPass
    : public arts::impl::SdeScheduleRefinementBase<
          SdeScheduleRefinementPass> {
  explicit SdeScheduleRefinementPass(sde::SDECostModel *costModel = nullptr)
      : costModel(costModel) {}

  void runOnOperation() override {
    if (!costModel)
      return;

    getOperation().walk([&](sde::SdeSuIterateOp op) {
      if (op.getChunkSize() ||
          !isScheduleRefinementCandidate(op.getScheduleAttr()))
        return;

      std::optional<int64_t> tripCount = getConstantTripCount(op);
      if (!tripCount)
        return;

      auto refinedSchedule = sde::SdeScheduleKindAttr::get(
          &getContext(), selectSchedule(*costModel, *tripCount));
      if (refinedSchedule == op.getScheduleAttr())
        return;
      op.setScheduleAttr(refinedSchedule);
    });
  }

private:
  sde::SDECostModel *costModel = nullptr;
};

} // namespace

namespace mlir::arts::sde {

std::unique_ptr<Pass>
createSdeScheduleRefinementPass(sde::SDECostModel *costModel) {
  return std::make_unique<SdeScheduleRefinementPass>(costModel);
}

} // namespace mlir::arts::sde
