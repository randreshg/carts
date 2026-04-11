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

#include "arts/dialect/sde/Transforms/Passes.h"
namespace mlir::arts {
#define GEN_PASS_DEF_RAISETOLINALG
#include "arts/dialect/sde/Transforms/Passes.h.inc"
} // namespace mlir::arts

#include "mlir/Dialect/Linalg/IR/Linalg.h"
#include "mlir/IR/AffineExpr.h"
#include "mlir/IR/AffineMap.h"
#include "mlir/IR/IRMapping.h"
#include "mlir/Interfaces/SideEffectInterfaces.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallBitVector.h"
#include "llvm/ADT/SmallVector.h"

#include "arts/utils/Debug.h"
ARTS_DEBUG_SETUP(raise_to_linalg);

using namespace mlir;
using namespace mlir::arts;

namespace {

//===----------------------------------------------------------------------===//
// Loop nest collection
//===----------------------------------------------------------------------===//

struct LoopNestInfo {
  SmallVector<Value> ivs;
  Block *innermostBody = nullptr;
  sde::SdeSuIterateOp rootIterOp;
};

static SmallVector<Operation *> getBodyOps(Block &body) {
  SmallVector<Operation *> ops;
  for (auto &op : body) {
    if (op.hasTrait<OpTrait::IsTerminator>())
      continue;
    if (isa<sde::SdeYieldOp>(op))
      continue;
    ops.push_back(&op);
  }
  return ops;
}

/// Recursively collect a perfectly nested scf.for chain from a body block.
static bool collectInner(Block &body, LoopNestInfo &info) {
  SmallVector<Operation *> ops = getBodyOps(body);

  if (ops.size() == 1) {
    if (auto innerFor = dyn_cast<scf::ForOp>(ops.front())) {
      info.ivs.push_back(innerFor.getInductionVar());
      return collectInner(*innerFor.getBody(), info);
    }
  }

  info.innermostBody = &body;
  return true;
}

/// Collect the full perfectly-nested loop structure from an sde.su_iterate.
static bool collectPerfectNest(sde::SdeSuIterateOp iterOp, LoopNestInfo &info) {
  info.rootIterOp = iterOp;

  Region &region = iterOp.getBody();
  if (region.empty() || region.front().getNumArguments() == 0)
    return false;

  llvm::append_range(info.ivs, region.front().getArguments());
  return collectInner(region.front(), info);
}

//===----------------------------------------------------------------------===//
// Affine expression analysis
//===----------------------------------------------------------------------===//

static std::optional<AffineExpr>
tryGetAffineExpr(Value value, ArrayRef<Value> ivs, MLIRContext *ctx) {
  for (auto [idx, iv] : llvm::enumerate(ivs)) {
    if (value == iv)
      return getAffineDimExpr(idx, ctx);
  }

  auto *defOp = value.getDefiningOp();
  if (!defOp)
    return std::nullopt;

  if (auto cst = dyn_cast<arith::ConstantOp>(defOp)) {
    if (auto intAttr = dyn_cast<IntegerAttr>(cst.getValue()))
      return getAffineConstantExpr(intAttr.getInt(), ctx);
    return std::nullopt;
  }

  if (auto addOp = dyn_cast<arith::AddIOp>(defOp)) {
    auto lhs = tryGetAffineExpr(addOp.getLhs(), ivs, ctx);
    auto rhs = tryGetAffineExpr(addOp.getRhs(), ivs, ctx);
    if (lhs && rhs)
      return *lhs + *rhs;
  }

  if (auto subOp = dyn_cast<arith::SubIOp>(defOp)) {
    auto lhs = tryGetAffineExpr(subOp.getLhs(), ivs, ctx);
    auto rhs = tryGetAffineExpr(subOp.getRhs(), ivs, ctx);
    if (lhs && rhs)
      return *lhs - *rhs;
  }

  if (auto mulOp = dyn_cast<arith::MulIOp>(defOp)) {
    auto lhs = tryGetAffineExpr(mulOp.getLhs(), ivs, ctx);
    auto rhs = tryGetAffineExpr(mulOp.getRhs(), ivs, ctx);
    if (lhs && rhs) {
      if (isa<AffineConstantExpr>(*lhs) || isa<AffineConstantExpr>(*rhs))
        return *lhs * *rhs;
    }
  }

  if (auto castOp = dyn_cast<arith::IndexCastOp>(defOp))
    return tryGetAffineExpr(castOp.getIn(), ivs, ctx);

  return std::nullopt;
}

static std::optional<AffineMap> tryBuildIndexingMap(OperandRange indices,
                                                    ArrayRef<Value> ivs,
                                                    MLIRContext *ctx) {
  SmallVector<AffineExpr> exprs;
  exprs.reserve(indices.size());
  for (Value idx : indices) {
    auto expr = tryGetAffineExpr(idx, ivs, ctx);
    if (!expr)
      return std::nullopt;
    exprs.push_back(*expr);
  }
  return AffineMap::get(/*dimCount=*/ivs.size(), /*symbolCount=*/0, exprs, ctx);
}

//===----------------------------------------------------------------------===//
// Memref access collection
//===----------------------------------------------------------------------===//

struct MemrefAccessEntry {
  Value memref;
  AffineMap indexingMap;
  Operation *op;
  bool isRead;
};

static bool collectMemrefAccesses(Block &body, ArrayRef<Value> ivs,
                                  SmallVectorImpl<MemrefAccessEntry> &reads,
                                  SmallVectorImpl<MemrefAccessEntry> &writes,
                                  MLIRContext *ctx) {
  for (auto &op : body) {
    if (op.hasTrait<OpTrait::IsTerminator>())
      continue;

    if (auto loadOp = dyn_cast<memref::LoadOp>(&op)) {
      auto map = tryBuildIndexingMap(loadOp.getIndices(), ivs, ctx);
      if (!map)
        return false;
      reads.push_back({loadOp.getMemref(), *map, loadOp.getOperation(),
                       /*isRead=*/true});
      continue;
    }

    if (auto storeOp = dyn_cast<memref::StoreOp>(&op)) {
      auto map = tryBuildIndexingMap(storeOp.getIndices(), ivs, ctx);
      if (!map)
        return false;
      writes.push_back({storeOp.getMemref(), *map, storeOp.getOperation(),
                        /*isRead=*/false});
      continue;
    }

    if (isMemoryEffectFree(&op))
      continue;

    ARTS_DEBUG("  bail: unknown op in body: " << op.getName());
    return false;
  }

  return !reads.empty() || !writes.empty();
}

//===----------------------------------------------------------------------===//
// Iterator type determination
//===----------------------------------------------------------------------===//

static void
computeIteratorTypes(unsigned numDims, ArrayRef<AffineMap> outputMaps,
                     SmallVectorImpl<utils::IteratorType> &iterTypes) {
  llvm::SmallBitVector dimsInOutputs(numDims, false);
  for (AffineMap map : outputMaps) {
    for (unsigned i = 0; i < map.getNumResults(); ++i) {
      map.getResult(i).walk([&](AffineExpr expr) {
        if (auto dimExpr = dyn_cast<AffineDimExpr>(expr))
          dimsInOutputs.set(dimExpr.getPosition());
      });
    }
  }

  for (unsigned d = 0; d < numDims; ++d) {
    iterTypes.push_back(dimsInOutputs.test(d) ? utils::IteratorType::parallel
                                              : utils::IteratorType::reduction);
  }
}

//===----------------------------------------------------------------------===//
// Pattern classification
//===----------------------------------------------------------------------===//

static bool hasConstantOffsets(AffineMap map) {
  for (auto result : map.getResults()) {
    auto binOp = dyn_cast<AffineBinaryOpExpr>(result);
    if (!binOp || binOp.getKind() != AffineExprKind::Add)
      continue;
    bool lhsDim = isa<AffineDimExpr>(binOp.getLHS());
    bool rhsDim = isa<AffineDimExpr>(binOp.getRHS());
    bool lhsCst = isa<AffineConstantExpr>(binOp.getLHS());
    bool rhsCst = isa<AffineConstantExpr>(binOp.getRHS());
    if ((lhsDim && rhsCst) || (lhsCst && rhsDim))
      return true;
  }
  return false;
}

static sde::SdeLinalgClassification
classifyPattern(ArrayRef<MemrefAccessEntry> reads,
                ArrayRef<AffineMap> outputMaps,
                ArrayRef<utils::IteratorType> iterTypes, unsigned numDims) {
  unsigned numParallel = 0;
  unsigned numReduction = 0;
  for (utils::IteratorType t : iterTypes) {
    if (t == utils::IteratorType::parallel)
      ++numParallel;
    else
      ++numReduction;
  }

  if (numReduction == 0) {
    for (auto &entry : reads) {
      if (hasConstantOffsets(entry.indexingMap))
        return sde::SdeLinalgClassification::stencil;
    }
    return sde::SdeLinalgClassification::elementwise;
  }

  if (numParallel == 2 && numReduction == 1 && numDims == 3 &&
      reads.size() >= 2 && !outputMaps.empty())
    return sde::SdeLinalgClassification::matmul;

  return sde::SdeLinalgClassification::reduction;
}

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

static bool isConstantZeroIndexingMap(AffineMap indexingMap) {
  if (indexingMap.getNumResults() != 1)
    return false;

  auto constant = dyn_cast<AffineConstantExpr>(indexingMap.getResult(0));
  return constant && constant.getValue() == 0;
}

static bool supportsReductionCarrierSubset(
    sde::SdeSuIterateOp iterOp, const LoopNestInfo &nest,
    ArrayRef<MemrefAccessEntry> reads, ArrayRef<MemrefAccessEntry> writes) {
  if (iterOp.getReductionAccumulators().size() != 1 || nest.ivs.size() != 1)
    return false;
  if (writes.size() != 1)
    return false;

  Value reductionAccumulator = iterOp.getReductionAccumulators().front();
  auto reductionType = dyn_cast<MemRefType>(reductionAccumulator.getType());
  if (!reductionType || reductionType.getRank() != 1)
    return false;

  const MemrefAccessEntry &write = writes.front();
  if (write.memref != reductionAccumulator ||
      !isConstantZeroIndexingMap(write.indexingMap))
    return false;

  unsigned matchingReductionReads = 0;
  for (const MemrefAccessEntry &read : reads) {
    if (read.memref != reductionAccumulator)
      continue;
    if (!isConstantZeroIndexingMap(read.indexingMap))
      return false;
    ++matchingReductionReads;
  }

  return matchingReductionReads == 1;
}

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

static bool raiseToLinalg(sde::SdeSuIterateOp iterOp, const LoopNestInfo &nest,
                          ArrayRef<MemrefAccessEntry> reads,
                          ArrayRef<MemrefAccessEntry> writes,
                          ArrayRef<utils::IteratorType> iterTypes) {
  Block &rootBody = iterOp.getBody().front();
  auto oldYield = dyn_cast<sde::SdeYieldOp>(rootBody.getTerminator());
  if (!oldYield)
    return false;

  Location loc = iterOp.getLoc();

  SmallVector<InputOperand> inputs;
  for (const MemrefAccessEntry &read : reads) {
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
  for (const MemrefAccessEntry &write : writes) {
    auto storeOp = dyn_cast<memref::StoreOp>(write.op);
    if (!storeOp)
      return false;

    if (findOperandIndex(outputs, write.memref, write.indexingMap))
      return false;

    auto memrefTy = dyn_cast<MemRefType>(write.memref.getType());
    if (!memrefTy)
      return false;

    outputs.push_back({write.memref, write.indexingMap,
                       memrefTy.getElementType()});
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

        for (const MemrefAccessEntry &write : writes) {
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

      LoopNestInfo nest;
      if (!collectPerfectNest(iterOp, nest))
        return;
      if (!nest.innermostBody || nest.ivs.empty())
        return;

      SmallVector<MemrefAccessEntry> reads;
      SmallVector<MemrefAccessEntry> writes;
      if (!collectMemrefAccesses(*nest.innermostBody, nest.ivs, reads, writes,
                                 ctx))
        return;

      SmallVector<AffineMap> outputMaps;
      outputMaps.reserve(writes.size() +
                         iterOp.getReductionAccumulators().size());
      for (const MemrefAccessEntry &write : writes)
        outputMaps.push_back(write.indexingMap);
      for (size_t i = 0; i < iterOp.getReductionAccumulators().size(); ++i)
        outputMaps.push_back(AffineMap::get(nest.ivs.size(), 0, {}, ctx));

      if (outputMaps.empty())
        return;

      SmallVector<utils::IteratorType> iterTypes;
      computeIteratorTypes(nest.ivs.size(), outputMaps, iterTypes);

      sde::SdeLinalgClassification pattern =
          classifyPattern(reads, outputMaps, iterTypes, nest.ivs.size());

      iterOp.setLinalgClassificationAttr(
          sde::SdeLinalgClassificationAttr::get(ctx, pattern));
      ++classified;
      bool supportsReductionCarrier =
          pattern == sde::SdeLinalgClassification::reduction &&
          supportsReductionCarrierSubset(iterOp, nest, reads, writes);
      bool supportsLinalgCarrier =
          pattern != sde::SdeLinalgClassification::stencil &&
          (pattern != sde::SdeLinalgClassification::reduction ||
           supportsReductionCarrier);
      if (supportsLinalgCarrier &&
          raiseToLinalg(iterOp, nest, reads, writes, iterTypes))
        ++raised;

      ARTS_DEBUG("classified sde.su_iterate as '"
                 << stringifySdeLinalgClassification(pattern) << "' with "
                 << nest.ivs.size() << " dims (" << reads.size() << " ins, "
                 << outputMaps.size() << " outs)");
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
