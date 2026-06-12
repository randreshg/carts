///==========================================================================///
/// File: VerifySdeRedistribute.cpp
///
/// Gate for completed SDE redistribution structure.
///
/// `sde-redistribute` must consume temporary layout-disagreement markers into
/// explicit `sde.redist` operations. This verifier rejects residual markers and
/// validates the local geometry carried by each `sde.redist`.
///==========================================================================///

#include "carts/dialect/sde/Analysis/LayoutGraph.h"
#include "carts/dialect/sde/Analysis/RedistributionEdges.h"
#include "carts/dialect/sde/IR/SdeDialect.h"
#include "carts/dialect/sde/Transforms/Passes.h"
#include "carts/utils/ArrayAttrUtils.h"
#include "carts/utils/ValueAnalysis.h"

namespace mlir::carts::sde {
#define GEN_PASS_DEF_VERIFYSDEREDISTRIBUTE
#include "carts/dialect/sde/Transforms/Passes.h.inc"
} // namespace mlir::carts::sde

#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/BuiltinTypes.h"
#include "mlir/Pass/Pass.h"

#include <string>

using namespace mlir;
using namespace mlir::carts;

namespace {

static bool hasNegative(ArrayRef<int64_t> values) {
  return llvm::any_of(values, [](int64_t value) { return value < 0; });
}

static bool hasNonPositive(ArrayRef<int64_t> values) {
  return llvm::any_of(values, [](int64_t value) { return value <= 0; });
}

static bool ownerDimsFitRank(ArrayRef<int64_t> ownerDims, unsigned rank) {
  return llvm::all_of(ownerDims, [&](int64_t dim) {
    return dim >= 0 && static_cast<unsigned>(dim) < rank;
  });
}

static bool blockShapeFitsType(ArrayRef<int64_t> blockShape,
                               MemRefType memrefType) {
  if (blockShape.size() != static_cast<size_t>(memrefType.getRank()) ||
      hasNonPositive(blockShape))
    return false;
  if (!memrefType.hasStaticShape())
    return true;
  ArrayRef<int64_t> shape = memrefType.getShape();
  for (auto [extent, dimExtent] : llvm::zip_equal(blockShape, shape))
    if (ShapedType::isStatic(dimExtent) && extent > dimExtent)
      return false;
  return true;
}

static LogicalResult verifyRedistGeometry(sde::SdeRedistOp redist) {
  bool failed = false;
  auto fail = [&](StringRef message) {
    redist.emitOpError() << "verify-sde-redistribute: " << message;
    failed = true;
  };

  if (!redist.getArrayIdAttr())
    fail("missing array_id");

  auto memrefType = dyn_cast<MemRefType>(redist.getMu().getType());
  if (!memrefType) {
    fail("redistribution root is not a memref");
    return failure();
  }
  unsigned rank = static_cast<unsigned>(memrefType.getRank());

  std::optional<SmallVector<int64_t, 4>> sourceOwnerDims =
      readI64ArrayAttr(redist.getSourceOwnerDims());
  std::optional<SmallVector<int64_t, 4>> sourceBlockShape =
      readI64ArrayAttr(redist.getSourceBlockShape());
  std::optional<SmallVector<int64_t, 4>> targetOwnerDims =
      readI64ArrayAttr(redist.getTargetOwnerDims());
  std::optional<SmallVector<int64_t, 4>> targetBlockShape =
      readI64ArrayAttr(redist.getTargetBlockShape());
  if (!sourceOwnerDims || !targetOwnerDims)
    fail("owner dimensions are not static i64 arrays");
  if (!sourceBlockShape || !targetBlockShape)
    fail("block shapes are not static i64 arrays");
  if (!sourceOwnerDims || !sourceBlockShape || !targetOwnerDims ||
      !targetBlockShape)
    return failure();

  if (!ownerDimsFitRank(*sourceOwnerDims, rank) ||
      !ownerDimsFitRank(*targetOwnerDims, rank))
    fail("owner dimensions do not fit the redistribution root rank");
  if (!blockShapeFitsType(*sourceBlockShape, memrefType) ||
      !blockShapeFitsType(*targetBlockShape, memrefType))
    fail("block shape does not fit the redistribution root type");

  std::optional<SmallVector<int64_t, 4>> haloShape;
  if (redist.getHaloShapeAttr())
    haloShape = readI64ArrayAttr(redist.getHaloShapeAttr());
  if (redist.getFamily() == sde::SdeMovementFamily::halo_like) {
    if (!haloShape) {
      fail("halo_like movement is missing haloShape");
    } else if (haloShape->size() != rank || hasNegative(*haloShape)) {
      fail("haloShape is not a non-negative rank-length i64 array");
    }
  } else if (redist.getHaloShapeAttr()) {
    fail("non-halo movement carries haloShape");
  }

  return failure(failed);
}

static bool hasNonZero(ArrayAttr attr) {
  if (!attr)
    return false;
  return llvm::any_of(attr, [](Attribute value) {
    auto integer = dyn_cast<IntegerAttr>(value);
    return integer && integer.getInt() != 0;
  });
}

static bool consumerHasReadRoot(sde::SdeSuIterateOp consumer,
                                sde::SdeRedistOp redist) {
  IntegerAttr arrayId = redist.getArrayIdAttr();
  if (!arrayId)
    return false;
  Value root = carts::ValueAnalysis::stripMemrefViewOps(redist.getMu());
  for (sde::SdeArrayLayoutRootOp provenance :
       consumer.getBody().getOps<sde::SdeArrayLayoutRootOp>()) {
    if (static_cast<int64_t>(provenance.getArrayId()) != arrayId.getInt() ||
        provenance.getMode() != sde::SdeAccessMode::read)
      continue;
    Value consumerRoot =
        carts::ValueAnalysis::stripMemrefViewOps(provenance.getRoot());
    if (carts::ValueAnalysis::sameMemrefRoot(root, consumerRoot))
      return true;
  }
  return false;
}

static std::optional<sde::LayoutGraphFact>
findConsumerReadFact(sde::SdeSuIterateOp consumer, int64_t arrayId) {
  if (ArrayAttr layout = consumer.getArrayLayoutAttr())
    for (const sde::LayoutGraphFact &fact : sde::parseArrayLayoutFacts(layout))
      if (fact.id == arrayId && fact.role == sde::LayoutGraphRole::read)
        return fact;
  return std::nullopt;
}

static sde::SdeSuIterateOp findAnchoredConsumer(sde::SdeRedistOp redist) {
  for (Operation *next = redist->getNextNode(); next;
       next = next->getNextNode()) {
    if (isa<sde::SdeRedistOp, sde::SdeSuBarrierOp>(next))
      continue;
    return dyn_cast<sde::SdeSuIterateOp>(next);
  }
  return {};
}

static LogicalResult verifyRedistAnchoredInConsumer(sde::SdeRedistOp redist) {
  IntegerAttr arrayId = redist.getArrayIdAttr();
  if (!arrayId)
    return failure();
  sde::SdeSuIterateOp consumer = findAnchoredConsumer(redist);
  if (!consumer)
    return redist.emitOpError()
           << "verify-sde-redistribute: sde.redist is not anchored before a "
              "consumer sde.su_iterate";
  if (!consumerHasReadRoot(consumer, redist))
    return redist.emitOpError()
           << "verify-sde-redistribute: anchored consumer has no matching "
              "read provenance for this redistribution root";

  std::optional<sde::LayoutGraphFact> readFact =
      findConsumerReadFact(consumer, arrayId.getInt());
  if (!readFact)
    return redist.emitOpError()
           << "verify-sde-redistribute: anchored consumer has no committed "
              "read layout for this redistribution array";

  switch (redist.getFamily()) {
  case sde::SdeMovementFamily::halo_like:
    if (!consumer.getPhysicalHaloShapeAttr() &&
        !hasNonZero(consumer.getAccessMinOffsetsAttr()) &&
        !hasNonZero(consumer.getAccessMaxOffsetsAttr()))
      return redist.emitOpError()
             << "verify-sde-redistribute: halo_like movement is not backed by "
                "consumer halo/access-window facts";
    return success();
  case sde::SdeMovementFamily::reduce_scatter_like:
    if (readFact->layoutKind == sde::ArrayLayoutKind::blockContraction ||
        consumer.getPartialReductionAttr() ||
        consumer.getReductionStrategyAttr())
      return success();
    return redist.emitOpError()
           << "verify-sde-redistribute: reduce_scatter_like movement is not "
              "backed by a contraction/reduction consumer";
  default:
    return success();
  }
}

struct VerifySdeRedistributePass
    : public sde::impl::VerifySdeRedistributeBase<VerifySdeRedistributePass> {
  void runOnOperation() override {
    ModuleOp module = getOperation();
    bool hasFailure = false;

    module.walk([&](sde::SdeSuIterateOp su) {
      if (su.getLayoutsDisagreeAttr()) {
        su.emitOpError()
            << "verify-sde-redistribute: residual layout-disagreement marker; "
               "sde-redistribute must consume it into sde.redist";
        hasFailure = true;
      }
    });

    module.walk([&](sde::SdeRedistOp redist) {
      if (failed(verifyRedistGeometry(redist)))
        hasFailure = true;
      std::string reason;
      if (!sde::redistGroundedInCommittedLayout(redist, reason)) {
        redist.emitOpError()
            << "verify-sde-redistribute: sde.redist is not grounded in "
               "committed SDE layout: "
            << reason;
        hasFailure = true;
      }
      if (failed(verifyRedistAnchoredInConsumer(redist)))
        hasFailure = true;
    });

    if (hasFailure)
      signalPassFailure();
  }
};

} // namespace

namespace mlir::carts::sde {
std::unique_ptr<Pass> createVerifySdeRedistributePass() {
  return std::make_unique<VerifySdeRedistributePass>();
}
} // namespace mlir::carts::sde
