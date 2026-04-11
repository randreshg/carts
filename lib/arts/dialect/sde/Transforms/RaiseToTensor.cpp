///==========================================================================///
/// File: RaiseToTensor.cpp
///
/// Rewrites transient memref-backed linalg.generic carriers inside SDE to
/// tensor-backed form so later SDE passes can apply tensor/linalg
/// transformations without leaking tensor IR into ARTS-specific lowering.
/// The original loop/memref body remains authoritative and SDE->ARTS consumes
/// the carriers later.
///==========================================================================///

#include "arts/dialect/sde/Transforms/Passes.h"
namespace mlir::arts {
#define GEN_PASS_DEF_RAISETOTENSOR
#include "arts/dialect/sde/Transforms/Passes.h.inc"
} // namespace mlir::arts

#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/IRMapping.h"

using namespace mlir;
using namespace mlir::arts;

namespace {

static RankedTensorType getRankedTensorType(Value memref) {
  return dyn_cast<RankedTensorType>(
      memref::getTensorTypeFromMemRefType(memref.getType()));
}

static SmallVector<Value>
buildTensorInputs(OpBuilder &builder, Location loc, ValueRange memrefInputs) {
  SmallVector<Value> tensorInputs;
  tensorInputs.reserve(memrefInputs.size());
  for (Value input : memrefInputs) {
    RankedTensorType tensorType = getRankedTensorType(input);
    if (!tensorType)
      return {};
    tensorInputs.push_back(
        bufferization::ToTensorOp::create(builder, loc, tensorType, input));
  }
  return tensorInputs;
}

static SmallVector<Value> buildTensorOutputs(OpBuilder &builder, Location loc,
                                             ValueRange memrefOutputs,
                                             bool useEmptyTensorInit) {
  SmallVector<Value> tensorOutputs;
  tensorOutputs.reserve(memrefOutputs.size());
  for (Value output : memrefOutputs) {
    RankedTensorType tensorType = getRankedTensorType(output);
    if (!tensorType)
      return {};

    if (useEmptyTensorInit) {
      tensorOutputs.push_back(tensor::EmptyOp::create(
          builder, loc, memref::getMixedSizes(builder, loc, output),
          tensorType.getElementType()));
      continue;
    }

    tensorOutputs.push_back(
        bufferization::ToTensorOp::create(builder, loc, tensorType, output));
  }
  return tensorOutputs;
}

static linalg::GenericOp rewriteGenericToTensor(linalg::GenericOp oldGeneric,
                                                bool useEmptyTensorInit) {
  OpBuilder builder(oldGeneric);
  Location loc = oldGeneric.getLoc();

  SmallVector<Value> tensorInputs =
      buildTensorInputs(builder, loc, oldGeneric.getDpsInputs());
  SmallVector<Value> tensorOutputs = buildTensorOutputs(
      builder, loc, oldGeneric.getDpsInits(), useEmptyTensorInit);
  if (tensorInputs.size() !=
          static_cast<size_t>(oldGeneric.getNumDpsInputs()) ||
      tensorOutputs.size() !=
          static_cast<size_t>(oldGeneric.getNumDpsInits()))
    return nullptr;

  SmallVector<Type> resultTensorTypes;
  resultTensorTypes.reserve(tensorOutputs.size());
  for (Value output : tensorOutputs)
    resultTensorTypes.push_back(output.getType());

  Block &oldBody = oldGeneric.getRegion().front();
  auto newGeneric = linalg::GenericOp::create(
      builder, loc, resultTensorTypes, tensorInputs, tensorOutputs,
      oldGeneric.getIndexingMapsArray(), oldGeneric.getIteratorTypesArray(),
      [&](OpBuilder &nestedBuilder, Location nestedLoc, ValueRange blockArgs) {
        IRMapping mapper;
        for (auto [oldArg, newArg] : llvm::zip(oldBody.getArguments(), blockArgs))
          mapper.map(oldArg, newArg);

        for (Operation &op : oldBody.without_terminator())
          nestedBuilder.clone(op, mapper);

        SmallVector<Value> yielded;
        auto oldYield = cast<linalg::YieldOp>(oldBody.getTerminator());
        yielded.reserve(oldYield.getNumOperands());
        for (Value value : oldYield.getValues())
          yielded.push_back(mapper.lookupOrDefault(value));
        linalg::YieldOp::create(nestedBuilder, nestedLoc, yielded);
      });

  newGeneric->setAttrs(oldGeneric->getAttrs());
  return newGeneric;
}

struct RaiseToTensorPass
    : public arts::impl::RaiseToTensorBase<RaiseToTensorPass> {
  void runOnOperation() override {
    SmallVector<linalg::GenericOp> candidates;
    getOperation().walk([&](linalg::GenericOp generic) {
      if (generic->getParentOfType<sde::SdeSuIterateOp>())
        candidates.push_back(generic);
    });

    for (linalg::GenericOp generic : candidates) {
      if (!generic)
        continue;
      if (!generic->getResults().empty())
        continue;
      if (!llvm::all_of(generic->getOperands(), [](Value operand) {
            return isa<MemRefType>(operand.getType());
          }))
        continue;

      auto iterOp = generic->getParentOfType<sde::SdeSuIterateOp>();
      bool useEmptyTensorInit =
          iterOp && iterOp.getLinalgClassification() ==
                        sde::SdeLinalgClassification::elementwise;

      linalg::GenericOp tensorGeneric =
          rewriteGenericToTensor(generic, useEmptyTensorInit);
      if (!tensorGeneric)
        continue;

      generic.erase();
    }
  }
};

} // namespace

namespace mlir::arts::sde {

std::unique_ptr<Pass> createRaiseToTensorPass() {
  return std::make_unique<RaiseToTensorPass>();
}

} // namespace mlir::arts::sde
