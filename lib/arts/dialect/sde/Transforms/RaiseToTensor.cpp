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

#include "llvm/ADT/DenseMap.h"

#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/IRMapping.h"

using namespace mlir;
using namespace mlir::arts;

namespace {

static RankedTensorType getRankedTensorType(Value memref) {
  return dyn_cast<RankedTensorType>(
      memref::getTensorTypeFromMemRefType(memref.getType()));
}

static Value getOrCreateTensorValue(OpBuilder &builder, Location loc,
                                    DenseMap<Value, Value> &tensorValues,
                                    Value memref) {
  if (Value tensor = tensorValues.lookup(memref))
    return tensor;

  RankedTensorType tensorType = getRankedTensorType(memref);
  if (!tensorType)
    return Value();

  Value tensor =
      bufferization::ToTensorOp::create(builder, loc, tensorType, memref);
  tensorValues[memref] = tensor;
  return tensor;
}

static SmallVector<Value>
buildTensorInputs(OpBuilder &builder, Location loc, ValueRange memrefInputs,
                  DenseMap<Value, Value> &tensorValues) {
  SmallVector<Value> tensorInputs;
  tensorInputs.reserve(memrefInputs.size());
  for (Value input : memrefInputs) {
    Value tensor = getOrCreateTensorValue(builder, loc, tensorValues, input);
    if (!tensor)
      return {};
    tensorInputs.push_back(tensor);
  }
  return tensorInputs;
}

static Value stripSimpleMemrefAlias(Value value) {
  while (auto castOp = value.getDefiningOp<memref::CastOp>())
    value = castOp.getSource();
  return value;
}

static BlockArgument getOutputBlockArgument(linalg::GenericOp generic,
                                            unsigned outputIndex) {
  return generic.getRegion().front().getArgument(generic.getNumDpsInputs() +
                                                 outputIndex);
}

static bool hasSimpleAliasWithOtherOperands(linalg::GenericOp generic,
                                            unsigned outputIndex) {
  Value output = stripSimpleMemrefAlias(generic.getDpsInits()[outputIndex]);

  for (Value input : generic.getDpsInputs()) {
    if (stripSimpleMemrefAlias(input) == output)
      return true;
  }

  for (auto [otherIndex, otherOutput] : llvm::enumerate(generic.getDpsInits())) {
    if (otherIndex == outputIndex)
      continue;
    if (stripSimpleMemrefAlias(otherOutput) == output)
      return true;
  }

  return false;
}

static bool shouldUseEmptyTensorInit(linalg::GenericOp generic,
                                     unsigned outputIndex) {
  if (!getOutputBlockArgument(generic, outputIndex).use_empty())
    return false;

  return !hasSimpleAliasWithOtherOperands(generic, outputIndex);
}

static SmallVector<Value>
buildTensorOutputs(OpBuilder &builder, Location loc,
                   linalg::GenericOp oldGeneric,
                   DenseMap<Value, Value> &tensorValues) {
  SmallVector<Value> tensorOutputs;
  tensorOutputs.reserve(oldGeneric.getNumDpsInits());
  for (auto [outputIndex, output] : llvm::enumerate(oldGeneric.getDpsInits())) {
    RankedTensorType tensorType = getRankedTensorType(output);
    if (!tensorType)
      return {};

    if (shouldUseEmptyTensorInit(oldGeneric, outputIndex)) {
      tensorOutputs.push_back(tensor::EmptyOp::create(
          builder, loc, memref::getMixedSizes(builder, loc, output),
          tensorType.getElementType()));
      continue;
    }

    Value tensor = getOrCreateTensorValue(builder, loc, tensorValues, output);
    if (!tensor)
      return {};
    tensorOutputs.push_back(tensor);
  }
  return tensorOutputs;
}

static linalg::GenericOp rewriteGenericToTensor(linalg::GenericOp oldGeneric) {
  OpBuilder builder(oldGeneric);
  Location loc = oldGeneric.getLoc();
  DenseMap<Value, Value> tensorValues;

  SmallVector<Value> tensorInputs =
      buildTensorInputs(builder, loc, oldGeneric.getDpsInputs(), tensorValues);
  SmallVector<Value> tensorOutputs =
      buildTensorOutputs(builder, loc, oldGeneric, tensorValues);
  if (tensorInputs.size() !=
          static_cast<size_t>(oldGeneric.getNumDpsInputs()) ||
      tensorOutputs.size() != static_cast<size_t>(oldGeneric.getNumDpsInits()))
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
        for (auto [oldArg, newArg] :
             llvm::zip(oldBody.getArguments(), blockArgs))
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

      linalg::GenericOp tensorGeneric = rewriteGenericToTensor(generic);
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
