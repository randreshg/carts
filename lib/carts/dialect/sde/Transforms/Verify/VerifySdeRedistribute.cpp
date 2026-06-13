///==========================================================================///
/// File: VerifySdeRedistribute.cpp
///
/// Gate for completed SDE redistribution structure.
///
/// Companion of `sde-redistribute`. Rejects residual layout-disagreement
/// scaffolding, any retired `sde.redist` carrier, and validates grounding and
/// consumer anchoring for the live SU movement ops.
///==========================================================================///

#include "carts/dialect/sde/Analysis/LayoutGraph.h"
#include "carts/dialect/sde/Analysis/RedistributionEdges.h"
#include "carts/dialect/sde/IR/SdeDialect.h"
#include "carts/dialect/sde/Transforms/Passes.h"
#include "carts/dialect/sde/Utils/SdeCommittedFactUtils.h"
#include "carts/utils/ArrayAttrUtils.h"
#include "carts/utils/ValueAnalysis.h"

namespace mlir::carts::sde {
#define GEN_PASS_DEF_VERIFYSDEREDISTRIBUTE
#include "carts/dialect/sde/Transforms/Passes.h.inc"

bool movementEndpointGroundedInCommittedLayout(
    Operation *moduleOp, Value movementRoot, int64_t arrayId,
    ArrayRef<int64_t> ownerDims, ArrayRef<int64_t> blockShape,
    bool allowExpandedFull, std::string &reason);
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

static LogicalResult verifyMovementEndpointGeometry(
    Operation *movement, Value root, ArrayAttr ownerDimsAttr,
    ArrayAttr blockShapeAttr, std::optional<ArrayAttr> haloShapeAttr) {
  bool failed = false;
  auto fail = [&](StringRef message) {
    movement->emitOpError() << "verify-sde-redistribute: " << message;
    failed = true;
  };

  auto memrefType = dyn_cast<MemRefType>(root.getType());
  if (!memrefType) {
    fail("redistribution root is not a memref");
    return failure();
  }
  unsigned rank = static_cast<unsigned>(memrefType.getRank());

  std::optional<SmallVector<int64_t, 4>> ownerDims =
      readI64ArrayAttr(ownerDimsAttr);
  std::optional<SmallVector<int64_t, 4>> blockShape =
      readI64ArrayAttr(blockShapeAttr);
  if (!ownerDims)
    fail("owner dimensions are not a static i64 array");
  if (!blockShape)
    fail("block shape is not a static i64 array");
  if (!ownerDims || !blockShape)
    return failure();

  if (!ownerDimsFitRank(*ownerDims, rank))
    fail("owner dimensions do not fit the redistribution root rank");
  if (!blockShapeFitsType(*blockShape, memrefType))
    fail("block shape does not fit the redistribution root type");

  if (haloShapeAttr) {
    std::optional<SmallVector<int64_t, 4>> haloShape =
        readI64ArrayAttr(*haloShapeAttr);
    if (!haloShape) {
      fail("haloShape is not a static i64 array");
    } else if (haloShape->size() != rank || hasNegative(*haloShape)) {
      fail("haloShape is not a non-negative rank-length i64 array");
    }
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

static bool consumerHasReadRoot(sde::SdeSuIterateOp consumer, int64_t arrayId,
                                Value movementRoot) {
  Value root = carts::ValueAnalysis::stripMemrefViewOps(movementRoot);
  for (sde::SdeArrayLayoutRootOp provenance :
       consumer.getBody().getOps<sde::SdeArrayLayoutRootOp>()) {
    if (static_cast<int64_t>(provenance.getArrayId()) != arrayId ||
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

static sde::SdeSuIterateOp findAnchoredConsumer(Operation *movement) {
  for (Operation *next = movement->getNextNode(); next;
       next = next->getNextNode()) {
    if (isa<sde::SdeRedistOp, sde::SdeSuBarrierOp, sde::SdeSuHaloOp,
            sde::SdeSuReduceScatterOp>(next))
      continue;
    return dyn_cast<sde::SdeSuIterateOp>(next);
  }
  return {};
}

static LogicalResult verifyMovementAnchoredInConsumer(
    Operation *movement, int64_t arrayId, Value movementRoot,
    bool requireHaloBacking, bool requireReductionBacking) {
  sde::SdeSuIterateOp consumer = findAnchoredConsumer(movement);
  if (!consumer)
    return movement->emitOpError()
           << "verify-sde-redistribute: movement op is not anchored before a "
              "consumer sde.su_iterate";
  if (!consumerHasReadRoot(consumer, arrayId, movementRoot))
    return movement->emitOpError()
           << "verify-sde-redistribute: anchored consumer has no matching "
              "read provenance for this redistribution root";

  std::optional<sde::LayoutGraphFact> readFact =
      findConsumerReadFact(consumer, arrayId);
  if (!readFact)
    return movement->emitOpError()
           << "verify-sde-redistribute: anchored consumer has no committed "
              "read layout for this redistribution array";

  if (requireHaloBacking) {
    if (!sde::deriveCommittedHaloShape(consumer) &&
        !hasNonZero(consumer.getAccessMinOffsetsAttr()) &&
        !hasNonZero(consumer.getAccessMaxOffsetsAttr()))
      return movement->emitOpError()
             << "verify-sde-redistribute: halo movement is not backed by "
                "consumer halo/access-window facts";
  }
  if (requireReductionBacking) {
    if (readFact->layoutKind == sde::ArrayLayoutKind::blockContraction ||
        consumer.getPartialReductionAttr())
      return success();
    return movement->emitOpError()
           << "verify-sde-redistribute: reduce-scatter movement is not backed "
              "by a contraction/reduction consumer";
  }
  return success();
}

static LogicalResult verifySuHalo(sde::SdeSuHaloOp halo, ModuleOp module) {
  IntegerAttr arrayId = halo.getArrayIdAttr();
  if (!arrayId)
    return halo.emitOpError() << "verify-sde-redistribute: missing array_id";
  if (failed(verifyMovementEndpointGeometry(halo, halo.getMu(),
                                            halo.getOwnerDims(),
                                            halo.getBlockShape(),
                                            halo.getHaloShape())))
    return failure();

  std::optional<SmallVector<int64_t, 4>> ownerDims =
      readI64ArrayAttr(halo.getOwnerDims());
  std::optional<SmallVector<int64_t, 4>> blockShape =
      readI64ArrayAttr(halo.getBlockShape());
  if (!ownerDims || !blockShape)
    return failure();

  std::string reason;
  if (!sde::movementEndpointGroundedInCommittedLayout(
          module, halo.getMu(), arrayId.getInt(), *ownerDims, *blockShape,
          /*allowExpandedFull=*/true, reason))
    return halo.emitOpError()
           << "verify-sde-redistribute: sde.su_halo is not grounded in "
              "committed SDE layout: "
           << reason;

  return verifyMovementAnchoredInConsumer(halo, arrayId.getInt(), halo.getMu(),
                                          /*requireHaloBacking=*/true,
                                          /*requireReductionBacking=*/false);
}

static LogicalResult verifySuReduceScatter(sde::SdeSuReduceScatterOp reduce,
                                           ModuleOp module) {
  IntegerAttr arrayId = reduce.getArrayIdAttr();
  if (!arrayId)
    return reduce.emitOpError()
           << "verify-sde-redistribute: missing array_id";
  if (failed(verifyMovementEndpointGeometry(reduce, reduce.getMu(),
                                            reduce.getOwnerDims(),
                                            reduce.getBlockShape(),
                                            std::nullopt)))
    return failure();

  std::optional<SmallVector<int64_t, 4>> ownerDims =
      readI64ArrayAttr(reduce.getOwnerDims());
  std::optional<SmallVector<int64_t, 4>> blockShape =
      readI64ArrayAttr(reduce.getBlockShape());
  if (!ownerDims || !blockShape)
    return failure();

  std::string reason;
  if (!sde::movementEndpointGroundedInCommittedLayout(
          module, reduce.getMu(), arrayId.getInt(), *ownerDims, *blockShape,
          /*allowExpandedFull=*/false, reason))
    return reduce.emitOpError()
           << "verify-sde-redistribute: sde.su_reduce_scatter is not grounded "
              "in committed SDE layout: "
           << reason;

  return verifyMovementAnchoredInConsumer(
      reduce, arrayId.getInt(), reduce.getMu(),
      /*requireHaloBacking=*/false, /*requireReductionBacking=*/true);
}

struct VerifySdeRedistributePass
    : public sde::impl::VerifySdeRedistributeBase<VerifySdeRedistributePass> {
  void runOnOperation() override {
    ModuleOp module = getOperation();
    bool hasFailure = false;

    module.walk([&](sde::SdeRedistOp redist) {
      redist.emitOpError()
          << "verify-sde-redistribute: sde.redist is retired on all movement "
             "families; use "
          << sde::suMovementReplacementForFamily(redist.getFamily());
      hasFailure = true;
    });

    module.walk([&](sde::SdeSuHaloOp halo) {
      if (failed(verifySuHalo(halo, module)))
        hasFailure = true;
    });

    module.walk([&](sde::SdeSuReduceScatterOp reduce) {
      if (failed(verifySuReduceScatter(reduce, module)))
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
