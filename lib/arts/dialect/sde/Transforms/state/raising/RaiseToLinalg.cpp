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

#include "mlir/Dialect/Bufferization/IR/Bufferization.h"
#include "mlir/Dialect/Linalg/IR/Linalg.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/Tensor/IR/Tensor.h"
#include "mlir/IR/IRMapping.h"
#include "mlir/Interfaces/SideEffectInterfaces.h"
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
  Value source; // memref or tensor value
  AffineMap indexingMap;
  SmallVector<Operation *> accessOps; // memref::LoadOp or tensor::ExtractOp
};

struct OutputOperand {
  Value source; // memref or tensor value
  AffineMap indexingMap;
  Type elementType;
};

/// Trace a tensor value through tensor.insert chains to find the original
/// tensor block arg or defining value.
static Value traceToTensorSource(Value v) {
  while (auto insertOp = v.getDefiningOp<tensor::InsertOp>())
    v = insertOp.getDest();
  return v;
}

static std::optional<unsigned> findOperandIndex(ArrayRef<InputOperand> inputs,
                                                Value source,
                                                AffineMap indexingMap) {
  for (auto [idx, input] : llvm::enumerate(inputs)) {
    if (input.source == source && input.indexingMap == indexingMap)
      return idx;
  }
  return std::nullopt;
}

static std::optional<unsigned> findOperandIndex(ArrayRef<OutputOperand> outputs,
                                                Value source,
                                                AffineMap indexingMap) {
  for (auto [idx, output] : llvm::enumerate(outputs)) {
    if (output.source == source && output.indexingMap == indexingMap)
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

  if (isa<memref::LoadOp, tensor::ExtractOp>(defOp))
    return mapper.lookupOrNull(value);

  // tensor.insert is not a scalar op — skip it.
  if (isa<tensor::InsertOp>(defOp))
    return Value();

  for (Value operand : defOp->getOperands()) {
    Value mappedOperand =
        cloneScalarValueIntoRegion(operand, body, builder, mapper);
    if (!mappedOperand)
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
  Block *computeBody = sde::getSuIterateComputeBlock(iterOp);
  // Find the yield terminator in the compute block (inside cu_region if
  // present, otherwise the su_iterate body directly).
  Operation *terminator = computeBody->getTerminator();
  if (!terminator || !isa<sde::SdeYieldOp>(terminator))
    return false;
  auto oldYield = cast<sde::SdeYieldOp>(terminator);

  Location loc = iterOp.getLoc();

  SmallVector<InputOperand> inputs;
  for (const sde::MemrefAccessEntry &read : reads) {
    // Accept both tensor.extract and memref.load.
    Value source;
    if (auto extractOp = dyn_cast<tensor::ExtractOp>(read.op))
      source = traceToTensorSource(extractOp.getTensor());
    else if (auto loadOp = dyn_cast<memref::LoadOp>(read.op))
      source = read.memref;
    else
      return false;

    auto inputIdx = findOperandIndex(inputs, source, read.indexingMap);
    if (!inputIdx) {
      inputs.push_back({source, read.indexingMap, {}});
      inputIdx = inputs.size() - 1;
    }
    inputs[*inputIdx].accessOps.push_back(read.op);
  }

  SmallVector<OutputOperand> outputs;
  DenseMap<Operation *, unsigned> writeOperandIndex;
  for (const sde::MemrefAccessEntry &write : writes) {
    // Accept both tensor.insert and memref.store.
    Value source;
    Type elementType;
    if (auto insertOp = dyn_cast<tensor::InsertOp>(write.op)) {
      source = traceToTensorSource(insertOp.getDest());
      auto tensorTy = dyn_cast<RankedTensorType>(source.getType());
      if (!tensorTy)
        return false;
      elementType = tensorTy.getElementType();
    } else if (auto storeOp = dyn_cast<memref::StoreOp>(write.op)) {
      source = write.memref;
      auto memrefTy = dyn_cast<MemRefType>(source.getType());
      if (!memrefTy)
        return false;
      elementType = memrefTy.getElementType();
    } else {
      return false;
    }

    if (findOperandIndex(outputs, source, write.indexingMap))
      return false;

    outputs.push_back({source, write.indexingMap, elementType});
    writeOperandIndex[write.op] = outputs.size() - 1;
  }

  if (outputs.empty())
    return false;

  OpBuilder builder(oldYield);

  SmallVector<AffineMap> indexingMaps;
  indexingMaps.reserve(inputs.size() + outputs.size());

  // Build tensor inputs — use tensor sources directly, wrap memrefs via
  // sde.mu_memref_to_tensor (SDE-native boundary op for external data).
  SmallVector<Value> tensorInputs;
  tensorInputs.reserve(inputs.size());
  for (const InputOperand &input : inputs) {
    Value tensorInput;
    if (auto tensorTy = dyn_cast<RankedTensorType>(input.source.getType())) {
      tensorInput = input.source;
    } else {
      auto mrType = cast<MemRefType>(input.source.getType());
      auto tTy =
          RankedTensorType::get(mrType.getShape(), mrType.getElementType());
      tensorInput =
          sde::SdeMuMemrefToTensorOp::create(builder, loc, tTy, input.source);
    }
    tensorInputs.push_back(tensorInput);
    indexingMaps.push_back(input.indexingMap);
  }

  // Build tensor outputs — use tensor sources directly, wrap memrefs via
  // sde.mu_memref_to_tensor (SDE-native boundary op).
  SmallVector<Value> tensorOutputs;
  SmallVector<Type> resultTypes;
  tensorOutputs.reserve(outputs.size());
  for (const auto &output : outputs) {
    Value tensorOutput;
    RankedTensorType tensorTy;
    if (auto tTy = dyn_cast<RankedTensorType>(output.source.getType())) {
      tensorTy = tTy;
      tensorOutput = output.source;
    } else {
      auto mrType = cast<MemRefType>(output.source.getType());
      tensorTy =
          RankedTensorType::get(mrType.getShape(), mrType.getElementType());
      tensorOutput =
          sde::SdeMuMemrefToTensorOp::create(builder, loc, tensorTy,
                                              output.source);
    }
    tensorOutputs.push_back(tensorOutput);
    resultTypes.push_back(tensorTy);
    indexingMaps.push_back(output.indexingMap);
  }

  bool cloneFailed = false;
  auto generic = linalg::GenericOp::create(
      builder, loc, resultTypes, tensorInputs, tensorOutputs, indexingMaps,
      iterTypes,
      [&](OpBuilder &nestedBuilder, Location nestedLoc, ValueRange blockArgs) {
        IRMapping mapper;
        for (auto [inputIdx, input] : llvm::enumerate(inputs)) {
          for (Operation *accessOp : input.accessOps) {
            if (auto loadOp = dyn_cast<memref::LoadOp>(accessOp))
              mapper.map(loadOp.getResult(), blockArgs[inputIdx]);
            else if (auto extractOp = dyn_cast<tensor::ExtractOp>(accessOp))
              mapper.map(extractOp.getResult(), blockArgs[inputIdx]);
          }
        }

        SmallVector<Value> yielded(outputs.size());
        SmallVector<bool> hasYield(outputs.size(), false);
        unsigned outputArgOffset = inputs.size();

        for (const sde::MemrefAccessEntry &write : writes) {
          Value valueToWrite;
          if (auto storeOp = dyn_cast<memref::StoreOp>(write.op))
            valueToWrite = storeOp.getValue();
          else if (auto insertOp = dyn_cast<tensor::InsertOp>(write.op))
            valueToWrite = insertOp.getScalar();
          else
            continue;

          auto writeIt = writeOperandIndex.find(write.op);
          if (writeIt == writeOperandIndex.end())
            continue;

          Value mappedValue = cloneScalarValueIntoRegion(
              valueToWrite, *nest.innermostBody, nestedBuilder, mapper);
          if (!mappedValue) {
            cloneFailed = true;
            continue;
          }
          yielded[writeIt->second] = mappedValue;
          hasYield[writeIt->second] = true;
        }

        for (auto [outputIdx, output] : llvm::enumerate(outputs)) {
          if (hasYield[outputIdx])
            continue;
          yielded[outputIdx] = blockArgs[outputArgOffset + outputIdx];
        }

        linalg::YieldOp::create(nestedBuilder, nestedLoc, yielded);
      });

  if (cloneFailed) {
    generic.erase();
    return false;
  }

  // Erase scalar body ops that the carrier now represents. The carrier is
  // authoritative — the scalar ops (memref.load, memref.store, tensor.extract,
  // tensor.insert, and now-dead arith ops) are redundant.
  //
  // Iteratively erase: first drop uses from stores/inserts (side-effecting
  // ops that produce no results), then erase dead ops in reverse order.
  unsigned erased = 0;
  bool changed = true;
  while (changed) {
    changed = false;
    SmallVector<Operation *> toErase;
    for (Operation &op : llvm::reverse(computeBody->without_terminator())) {
      // Keep the carrier and its support ops.
      if (isa<linalg::GenericOp>(op) ||
          isa<sde::SdeMuMemrefToTensorOp>(op) ||
          isa<bufferization::ToTensorOp>(op) ||
          isa<tensor::EmptyOp>(op))
        continue;
      // Erase stores/inserts (side effects but no SSA results to worry about).
      if (isa<memref::StoreOp, tensor::InsertOp>(&op)) {
        toErase.push_back(&op);
        continue;
      }
      // Erase loads/extracts and pure ops that are now dead.
      if (op.use_empty()) {
        toErase.push_back(&op);
        continue;
      }
    }
    for (Operation *op : toErase) {
      op->erase();
      ++erased;
      changed = true;
    }
  }

  ARTS_DEBUG("  raised sde.su_iterate body to linalg.generic with "
             << inputs.size() << " inputs and " << outputs.size()
             << " outputs (carrier-authoritative, " << erased
             << " scalar ops erased)");
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
