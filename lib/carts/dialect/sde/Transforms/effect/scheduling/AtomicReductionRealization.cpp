///==========================================================================///
/// File: AtomicReductionRealization.cpp
///
/// SDE-owned realization of atomic reductions into explicit leaf CU work.
///==========================================================================///

#include "carts/dialect/sde/Transforms/Passes.h"
namespace mlir::carts::sde {
#define GEN_PASS_DEF_SDEATOMICREDUCTIONREALIZATION
#include "carts/dialect/sde/Transforms/Passes.h.inc"
} // namespace mlir::carts::sde

#include "carts/dialect/sde/Analysis/LayoutGraph.h"
#include "carts/dialect/sde/Analysis/SdeAnalysisUtils.h"
#include "carts/utils/ValueAnalysis.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/BuiltinTypes.h"
#include "llvm/ADT/STLExtras.h"

using namespace mlir;
using namespace mlir::carts;

namespace {

struct AtomicReductionMatch {
  memref::LoadOp accumulatorLoad;
  Operation *combiner = nullptr;
  memref::StoreOp store;
  Value partial;
};

static bool isIntegerAdd(Operation *op) {
  return isa_and_nonnull<arith::AddIOp>(op);
}

static Type getAccumulatorElementType(Value accumulator) {
  if (auto shapedType = dyn_cast<ShapedType>(accumulator.getType()))
    return shapedType.getElementType();
  return accumulator.getType();
}

static bool hasNestedSequentialLoop(sde::SdeSuIterateOp op) {
  bool found = false;
  op->walk([&](Operation *nested) {
    if (isa<scf::ForOp, affine::AffineForOp>(nested)) {
      found = true;
      return WalkResult::interrupt();
    }
    return WalkResult::advance();
  });
  return found;
}

static bool isAtomicReductionCandidate(sde::SdeSuIterateOp op) {
  if (op.getReductionAccumulators().empty() || op.getPartialReductionAttr())
    return false;
  if (hasNestedSequentialLoop(op))
    return false;
  ArrayAttr kindsAttr = op.getReductionKindsAttr();
  if (!kindsAttr || kindsAttr.size() != op.getReductionAccumulators().size())
    return false;
  return llvm::all_of(
      llvm::zip(kindsAttr, op.getReductionAccumulators()), [](auto pair) {
        auto [attr, accumulator] = pair;
        auto kindAttr = dyn_cast<sde::SdeReductionKindAttr>(attr);
        return kindAttr && kindAttr.getValue() == sde::SdeReductionKind::add &&
               !isa<FloatType>(getAccumulatorElementType(accumulator));
      });
}

static LogicalResult
parseReductionKinds(sde::SdeSuIterateOp op,
                    SmallVectorImpl<sde::SdeReductionKind> &kinds) {
  ArrayAttr kindsAttr = op.getReductionKindsAttr();
  if (!kindsAttr || kindsAttr.size() != op.getReductionAccumulators().size())
    return op.emitOpError()
           << "has atomic reduction strategy without one reduction kind per "
              "accumulator";

  kinds.clear();
  kinds.reserve(kindsAttr.size());
  for (Attribute attr : kindsAttr) {
    auto kindAttr = dyn_cast<sde::SdeReductionKindAttr>(attr);
    if (!kindAttr)
      return op.emitOpError() << "has non-SDE reduction kind attribute";
    kinds.push_back(kindAttr.getValue());
  }
  return success();
}

static bool allIndicesAddressBase(ValueRange indices) {
  return llvm::all_of(indices, [](Value index) {
    return ValueAnalysis::isZeroConstant(index);
  });
}

static std::optional<AtomicReductionMatch>
matchAtomicAddStore(Value accumulator, memref::StoreOp store) {
  if (!ValueAnalysis::sameMemrefRoot(accumulator, store.getMemref()))
    return std::nullopt;

  Operation *combiner = store.getValueToStore().getDefiningOp();
  if (!isIntegerAdd(combiner) || !combiner->getResult(0).hasOneUse())
    return std::nullopt;

  for (unsigned operandIdx = 0; operandIdx < combiner->getNumOperands();
       ++operandIdx) {
    auto load =
        combiner->getOperand(operandIdx).getDefiningOp<memref::LoadOp>();
    if (!load || !load.getResult().hasOneUse())
      continue;
    if (!ValueAnalysis::sameDirectMemrefAccess(
            load.getMemref(), load.getIndices(), store.getMemref(),
            store.getIndices()))
      continue;

    unsigned partialIdx = operandIdx == 0 ? 1 : 0;
    return AtomicReductionMatch{load, combiner, store,
                                combiner->getOperand(partialIdx)};
  }

  return std::nullopt;
}

static LogicalResult realizeAccumulatorAtomic(sde::SdeSuIterateOp op,
                                              Value accumulator,
                                              sde::SdeReductionKind kind) {
  if (kind != sde::SdeReductionKind::add)
    return op.emitOpError() << "cannot realize non-add atomic reduction in SDE";

  Block *computeBlock = sde::getSuIterateComputeBlock(op);
  if (!computeBlock)
    return op.emitOpError() << "has no leaf CU body for atomic reduction";

  SmallVector<memref::StoreOp, 4> stores;
  computeBlock->walk([&](memref::StoreOp store) {
    if (ValueAnalysis::sameMemrefRoot(accumulator, store.getMemref()))
      stores.push_back(store);
  });
  if (stores.empty())
    return op.emitOpError()
           << "has atomic reduction strategy but no accumulator store in the "
              "leaf CU body";

  std::optional<AtomicReductionMatch> selected;
  for (memref::StoreOp store : stores) {
    std::optional<AtomicReductionMatch> match =
        matchAtomicAddStore(accumulator, store);
    if (!match)
      return store.emitOpError()
             << "cannot realize atomic reduction; every accumulator store "
                "in the leaf CU must be a load/add/store update";
    if (!allIndicesAddressBase(store.getIndices()))
      return store.emitOpError()
             << "cannot realize indexed atomic reduction; SDE cu_atomic "
                "currently addresses the base memref element";
    if (selected)
      return op.emitOpError()
             << "has multiple stores to one atomic reduction accumulator";
    selected = *match;
  }

  if (!selected)
    return op.emitOpError()
           << "could not rewrite atomic reduction; expected a leaf-CU "
              "load/add/store update of the accumulator";

  OpBuilder builder(selected->store);
  sde::SdeCuAtomicOp::create(
      builder, selected->store.getLoc(),
      sde::SdeReductionKindAttr::get(op.getContext(), kind),
      selected->store.getMemref(), selected->partial);

  selected->store.erase();
  if (selected->combiner->use_empty())
    selected->combiner->erase();
  if (selected->accumulatorLoad->use_empty())
    selected->accumulatorLoad.erase();
  return success();
}

static void clearConsumedReductionMetadata(sde::SdeSuIterateOp op) {
  op.getReductionAccumulatorsMutable().clear();
  op->removeAttr(op.getReductionKindsAttrName());
}

struct SdeAtomicReductionRealizationPass
    : public sde::impl::SdeAtomicReductionRealizationBase<
          SdeAtomicReductionRealizationPass> {
  void runOnOperation() override {
    SmallVector<sde::SdeSuIterateOp, 8> loops;
    getOperation().walk([&](sde::SdeSuIterateOp op) { loops.push_back(op); });

    for (sde::SdeSuIterateOp op : loops) {
      if (!isAtomicReductionCandidate(op))
        continue;
      if (op.getReductionAccumulators().empty()) {
        op.emitOpError()
            << "has atomic-eligible reduction without reduction accumulators";
        signalPassFailure();
        return;
      }

      SmallVector<sde::SdeReductionKind> kinds;
      if (failed(parseReductionKinds(op, kinds))) {
        signalPassFailure();
        return;
      }

      for (auto [accumulator, kind] :
           llvm::zip(op.getReductionAccumulators(), kinds)) {
        if (failed(realizeAccumulatorAtomic(op, accumulator, kind))) {
          signalPassFailure();
          return;
        }
      }
      clearConsumedReductionMetadata(op);
    }
  }
};

} // namespace

namespace mlir::carts::sde {

std::unique_ptr<Pass> createSdeAtomicReductionRealizationPass() {
  return std::make_unique<SdeAtomicReductionRealizationPass>();
}

} // namespace mlir::carts::sde
