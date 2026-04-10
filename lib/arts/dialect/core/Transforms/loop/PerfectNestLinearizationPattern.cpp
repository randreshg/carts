///==========================================================================///
/// PerfectNestLinearizationPattern.cpp - arts.for + scf.for linearization
///
/// Detects a directly nested perfect loop nest:
///   arts.for %i = lb0 to ub0 step 1 {
///     scf.for %j = lb1 to ub1 step 1 {
///       body(%i, %j)
///     }
///   }
///
/// and rewrites it to:
///   arts.for %linear = 0 to trip0 * trip1 step 1 {
///     %i = lb0 + (%linear div trip1)
///     %j = lb1 + (%linear rem trip1)
///     body(%i, %j)
///   }
///
/// This exposes more parallel work to ARTS without introducing nested task
/// creation. The initial implementation is intentionally narrow:
///   - one-dimensional arts.for
///   - one directly nested scf.for
///   - both loops are unit-stride
///   - no reductions / iter_args
///   - no extra ops around the inner loop
///==========================================================================///

#include "arts/dialect/core/Transforms/loop/LoopNormalizer.h"
#include "arts/utils/Debug.h"
#include "arts/utils/Utils.h"
#include "arts/utils/ValueAnalysis.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/IRMapping.h"
#include "mlir/Interfaces/SideEffectInterfaces.h"

ARTS_DEBUG_SETUP(loop_normalization);

using namespace mlir;
using namespace mlir::arts;

namespace mlir {
namespace arts {

namespace {

static bool isUnitStrideLoop(scf::ForOp loop) {
  auto step = ValueAnalysis::tryFoldConstantIndex(loop.getStep());
  return step && *step == 1;
}

static bool containsNestedLoop(scf::ForOp loop) {
  bool found = false;
  loop.getBody()->walk([&](scf::ForOp nested) {
    if (nested != loop)
      found = true;
  });
  return found;
}

static std::optional<int64_t> getConstantUnitTripCount(scf::ForOp loop) {
  if (!loop || !isUnitStrideLoop(loop))
    return std::nullopt;

  auto lb = ValueAnalysis::tryFoldConstantIndex(loop.getLowerBound());
  auto ub = ValueAnalysis::tryFoldConstantIndex(loop.getUpperBound());
  if (!lb || !ub || *ub < *lb)
    return std::nullopt;
  return *ub - *lb;
}

static std::optional<int64_t>
getRemainingPerfectNestTripProduct(scf::ForOp loop) {
  if (!loop)
    return std::nullopt;

  int64_t product = 1;
  scf::ForOp current = loop;
  while (true) {
    Operation *candidate = nullptr;
    for (Operation &op : current.getBody()->without_terminator()) {
      if (candidate)
        return product;
      candidate = &op;
    }

    auto nested = dyn_cast_or_null<scf::ForOp>(candidate);
    if (!nested)
      return product;

    auto trip = getConstantUnitTripCount(nested);
    if (!trip)
      return std::nullopt;

    product *= *trip;
    current = nested;
  }
}

static int64_t getRemainingPerfectNestDepth(scf::ForOp loop) {
  if (!loop)
    return 0;

  int64_t depth = 0;
  scf::ForOp current = loop;
  while (true) {
    Operation *candidate = nullptr;
    for (Operation &op : current.getBody()->without_terminator()) {
      if (candidate)
        return depth;
      candidate = &op;
    }

    auto nested = dyn_cast_or_null<scf::ForOp>(candidate);
    if (!nested)
      return depth;

    depth++;
    current = nested;
  }
}

static bool isZeroReductionFreeArtsFor(ForOp artsFor) {
  auto step = ValueAnalysis::tryFoldConstantIndex(artsFor.getStep().front());
  return artsFor.getLowerBound().size() == 1 &&
         artsFor.getUpperBound().size() == 1 && artsFor.getStep().size() == 1 &&
         artsFor.getReductionAccumulators().empty() && step && *step == 1;
}

static Value createUnitTripCount(OpBuilder &builder, Location loc, Value lb,
                                 Value ub) {
  Value zero = arts::createZeroIndex(builder, loc);
  Value inRange =
      arith::CmpIOp::create(builder, loc, arith::CmpIPredicate::uge, ub, lb);
  Value diff = arith::SubIOp::create(builder, loc, ub, lb);
  return arith::SelectOp::create(builder, loc, inRange, diff, zero);
}

class PerfectNestLinearizationPattern : public LoopPattern {
public:
  explicit PerfectNestLinearizationPattern(MetadataManager &metadataManager)
      : metadataManager(metadataManager) {}

  bool match(ForOp artsFor) override;
  LogicalResult apply(OpBuilder &builder) override;
  StringRef getName() const override { return "perfect-nest-linearization"; }

private:
  MetadataManager &metadataManager;
  ForOp outerArtsFor = nullptr;
  scf::ForOp innerLoop = nullptr;
  SmallVector<Operation *, 4> preludeOps;
};

bool PerfectNestLinearizationPattern::match(ForOp artsFor) {
  outerArtsFor = nullptr;
  innerLoop = nullptr;
  preludeOps.clear();

  if (!isZeroReductionFreeArtsFor(artsFor))
    return false;

  Block &body = artsFor.getRegion().front();
  if (body.getNumArguments() != 1)
    return false;

  Operation *candidate = nullptr;
  bool sawCandidate = false;
  for (Operation &op : body.without_terminator()) {
    if (auto loop = dyn_cast<scf::ForOp>(&op)) {
      if (candidate)
        return false;
      candidate = loop;
      sawCandidate = true;
      continue;
    }

    if (sawCandidate)
      return false;

    auto effects = dyn_cast<MemoryEffectOpInterface>(&op);
    if (!effects || !effects.hasNoEffect() || op.getNumRegions() != 0)
      return false;
    preludeOps.push_back(&op);
  }
  if (!candidate)
    return false;

  auto nested = dyn_cast<scf::ForOp>(candidate);
  if (!nested || !nested.getInitArgs().empty() || !isUnitStrideLoop(nested))
    return false;
  if (!containsNestedLoop(nested))
    return false;

  Value outerIV = body.getArgument(0);
  if (ValueAnalysis::dependsOn(nested.getLowerBound(), outerIV) ||
      ValueAnalysis::dependsOn(nested.getUpperBound(), outerIV) ||
      ValueAnalysis::dependsOn(nested.getStep(), outerIV))
    return false;

  /// Profitability: only absorb one loop when the remaining perfectly nested
  /// work per linearized iteration is still substantial. This avoids
  /// over-fragmenting small pointwise kernels such as batchnorm, while still
  /// exposing concurrency for deeper nests like pooling.
  constexpr int64_t kMinRemainingInnerWork = 2048;
  auto remainingWork = getRemainingPerfectNestTripProduct(nested);
  if (!remainingWork || *remainingWork < kMinRemainingInnerWork)
    return false;
  if (getRemainingPerfectNestDepth(nested) < 2)
    return false;

  outerArtsFor = artsFor;
  innerLoop = nested;
  return true;
}

LogicalResult PerfectNestLinearizationPattern::apply(OpBuilder &builder) {
  Location loc = outerArtsFor.getLoc();
  builder.setInsertionPoint(outerArtsFor);

  Value zero = arts::createZeroIndex(builder, loc);
  Value one = arts::createOneIndex(builder, loc);
  Value outerLb = outerArtsFor.getLowerBound().front();
  Value outerUb = outerArtsFor.getUpperBound().front();
  Value innerLb = innerLoop.getLowerBound();
  Value innerUb = innerLoop.getUpperBound();

  Value outerTrip = createUnitTripCount(builder, loc, outerLb, outerUb);
  Value innerTrip = createUnitTripCount(builder, loc, innerLb, innerUb);
  Value totalTrip = arith::MulIOp::create(builder, loc, outerTrip, innerTrip);

  auto linearFor =
      ForOp::create(builder, loc, ValueRange{zero}, ValueRange{totalTrip},
                    ValueRange{one}, nullptr, ValueRange{});

  Region &dstRegion = linearFor.getRegion();
  if (dstRegion.empty())
    dstRegion.push_back(new Block());
  Block &dst = dstRegion.front();
  if (dst.getNumArguments() == 0)
    dst.addArgument(builder.getIndexType(), loc);

  OpBuilder bodyBuilder = OpBuilder::atBlockBegin(&dst);
  Value linearIV = dst.getArgument(0);
  Value outerLinear =
      arith::DivUIOp::create(bodyBuilder, loc, linearIV, innerTrip);
  Value innerLinear =
      arith::RemUIOp::create(bodyBuilder, loc, linearIV, innerTrip);
  Value newOuterIV =
      arith::AddIOp::create(bodyBuilder, loc, outerLb, outerLinear);
  Value newInnerIV =
      arith::AddIOp::create(bodyBuilder, loc, innerLb, innerLinear);

  IRMapping mapping;
  mapping.map(outerArtsFor.getRegion().front().getArgument(0), newOuterIV);
  mapping.map(innerLoop.getInductionVar(), newInnerIV);

  for (Operation *op : preludeOps)
    bodyBuilder.clone(*op, mapping);

  for (Operation &op : innerLoop.getBody()->without_terminator())
    bodyBuilder.clone(op, mapping);
  arts::YieldOp::create(bodyBuilder, loc);

  rewriteNormalizedLoop(outerArtsFor.getOperation(), linearFor.getOperation(),
                        metadataManager);
  outerArtsFor.erase();

  ARTS_INFO("Applied perfect-nest linearization on arts.for");
  return success();
}

} // namespace

std::unique_ptr<LoopPattern>
createPerfectNestLinearizationPattern(MetadataManager &metadataManager) {
  return std::make_unique<PerfectNestLinearizationPattern>(metadataManager);
}

} // namespace arts
} // namespace mlir
