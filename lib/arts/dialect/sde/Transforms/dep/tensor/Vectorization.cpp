///==========================================================================///
/// File: Vectorization.cpp
///
/// Carrier-aware vectorization width analysis for su_iterate bodies.
/// Walks carrier-authoritative linalg.generic ops and stamps
/// sde.vectorize_width on the enclosing su_iterate based on element type
/// and static loop ranges. The actual vectorization is performed downstream
/// by LoopVectorizationHints (in rt/) which emits LLVM vectorization metadata.
///
/// Only analyzes carriers with:
/// - All-parallel iterator types (no reduction vectorization yet)
/// - Static shapes on all operands and loop ranges
/// - Tensor semantics (not buffer)
/// Defaults: f32->8, f64->4, i32->8, i64->4.
///==========================================================================///

#include "arts/dialect/sde/Transforms/Passes.h"
namespace mlir::arts {
#define GEN_PASS_DEF_VECTORIZATION
#include "arts/dialect/sde/Transforms/Passes.h.inc"
} // namespace mlir::arts

#include "arts/utils/Debug.h"
#include "arts/utils/OperationAttributes.h"
#include "mlir/Dialect/Linalg/IR/Linalg.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"

ARTS_DEBUG_SETUP(vectorization);

using namespace mlir;
using namespace mlir::arts;

namespace {

/// Get default vectorization width for an element type.
static int64_t getDefaultVectorWidth(Type elemTy) {
  if (elemTy.isF32())
    return 8;
  if (elemTy.isF64())
    return 4;
  if (elemTy.isInteger(32))
    return 8;
  if (elemTy.isInteger(64))
    return 4;
  return 4;
}

/// Check whether a body block is carrier-authoritative: it contains a
/// linalg.generic carrier but NO scalar executable ops (memref.load/store).
static bool isCarrierAuthoritative(Block &body) {
  bool hasCarrier = false;
  bool hasScalar = false;
  for (Operation &op : body.without_terminator()) {
    if (isa<linalg::GenericOp>(op)) {
      hasCarrier = true;
      continue;
    }
    if (isa<memref::LoadOp, memref::StoreOp>(op)) {
      hasScalar = true;
      continue;
    }
    op.walk([&](Operation *nested) {
      if (isa<memref::LoadOp, memref::StoreOp>(nested))
        hasScalar = true;
    });
  }
  return hasCarrier && !hasScalar;
}

/// Check if a linalg.generic is a vectorization candidate:
/// - All iterator types must be parallel (no reduction vectorization yet)
/// - All shapes must be static
/// - Must have tensor semantics
/// - Must have at least one output
static bool isVectorizationCandidate(linalg::GenericOp generic) {
  for (auto iterType : generic.getIteratorTypesArray()) {
    if (iterType != utils::IteratorType::parallel)
      return false;
  }
  if (generic.hasPureBufferSemantics())
    return false;
  for (OpOperand &operand : generic->getOpOperands()) {
    auto ty = dyn_cast<RankedTensorType>(operand.get().getType());
    if (!ty || !ty.hasStaticShape())
      return false;
  }
  for (int64_t range : generic.getStaticLoopRanges()) {
    if (ShapedType::isDynamic(range))
      return false;
  }
  return generic.getNumDpsInits() > 0;
}

struct VectorizationPass
    : public arts::impl::VectorizationBase<VectorizationPass> {
  using arts::impl::VectorizationBase<VectorizationPass>::VectorizationBase;

  void runOnOperation() override {
    ModuleOp module = getOperation();
    unsigned stamped = 0;

    module.walk([&](sde::SdeSuIterateOp iterOp) {
      // Skip if already stamped.
      if (iterOp->hasAttr(AttrNames::Operation::Sde::VectorizeWidth))
        return;

      Block *computeBlock = sde::getSuIterateComputeBlock(iterOp);
      if (!computeBlock)
        return;
      if (!isCarrierAuthoritative(*computeBlock))
        return;

      // Find the first vectorizable carrier.
      linalg::GenericOp carrier;
      computeBlock->walk([&](linalg::GenericOp generic) {
        if (!carrier && isVectorizationCandidate(generic))
          carrier = generic;
      });
      if (!carrier)
        return;

      // Determine width from element type.
      Type elemTy = cast<RankedTensorType>(carrier.getDpsInits()[0].getType())
                        .getElementType();
      int64_t width = getDefaultVectorWidth(elemTy);

      // Clamp to innermost loop range if smaller.
      SmallVector<int64_t> ranges = carrier.getStaticLoopRanges();
      if (!ranges.empty())
        width = std::min(width, ranges.back());

      iterOp->setAttr(
          AttrNames::Operation::Sde::VectorizeWidth,
          IntegerAttr::get(IndexType::get(&getContext()), width));
      ++stamped;
    });

    if (stamped > 0)
      ARTS_INFO("Vectorization: stamped width on " << stamped
                                                    << " su_iterate op(s)");
    else
      ARTS_DEBUG("Vectorization: no vectorizable carriers found");
  }
};

} // namespace

namespace mlir::arts::sde {

std::unique_ptr<Pass> createVectorizationPass() {
  return std::make_unique<VectorizationPass>();
}

} // namespace mlir::arts::sde
