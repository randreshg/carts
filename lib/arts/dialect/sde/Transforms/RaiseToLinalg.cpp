///==========================================================================///
/// File: RaiseToLinalg.cpp
///
/// Raises supported perfectly nested, affine `sde.su_iterate` bodies to
/// transient `linalg.generic` carriers when the body is structurally
/// compatible. The pass also stamps an SDE-owned loop classification on the
/// source op so `ConvertSdeToArts` can recover contracts for loops that still
/// stay on the classification-only fallback path.
///
/// Recognized patterns:
///   - elementwise : all-parallel iterators, identity/permutation maps
///   - matmul      : triple-nested, (m,k)*(k,n)->(m,n), reduction on k
///   - stencil     : all-parallel with constant-offset read maps
///   - reduction   : at least one reduction iterator
///
///==========================================================================///

#include "arts/dialect/sde/Analysis/StructuredOpAnalysis.h"
#include "arts/dialect/sde/Transforms/Passes.h"
namespace mlir::arts {
#define GEN_PASS_DEF_RAISETOLINALG
#include "arts/dialect/sde/Transforms/Passes.h.inc"
} // namespace mlir::arts

#include "mlir/Dialect/Linalg/IR/Linalg.h"
#include "mlir/IR/IRMapping.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallVector.h"

#include "arts/utils/Debug.h"
ARTS_DEBUG_SETUP(raise_to_linalg);

using namespace mlir;
using namespace mlir::arts;

namespace {
//===----------------------------------------------------------------------===//
// Linalg raising
//===----------------------------------------------------------------------===//

struct InputOperand {
  Value memref;
  AffineMap indexingMap;
  SmallVector<memref::LoadOp> loads;
};

struct OutputOperand {
  Value memref;
  AffineMap indexingMap;
  Type elementType;
};

static std::optional<unsigned> findOperandIndex(ArrayRef<InputOperand> inputs,
                                                Value memref,
                                                AffineMap indexingMap) {
  for (auto [idx, input] : llvm::enumerate(inputs)) {
    if (input.memref == memref && input.indexingMap == indexingMap)
      return idx;
  }
  return std::nullopt;
}

static std::optional<unsigned> findOperandIndex(ArrayRef<OutputOperand> outputs,
                                                Value memref,
                                                AffineMap indexingMap) {
  for (auto [idx, output] : llvm::enumerate(outputs)) {
    if (output.memref == memref && output.indexingMap == indexingMap)
      return idx;
  }
  return std::nullopt;
}

static Value cloneScalarValueIntoRegion(Value value, Block &body,
                                        OpBuilder &builder, IRMapping &mapper) {
  if (Value mapped = mapper.lookupOrNull(value))
    return mapped;

  if (auto blockArg = dyn_cast<BlockArgument>(value)) {
    if (blockArg.getOwner() == &body)
      return Value();
    return value;
  }

  Operation *defOp = value.getDefiningOp();
  if (!defOp || defOp->getBlock() != &body)
    return value;

  if (isa<memref::LoadOp>(defOp))
    return mapper.lookupOrNull(value);

  for (Value operand : defOp->getOperands()) {
    Value mappedOperand =
        cloneScalarValueIntoRegion(operand, body, builder, mapper);
    if (!mappedOperand && isa<BlockArgument>(operand))
      return Value();
  }

  builder.clone(*defOp, mapper);
  return mapper.lookupOrNull(value);
}

static bool raiseToLinalg(sde::SdeSuIterateOp iterOp,
                          const sde::LoopNestInfo &nest,
                          ArrayRef<sde::MemrefAccessEntry> reads,
                          ArrayRef<sde::MemrefAccessEntry> writes,
                          ArrayRef<utils::IteratorType> iterTypes) {
  Block &rootBody = iterOp.getBody().front();
  auto oldYield = dyn_cast<sde::SdeYieldOp>(rootBody.getTerminator());
  if (!oldYield)
    return false;

  Location loc = iterOp.getLoc();

  SmallVector<InputOperand> inputs;
  for (const sde::MemrefAccessEntry &read : reads) {
    auto loadOp = dyn_cast<memref::LoadOp>(read.op);
    if (!loadOp)
      return false;

    auto inputIdx = findOperandIndex(inputs, read.memref, read.indexingMap);
    if (!inputIdx) {
      inputs.push_back({read.memref, read.indexingMap, {}});
      inputIdx = inputs.size() - 1;
    }
    inputs[*inputIdx].loads.push_back(loadOp);
  }

  SmallVector<OutputOperand> outputs;
  DenseMap<Operation *, unsigned> storeOperandIndex;
  for (const sde::MemrefAccessEntry &write : writes) {
    auto storeOp = dyn_cast<memref::StoreOp>(write.op);
    if (!storeOp)
      return false;

    if (findOperandIndex(outputs, write.memref, write.indexingMap))
      return false;

    auto memrefTy = dyn_cast<MemRefType>(write.memref.getType());
    if (!memrefTy)
      return false;

    outputs.push_back(
        {write.memref, write.indexingMap, memrefTy.getElementType()});
    storeOperandIndex[write.op] = outputs.size() - 1;
  }

  if (outputs.empty())
    return false;

  OpBuilder builder(oldYield);

  SmallVector<Value> inputMemrefs;
  SmallVector<Value> outputMemrefs;
  SmallVector<AffineMap> indexingMaps;
  inputMemrefs.reserve(inputs.size());
  outputMemrefs.reserve(outputs.size());
  indexingMaps.reserve(inputs.size() + outputs.size());

  for (const InputOperand &input : inputs) {
    inputMemrefs.push_back(input.memref);
    indexingMaps.push_back(input.indexingMap);
  }

  for (const auto &output : outputs) {
    outputMemrefs.push_back(output.memref);
    indexingMaps.push_back(output.indexingMap);
  }

  auto generic = linalg::GenericOp::create(
      builder, loc, inputMemrefs, outputMemrefs, indexingMaps, iterTypes,
      [&](OpBuilder &nestedBuilder, Location nestedLoc, ValueRange blockArgs) {
        IRMapping mapper;
        for (auto [inputIdx, input] : llvm::enumerate(inputs)) {
          for (memref::LoadOp loadOp : input.loads)
            mapper.map(loadOp.getResult(), blockArgs[inputIdx]);
        }

        SmallVector<Value> yielded(outputs.size());
        SmallVector<bool> hasYield(outputs.size(), false);
        unsigned outputArgOffset = inputs.size();

        for (const sde::MemrefAccessEntry &write : writes) {
          auto storeOp = cast<memref::StoreOp>(write.op);
          auto storeIt = storeOperandIndex.find(storeOp.getOperation());
          if (storeIt == storeOperandIndex.end())
            continue;

          Value mappedValue = cloneScalarValueIntoRegion(
              storeOp.getValue(), *nest.innermostBody, nestedBuilder, mapper);
          if (!mappedValue)
            continue;
          yielded[storeIt->second] = mappedValue;
          hasYield[storeIt->second] = true;
        }

        for (auto [outputIdx, output] : llvm::enumerate(outputs)) {
          if (hasYield[outputIdx])
            continue;
          yielded[outputIdx] = blockArgs[outputArgOffset + outputIdx];
        }

        linalg::YieldOp::create(nestedBuilder, nestedLoc, yielded);
      });

  ARTS_DEBUG("  raised sde.su_iterate body to linalg.generic with "
             << inputs.size() << " inputs and " << outputs.size()
             << " outputs");
  (void)generic;
  return true;
}

//===----------------------------------------------------------------------===//
// Pass implementation
//===----------------------------------------------------------------------===//

struct RaiseToLinalgPass
    : public arts::impl::RaiseToLinalgBase<RaiseToLinalgPass> {

  void runOnOperation() override {
    ModuleOp module = getOperation();
    MLIRContext *ctx = &getContext();
    unsigned analyzed = 0;
    unsigned classified = 0;
    unsigned raised = 0;

    module.walk([&](sde::SdeSuIterateOp iterOp) {
      ++analyzed;

      std::optional<sde::StructuredLoopSummary> summary =
          sde::analyzeStructuredLoop(iterOp);
      if (!summary)
        return;

      sde::SdeStructuredClassification pattern = summary->classification;

      iterOp.setStructuredClassificationAttr(
          sde::SdeStructuredClassificationAttr::get(ctx, pattern));
      ++classified;
      if (summary->supportsLinalgCarrier() &&
          raiseToLinalg(iterOp, summary->nest, summary->reads, summary->writes,
                        summary->iterTypes))
        ++raised;

      ARTS_DEBUG("classified sde.su_iterate as '"
                 << stringifySdeStructuredClassification(pattern) << "' with "
                 << summary->nest.ivs.size() << " dims ("
                 << summary->reads.size() << " ins, "
                 << summary->outputMaps.size() << " outs)");
    });

    ARTS_INFO("RaiseToLinalg: analyzed " << analyzed << " loops, classified "
                                         << classified << ", raised "
                                         << raised);
  }
};

} // namespace

namespace mlir::arts::sde {

std::unique_ptr<Pass> createRaiseToLinalgPass() {
  return std::make_unique<RaiseToLinalgPass>();
}

} // namespace mlir::arts::sde
