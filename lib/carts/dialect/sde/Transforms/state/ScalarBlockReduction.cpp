///==========================================================================///
/// File: ScalarBlockReduction.cpp
///
/// SDE-owned scalar block-reduction transformation.
///
/// Rewrites a single-CU scalar reduction loop into block-parallel partial
/// producers plus a single-CU final combine. Rank-expanded inputs keep owner
/// block coordinates explicit in the partial MU shape.
///==========================================================================///

#include "carts/dialect/sde/IR/SdeDialect.h"
#include "carts/dialect/sde/Transforms/Passes.h"
#include "carts/dialect/sde/Utils/SdeAttrNames.h"
#include "carts/dialect/sde/Utils/MuAccessWindow.h"
#include "carts/dialect/sde/Utils/SdeCommittedFactUtils.h"
#include "carts/utils/ArrayAttrUtils.h"
#include "carts/utils/ValueAnalysis.h"

namespace mlir::carts::sde {
#define GEN_PASS_DEF_SDESCALARBLOCKREDUCTION
#include "carts/dialect/sde/Transforms/Passes.h.inc"
} // namespace mlir::carts::sde

#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/Interfaces/SideEffectInterfaces.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallPtrSet.h"

#include <optional>

using namespace mlir;
using namespace mlir::carts;

namespace {

struct BlockGeometry1D {
  Value root;
  MemRefType type;
  int64_t extent = 0;
  int64_t blockExtent = 0;
  int64_t blockCount = 0;
  bool rankExpandedMu = false;
};

struct AccumulatorUpdate {
  Value memref;
  Type elementType;
  bool isFloat = false;
};

struct ReductionCandidate {
  scf::ForOp loop;
  SmallVector<AccumulatorUpdate, 4> accumulators;
  BlockGeometry1D sourceGeometry;
  int64_t step = 0;
  int64_t partialSlots = 1;
  bool preserveFloatOrder = false;
};

static bool isRank0Memref(Value value) {
  auto type = dyn_cast_or_null<MemRefType>(value.getType());
  return type && type.getRank() == 0 && type.hasStaticShape();
}

static bool isSupportedStaticSourceMemref(Value value) {
  auto type = dyn_cast_or_null<MemRefType>(value.getType());
  if (!type || !type.hasStaticShape())
    return false;
  if (type.getRank() == 1)
    return type.getDimSize(0) > 0;
  if (type.getRank() >= 2)
    return type.getDimSize(0) > 1 &&
           isa_and_nonnull<sde::SdeMuAllocOp>(value.getDefiningOp());
  return false;
}

static std::optional<int64_t> getConstantIndex(Value value) {
  return ValueAnalysis::tryFoldConstantIndex(value);
}

static int64_t ceilDiv(int64_t lhs, int64_t rhs) {
  return (lhs + rhs - 1) / rhs;
}

static bool isZeroBasedStaticLoop(scf::ForOp loop, int64_t &upper,
                                  int64_t &step) {
  std::optional<int64_t> lb = getConstantIndex(loop.getLowerBound());
  std::optional<int64_t> ub = getConstantIndex(loop.getUpperBound());
  std::optional<int64_t> st = getConstantIndex(loop.getStep());
  if (!lb || !ub || !st || *lb != 0 || *ub <= 0 || *st <= 0)
    return false;
  upper = *ub;
  step = *st;
  return true;
}

static std::optional<unsigned>
rank0AccumulatorIndex(Value memref, ArrayRef<AccumulatorUpdate> accumulators) {
  Value root = ValueAnalysis::stripMemrefViewOps(memref);
  for (auto [idx, acc] : llvm::enumerate(accumulators))
    if (ValueAnalysis::stripMemrefViewOps(acc.memref) == root)
      return static_cast<unsigned>(idx);
  return std::nullopt;
}

static bool isLoadFromAccumulator(Value value, Value accumulator) {
  auto load = value.getDefiningOp<memref::LoadOp>();
  if (!load || !load.getIndices().empty())
    return false;
  return ValueAnalysis::stripMemrefViewOps(load.getMemRef()) ==
         ValueAnalysis::stripMemrefViewOps(accumulator);
}

static std::optional<AccumulatorUpdate>
matchAccumulatorStore(memref::StoreOp store) {
  if (!store.getIndices().empty() || !isRank0Memref(store.getMemref()))
    return std::nullopt;

  Operation *def = store.getValueToStore().getDefiningOp();
  if (!def || !isa<arith::AddFOp, arith::AddIOp>(def))
    return std::nullopt;

  Value lhs = def->getOperand(0);
  Value rhs = def->getOperand(1);
  if (!isLoadFromAccumulator(lhs, store.getMemref()) &&
      !isLoadFromAccumulator(rhs, store.getMemref()))
    return std::nullopt;

  Type elementType =
      cast<MemRefType>(store.getMemref().getType()).getElementType();
  return AccumulatorUpdate{ValueAnalysis::stripMemrefViewOps(store.getMemref()),
                           elementType, isa<FloatType>(elementType)};
}

static std::optional<SmallVector<int64_t, 2>> readI64Vector(ArrayAttr attr) {
  std::optional<SmallVector<int64_t, 4>> values = readI64ArrayAttr(attr);
  if (!values)
    return std::nullopt;
  SmallVector<int64_t, 2> out(values->begin(), values->end());
  return out;
}

static std::optional<BlockGeometry1D>
findCommittedBlockGeometryForRoot(Value root) {
  root = ValueAnalysis::stripMemrefViewOps(root);
  if (!isSupportedStaticSourceMemref(root))
    return std::nullopt;

  auto type = cast<MemRefType>(root.getType());
  bool rankExpandedMu = type.getRank() >= 2;
  std::optional<BlockGeometry1D> result;
  for (Operation *user : root.getUsers()) {
    if (!isa<memref::StoreOp>(user))
      continue;
    sde::SdeSuIterateOp su = user->getParentOfType<sde::SdeSuIterateOp>();
    while (su && !sde::recoverCommittedPhysicalLayout(su))
      su = su->getParentOfType<sde::SdeSuIterateOp>();
    if (!su)
      continue;

    std::optional<sde::CommittedSuPhysicalLayout> layout =
        sde::recoverCommittedPhysicalLayout(su);
    if (!layout || layout->ownerDims.size() != 1 || layout->blockShape.empty())
      continue;
    int64_t ownerDim = layout->ownerDims[0];
    if (ownerDim < 0 || static_cast<size_t>(ownerDim) >= layout->blockShape.size())
      continue;

    int64_t extent = type.getDimSize(0);
    int64_t blockExtent = layout->blockShape[ownerDim];
    if (blockExtent <= 0)
      continue;
    int64_t blockCount = ceilDiv(type.getDimSize(0), blockExtent);
    if (rankExpandedMu) {
      unsigned logicalRank = type.getRank() - 1;
      if (layout->blockShape.size() != logicalRank ||
          static_cast<unsigned>(ownerDim) >= logicalRank)
        continue;
      bool shapeMatches = true;
      for (auto [dim, block] : llvm::enumerate(layout->blockShape))
        if (type.getDimSize(dim + 1) != block) {
          shapeMatches = false;
          break;
        }
      if (!shapeMatches)
        continue;
      blockExtent = type.getDimSize(ownerDim + 1);
      blockCount = type.getDimSize(0);
      extent = blockCount * blockExtent;
    } else if (ownerDim != 0) {
      continue;
    }
    if (blockCount <= 1)
      continue;
    if (!rankExpandedMu && blockExtent > type.getDimSize(0))
      continue;
    BlockGeometry1D candidate{root,        type,       extent,
                              blockExtent, blockCount, rankExpandedMu};
    if (result) {
      if (result->extent != candidate.extent ||
          result->blockExtent != candidate.blockExtent ||
          result->blockCount != candidate.blockCount)
        return std::nullopt;
    } else {
      result = candidate;
    }
  }
  return result;
}

static bool isSupportedPureOp(Operation *op) {
  if (isa<arith::ConstantOp, arith::AddFOp, arith::AddIOp, arith::SubIOp,
          arith::MulIOp, arith::MulFOp, arith::DivUIOp, arith::RemUIOp,
          arith::IndexCastOp, arith::ExtFOp, arith::TruncFOp, arith::CmpIOp,
          arith::SelectOp>(op))
    return true;
  auto effects = dyn_cast<MemoryEffectOpInterface>(op);
  return effects && effects.hasNoEffect();
}

static bool validateLoopBody(ReductionCandidate &candidate) {
  llvm::SmallPtrSet<Operation *, 16> nested;
  candidate.loop.getBody()->walk([&](Operation *op) {
    if (op == candidate.loop.getOperation())
      return WalkResult::advance();
    if (isa<scf::ForOp, scf::IfOp>(op)) {
      nested.insert(op);
      return WalkResult::interrupt();
    }
    return WalkResult::advance();
  });
  if (!nested.empty())
    return false;

  for (Operation &op : candidate.loop.getBody()->without_terminator()) {
    if (auto store = dyn_cast<memref::StoreOp>(&op)) {
      if (!rank0AccumulatorIndex(store.getMemref(), candidate.accumulators))
        return false;
      continue;
    }
    if (auto load = dyn_cast<memref::LoadOp>(&op)) {
      if (load.getIndices().empty() &&
          rank0AccumulatorIndex(load.getMemRef(), candidate.accumulators))
        continue;
      Value root = ValueAnalysis::stripMemrefViewOps(load.getMemRef());
      std::optional<BlockGeometry1D> geometry =
          findCommittedBlockGeometryForRoot(root);
      if (!geometry)
        return false;
      if (candidate.sourceGeometry.root) {
        if (candidate.sourceGeometry.extent != geometry->extent ||
            candidate.sourceGeometry.blockExtent != geometry->blockExtent ||
            candidate.sourceGeometry.blockCount != geometry->blockCount)
          return false;
      } else {
        candidate.sourceGeometry = *geometry;
      }
      continue;
    }
    if (!isSupportedPureOp(&op))
      return false;
  }
  return static_cast<bool>(candidate.sourceGeometry.root);
}

static bool isValueDefinedInsideCu(Value value, sde::SdeCuRegionOp cu,
                                   Operation *allowedNestedOp) {
  if (auto blockArg = dyn_cast<BlockArgument>(value)) {
    Block *owner = blockArg.getOwner();
    return owner && cu.getBody().isAncestor(owner->getParent());
  }

  Operation *def = value.getDefiningOp();
  if (!def)
    return false;
  if (allowedNestedOp && allowedNestedOp->isAncestor(def))
    return false;
  Region *parentRegion = def->getParentRegion();
  return parentRegion && cu.getBody().isAncestor(parentRegion);
}

static bool canHoistProducerOutsideParentCu(ReductionCandidate &candidate,
                                            sde::SdeCuRegionOp parentCu) {
  if (!parentCu || parentCu.getNumResults() != 0 ||
      !parentCu.getIterArgs().empty())
    return false;
  if (candidate.loop->getBlock() != &parentCu.getBody().front())
    return false;

  auto valueCanHoist = [&](Value value) {
    return !isValueDefinedInsideCu(value, parentCu, candidate.loop);
  };
  for (const AccumulatorUpdate &acc : candidate.accumulators)
    if (!valueCanHoist(acc.memref))
      return false;
  if (!valueCanHoist(candidate.loop.getLowerBound()) ||
      !valueCanHoist(candidate.loop.getUpperBound()) ||
      !valueCanHoist(candidate.loop.getStep()))
    return false;

  for (Operation &op : candidate.loop.getBody()->without_terminator()) {
    for (Value operand : op.getOperands()) {
      if (operand == candidate.loop.getInductionVar())
        continue;
      if (!valueCanHoist(operand))
        return false;
    }
  }
  return true;
}

static std::optional<ReductionCandidate> matchReductionLoop(scf::ForOp loop) {
  if (loop->getParentOfType<sde::SdeSuIterateOp>())
    return std::nullopt;
  sde::SdeCuRegionOp cu = loop->getParentOfType<sde::SdeCuRegionOp>();
  if (!cu || cu.getKind() != sde::SdeCuKind::single)
    return std::nullopt;
  if (loop.getNumResults() != 0 || loop.getBody()->getNumArguments() != 1)
    return std::nullopt;

  ReductionCandidate candidate;
  candidate.loop = loop;
  int64_t upper = 0;
  if (!isZeroBasedStaticLoop(loop, upper, candidate.step))
    return std::nullopt;

  DenseMap<Value, unsigned> accumulatorIndex;
  for (Operation &op : loop.getBody()->without_terminator()) {
    auto store = dyn_cast<memref::StoreOp>(&op);
    if (!store)
      continue;
    std::optional<AccumulatorUpdate> update = matchAccumulatorStore(store);
    if (!update)
      return std::nullopt;
    Value root = update->memref;
    auto [it, inserted] =
        accumulatorIndex.try_emplace(root, candidate.accumulators.size());
    if (inserted)
      candidate.accumulators.push_back(*update);
  }
  if (candidate.accumulators.empty())
    return std::nullopt;

  if (!validateLoopBody(candidate))
    return std::nullopt;
  if (upper > candidate.sourceGeometry.extent)
    return std::nullopt;

  bool hasFloat =
      llvm::any_of(candidate.accumulators,
                   [](const AccumulatorUpdate &acc) { return acc.isFloat; });
  if (hasFloat && candidate.step < candidate.sourceGeometry.blockExtent) {
    candidate.preserveFloatOrder = true;
    candidate.partialSlots =
        ceilDiv(candidate.sourceGeometry.blockExtent, candidate.step);
  }

  return candidate;
}

static Value constantIndex(OpBuilder &builder, Location loc, int64_t value) {
  return arith::ConstantIndexOp::create(builder, loc, value);
}

static Value zeroForType(OpBuilder &builder, Location loc, Type type) {
  if (isa<FloatType>(type))
    return arith::ConstantOp::create(builder, loc,
                                     builder.getFloatAttr(type, 0.0));
  if (auto intType = dyn_cast<IntegerType>(type))
    return arith::ConstantOp::create(builder, loc,
                                     builder.getIntegerAttr(intType, 0));
  if (isa<IndexType>(type))
    return constantIndex(builder, loc, 0);
  return {};
}

static Value buildAdd(OpBuilder &builder, Location loc, Value lhs, Value rhs) {
  if (isa<FloatType>(lhs.getType()))
    return arith::AddFOp::create(builder, loc, lhs, rhs);
  return arith::AddIOp::create(builder, loc, lhs, rhs);
}

static Value buildFirstIndexInBlock(OpBuilder &builder, Location loc,
                                    Value blockIv, int64_t step,
                                    int64_t blockExtent) {
  Value blockSize = constantIndex(builder, loc, blockExtent);
  Value blockStart = arith::MulIOp::create(builder, loc, blockIv, blockSize);
  if (step > 0 && blockExtent % step == 0)
    return blockStart;
  Value stepValue = constantIndex(builder, loc, step);
  Value rem = arith::RemUIOp::create(builder, loc, blockStart, stepValue);
  Value zero = constantIndex(builder, loc, 0);
  Value isAligned =
      arith::CmpIOp::create(builder, loc, arith::CmpIPredicate::eq, rem, zero);
  Value adjust = arith::SubIOp::create(builder, loc, stepValue, rem);
  Value rounded = arith::AddIOp::create(builder, loc, blockStart, adjust);
  return arith::SelectOp::create(builder, loc, isAligned, blockStart, rounded);
}

static Value buildBlockEnd(OpBuilder &builder, Location loc, Value blockIv,
                           Value loopUpper, int64_t blockExtent) {
  Value one = constantIndex(builder, loc, 1);
  Value blockSize = constantIndex(builder, loc, blockExtent);
  Value nextBlock = arith::AddIOp::create(builder, loc, blockIv, one);
  Value rawEnd = arith::MulIOp::create(builder, loc, nextBlock, blockSize);
  return arith::MinUIOp::create(builder, loc, rawEnd, loopUpper);
}

static bool hasAccumulatorSlot(const ReductionCandidate &candidate) {
  return candidate.preserveFloatOrder || candidate.accumulators.size() > 1;
}

static MemRefType buildPartialType(MLIRContext *ctx,
                                   const ReductionCandidate &candidate) {
  Type elementType = candidate.accumulators.front().elementType;
  SmallVector<int64_t, 2> shape;
  shape.push_back(candidate.sourceGeometry.blockCount);
  if (candidate.sourceGeometry.rankExpandedMu)
    shape.push_back(1);
  if (candidate.preserveFloatOrder) {
    shape.push_back(candidate.partialSlots);
    if (hasAccumulatorSlot(candidate))
      shape.push_back(static_cast<int64_t>(candidate.accumulators.size()));
  } else if (hasAccumulatorSlot(candidate)) {
    shape.push_back(static_cast<int64_t>(candidate.accumulators.size()));
  }
  return MemRefType::get(shape, elementType);
}

static SmallVector<int64_t, 4>
partialPhysicalBlockShape(const ReductionCandidate &candidate) {
  SmallVector<int64_t, 4> shape;
  shape.push_back(1);
  if (candidate.preserveFloatOrder)
    shape.push_back(candidate.partialSlots);
  if (hasAccumulatorSlot(candidate))
    shape.push_back(static_cast<int64_t>(candidate.accumulators.size()));
  return shape;
}

static int64_t nextInternalArrayId(Operation *anchor) {
  Operation *scope = anchor;
  if (ModuleOp module = anchor->getParentOfType<ModuleOp>())
    scope = module.getOperation();

  int64_t maxId = -1;
  auto record = [&](IntegerAttr attr) {
    if (attr)
      maxId = std::max(maxId, attr.getInt());
  };
  scope->walk([&](sde::SdeArrayLayoutRootOp root) {
    maxId = std::max<int64_t>(maxId, root.getArrayId());
  });
  scope->walk(
      [&](sde::SdeMuAccessWindowOp win) { record(win.getArrayIdAttr()); });
  scope->walk([&](sde::SdeSuHaloOp halo) { record(halo.getArrayIdAttr()); });
  scope->walk([&](sde::SdeSuReduceScatterOp reduce) {
    record(reduce.getArrayIdAttr());
  });
  return maxId + 1;
}

static SmallVector<Value, 4> partialIndices(const ReductionCandidate &candidate,
                                            OpBuilder &builder, Location loc,
                                            Value blockIv, Value partialSlot,
                                            unsigned accumulatorSlot) {
  SmallVector<Value, 4> indices;
  indices.push_back(blockIv);
  if (candidate.sourceGeometry.rankExpandedMu)
    indices.push_back(constantIndex(builder, loc, 0));
  if (candidate.preserveFloatOrder)
    indices.push_back(partialSlot);
  if (hasAccumulatorSlot(candidate))
    indices.push_back(constantIndex(builder, loc, accumulatorSlot));
  return indices;
}

static SmallVector<memref::AllocaOp, 4>
createLocalAccumulators(ReductionCandidate &candidate, OpBuilder &builder,
                        Location loc) {
  SmallVector<memref::AllocaOp, 4> locals;
  for (const AccumulatorUpdate &acc : candidate.accumulators) {
    auto localType = MemRefType::get({}, acc.elementType);
    auto local = memref::AllocaOp::create(builder, loc, localType);
    Value zeroValue = zeroForType(builder, loc, acc.elementType);
    memref::StoreOp::create(builder, loc, zeroValue, local.getMemref(),
                            ValueRange{});
    locals.push_back(local);
  }
  return locals;
}

static void cloneReductionBody(ReductionCandidate &candidate,
                               Value replacementIv,
                               ArrayRef<memref::AllocaOp> locals,
                               OpBuilder &builder) {
  IRMapping mapper;
  mapper.map(candidate.loop.getInductionVar(), replacementIv);
  for (auto [idx, acc] : llvm::enumerate(candidate.accumulators))
    mapper.map(acc.memref, locals[idx]->getResult(0));
  for (Operation &op : candidate.loop.getBody()->without_terminator())
    builder.clone(op, mapper);
}

static void storePartials(ReductionCandidate &candidate, Value partial,
                          Value blockIv, Value partialSlot,
                          ArrayRef<memref::AllocaOp> locals, OpBuilder &builder,
                          Location loc) {
  for (auto [idx, local] : llvm::enumerate(locals)) {
    Value value = memref::LoadOp::create(builder, loc, local->getResult(0));
    memref::StoreOp::create(builder, loc, value, partial,
                            partialIndices(candidate, builder, loc, blockIv,
                                           partialSlot,
                                           static_cast<unsigned>(idx)));
  }
}

static scf::ForOp
createOrderPreservingProducerBody(ReductionCandidate &candidate, Value partial,
                                  Value blockIv, Value first, Value end,
                                  OpBuilder &builder, Location loc) {
  Value zero = constantIndex(builder, loc, 0);
  Value one = constantIndex(builder, loc, 1);
  Value slots = constantIndex(builder, loc, candidate.partialSlots);
  auto slotLoop = scf::ForOp::create(builder, loc, zero, slots, one);

  builder.setInsertionPointToStart(slotLoop.getBody());
  Value slotIv = slotLoop.getInductionVar();
  SmallVector<memref::AllocaOp, 4> locals =
      createLocalAccumulators(candidate, builder, loc);
  Value offset =
      arith::MulIOp::create(builder, loc, slotIv, candidate.loop.getStep());
  Value sampleIv = arith::AddIOp::create(builder, loc, first, offset);
  Value valid = arith::CmpIOp::create(builder, loc, arith::CmpIPredicate::ult,
                                      sampleIv, end);
  auto ifOp = scf::IfOp::create(builder, loc, TypeRange{}, valid,
                                /*withElseRegion=*/false);
  builder.setInsertionPointToStart(&ifOp.getThenRegion().front());
  cloneReductionBody(candidate, sampleIv, locals, builder);

  builder.setInsertionPointAfter(ifOp);
  storePartials(candidate, partial, blockIv, slotIv, locals, builder, loc);
  return slotLoop;
}

static void createBlockSummingProducerBody(ReductionCandidate &candidate,
                                           Value partial, Value blockIv,
                                           Value first, Value end,
                                           OpBuilder &builder, Location loc) {
  SmallVector<memref::AllocaOp, 4> locals =
      createLocalAccumulators(candidate, builder, loc);
  auto localLoop =
      scf::ForOp::create(builder, loc, first, end, candidate.loop.getStep());
  builder.setInsertionPointToStart(localLoop.getBody());
  cloneReductionBody(candidate, localLoop.getInductionVar(), locals, builder);

  builder.setInsertionPointAfter(localLoop);
  storePartials(candidate, partial, blockIv, constantIndex(builder, loc, 0),
                locals, builder, loc);
}

static std::optional<sde::SdeMuAllocOp>
findMuAllocForMemref(Value memref, ModuleOp module) {
  Value stripped = ValueAnalysis::stripMemrefViewOps(memref);
  if (auto mu = stripped.getDefiningOp<sde::SdeMuAllocOp>())
    return mu;
  if (!module)
    return std::nullopt;
  std::optional<sde::SdeMuAllocOp> found;
  module.walk([&](sde::SdeMuAllocOp mu) {
    if (ValueAnalysis::stripMemrefViewOps(mu.getMemref()) != stripped)
      return WalkResult::advance();
    found = mu;
    return WalkResult::interrupt();
  });
  return found;
}

static bool canRaiseSourceReadWindow(Value sourceRoot, ModuleOp module) {
  std::optional<sde::SdeMuAllocOp> mu =
      findMuAllocForMemref(sourceRoot, module);
  if (!mu)
    return true;
  for (const sde::RaisedWindowSpec &spec : sde::queryAccessWindows(*mu))
    if (spec.mode == sde::SdeAccessMode::read)
      return true;
  return false;
}

static std::optional<int64_t> arrayIdForSourceMemref(Value sourceRoot,
                                                     ModuleOp module) {
  if (auto sourceMu = sourceRoot.getDefiningOp<sde::SdeMuAllocOp>())
    return sde::getMuArrayIdFromLayoutRoot(sourceMu);
  Value stripped = ValueAnalysis::stripMemrefViewOps(sourceRoot);
  if (module) {
    std::optional<int64_t> found;
    module.walk([&](sde::SdeMuAllocOp mu) {
      if (ValueAnalysis::stripMemrefViewOps(mu.getMemref()) != stripped)
        return WalkResult::advance();
      if (std::optional<int64_t> id = sde::getMuArrayIdFromLayoutRoot(mu)) {
        found = id;
        return WalkResult::interrupt();
      }
      return WalkResult::advance();
    });
    if (found)
      return found;
  }
  for (Operation *user : stripped.getUsers()) {
    if (auto root = dyn_cast<sde::SdeArrayLayoutRootOp>(user))
      return static_cast<int64_t>(root.getArrayId());
  }
  return std::nullopt;
}

static Value primarySourceLoadMemref(ReductionCandidate &candidate) {
  for (Operation &op : candidate.loop.getBody()->without_terminator()) {
    auto load = dyn_cast<memref::LoadOp>(&op);
    if (!load || load.getIndices().empty())
      continue;
    if (rank0AccumulatorIndex(load.getMemRef(), candidate.accumulators))
      continue;
    return ValueAnalysis::stripMemrefViewOps(load.getMemRef());
  }
  return candidate.sourceGeometry.root;
}

static sde::SdeSuIterateOp createProducer(ReductionCandidate &candidate,
                                          Value partial,
                                          std::optional<int64_t> partialArrayId,
                                          OpBuilder &builder) {
  MLIRContext *ctx = builder.getContext();
  Location loc = candidate.loop.getLoc();
  Value zero = constantIndex(builder, loc, 0);
  Value one = constantIndex(builder, loc, 1);
  Value blockCount =
      constantIndex(builder, loc, candidate.sourceGeometry.blockCount);

  sde::SuIterateAttrs suAttrs;
  suAttrs.structuredClassification = sde::SdeStructuredClassificationAttr::get(
      ctx, sde::SdeStructuredClassification::elementwise);
  auto su = sde::buildSuIterate(builder, loc, ValueRange{zero},
                                ValueRange{blockCount}, ValueRange{one}, suAttrs);

  Block &suBody = sde::ensureBlock(su.getBody());
  if (suBody.getNumArguments() == 0)
    suBody.addArgument(builder.getIndexType(), loc);
  Value blockIv = suBody.getArgument(0);

  builder.setInsertionPointToStart(&suBody);
  if (partialArrayId)
    sde::SdeArrayLayoutRootOp::create(
        builder, loc, partial,
        sde::SdeAccessModeAttr::get(ctx, sde::SdeAccessMode::write),
        builder.getI64IntegerAttr(*partialArrayId));
  Value sourceRoot = primarySourceLoadMemref(candidate);
  ModuleOp module = candidate.loop->getParentOfType<ModuleOp>();
  std::optional<int64_t> sourceArrayId =
      arrayIdForSourceMemref(sourceRoot, module);
  if (sourceArrayId)
    sde::SdeArrayLayoutRootOp::create(
        builder, loc, sourceRoot,
        sde::SdeAccessModeAttr::get(ctx, sde::SdeAccessMode::read),
        builder.getI64IntegerAttr(*sourceArrayId));
  auto cu = sde::buildCuRegion(
      builder, loc, sde::SdeCuKindAttr::get(ctx, sde::SdeCuKind::parallel));
  Block &cuBody = sde::ensureBlock(cu.getBody());
  builder.setInsertionPointToStart(&cuBody);

  Value first = buildFirstIndexInBlock(builder, loc, blockIv, candidate.step,
                                       candidate.sourceGeometry.blockExtent);
  Value end =
      buildBlockEnd(builder, loc, blockIv, candidate.loop.getUpperBound(),
                    candidate.sourceGeometry.blockExtent);
  if (candidate.preserveFloatOrder) {
    scf::ForOp slotLoop = createOrderPreservingProducerBody(
        candidate, partial, blockIv, first, end, builder, loc);
    builder.setInsertionPointAfter(slotLoop);
  } else {
    createBlockSummingProducerBody(candidate, partial, blockIv, first, end,
                                   builder, loc);
  }
  sde::SdeYieldOp::create(builder, loc, ValueRange{});

  builder.setInsertionPointToEnd(&suBody);
  sde::SdeYieldOp::create(builder, loc, ValueRange{});
  if (std::optional<SmallVector<int64_t, 2>> blockShape =
          partialPhysicalBlockShape(candidate))
    (void)sde::commitWriterPhysicalLayoutFacts(su, {0}, *blockShape, {1});
  return su;
}

static void createFinalCombine(ReductionCandidate &candidate, Value partial,
                               OpBuilder &builder) {
  Location loc = candidate.loop.getLoc();
  Value zero = constantIndex(builder, loc, 0);
  Value one = constantIndex(builder, loc, 1);
  Value blockCount =
      constantIndex(builder, loc, candidate.sourceGeometry.blockCount);
  auto combine = scf::ForOp::create(builder, loc, zero, blockCount, one);

  builder.setInsertionPointToStart(combine.getBody());
  Value blockIv = combine.getInductionVar();
  auto emitAccumulates = [&](Value partialSlot) {
    for (auto [idx, acc] : llvm::enumerate(candidate.accumulators)) {
      Value old = memref::LoadOp::create(builder, loc, acc.memref);
      Value partialValue = memref::LoadOp::create(
          builder, loc, partial,
          partialIndices(candidate, builder, loc, blockIv, partialSlot,
                         static_cast<unsigned>(idx)));
      Value sum = buildAdd(builder, loc, old, partialValue);
      memref::StoreOp::create(builder, loc, sum, acc.memref, ValueRange{});
    }
  };
  if (candidate.partialSlots == 1) {
    emitAccumulates(constantIndex(builder, loc, 0));
    builder.setInsertionPointAfter(combine);
    return;
  }
  auto slotLoop = scf::ForOp::create(
      builder, loc, zero, constantIndex(builder, loc, candidate.partialSlots),
      one);
  builder.setInsertionPointToStart(slotLoop.getBody());
  emitAccumulates(slotLoop.getInductionVar());
  builder.setInsertionPointAfter(combine);
}

static sde::SdeCuRegionOp createEmptySingleCuAfter(Operation *anchor,
                                                   Location loc) {
  OpBuilder builder(anchor);
  builder.setInsertionPointAfter(anchor);
  auto cu = sde::buildCuRegion(
      builder, loc,
      sde::SdeCuKindAttr::get(anchor->getContext(), sde::SdeCuKind::single));
  cu.setSerialReasonAttr(sde::SdeSerialReasonAttr::get(
      anchor->getContext(), sde::SdeSerialReason::reduction_combine));
  Block &body = sde::ensureBlock(cu.getBody());
  OpBuilder bodyBuilder(cu.getContext());
  bodyBuilder.setInsertionPointToEnd(&body);
  sde::SdeYieldOp::create(bodyBuilder, loc, ValueRange{});
  return cu;
}

static bool hasWorkBeforeTerminator(sde::SdeCuRegionOp cu) {
  if (!cu || cu.getBody().empty())
    return false;
  for (Operation &op : cu.getBody().front().without_terminator()) {
    (void)op;
    return true;
  }
  return false;
}

static bool isNestedUnder(Operation *op, Operation *container) {
  for (Operation *cursor = op; cursor; cursor = cursor->getParentOp())
    if (cursor == container)
      return true;
  return false;
}

static Value
cloneExternalProducer(Value value, sde::SdeCuRegionOp source,
                      const llvm::SmallPtrSetImpl<Operation *> &span,
                      IRMapping &mapper, OpBuilder &builder) {
  if (!value)
    return {};
  if (Value mapped = mapper.lookupOrNull(value))
    return mapped;

  Operation *def = value.getDefiningOp();
  if (!def || !isNestedUnder(def, source.getOperation()) || span.contains(def))
    return value;
  if (!isSupportedPureOp(def) || def->getNumRegions() != 0 ||
      def->getNumResults() == 0)
    return {};

  for (Value operand : def->getOperands()) {
    Value mappedOperand =
        cloneExternalProducer(operand, source, span, mapper, builder);
    if (!mappedOperand)
      return {};
    if (mappedOperand != operand)
      mapper.map(operand, mappedOperand);
  }
  Operation *cloned = builder.clone(*def, mapper);
  return mapper.lookupOrNull(value) ? mapper.lookupOrNull(value)
                                    : cloned->getResult(0);
}

static LogicalResult moveFollowingOpsToCu(Operation *anchor,
                                          sde::SdeCuRegionOp source,
                                          sde::SdeCuRegionOp target) {
  SmallVector<Operation *, 8> span;
  for (Operation *next = anchor->getNextNode();
       next && !isa<sde::SdeYieldOp>(next); next = next->getNextNode())
    span.push_back(next);
  llvm::SmallPtrSet<Operation *, 32> spanSet;
  for (Operation *op : span)
    op->walk([&](Operation *nested) { spanSet.insert(nested); });

  Block &targetBody = target.getBody().front();
  Operation *targetYield = targetBody.getTerminator();
  OpBuilder builder(target.getContext());
  builder.setInsertionPoint(targetYield);
  IRMapping mapper;
  for (Operation *op : span) {
    WalkResult result = op->walk([&](Operation *nested) {
      for (OpOperand &operand : nested->getOpOperands()) {
        Value mapped = cloneExternalProducer(operand.get(), source, spanSet,
                                             mapper, builder);
        if (mapped)
          continue;
        InFlightDiagnostic diag = nested->emitError()
                                  << "uses non-cloneable CU-local value "
                                     "across scalar block-reduction CU split: "
                                  << operand.get();
        if (Operation *def = operand.get().getDefiningOp())
          diag << " defined by " << def->getName();
        return WalkResult::interrupt();
      }
      return WalkResult::advance();
    });
    if (result.wasInterrupted())
      return failure();
  }

  for (Operation *op : span)
    op->moveBefore(targetYield);
  for (Operation *op : span) {
    op->walk([&](Operation *nested) {
      for (OpOperand &operand : nested->getOpOperands()) {
        if (Value mapped = mapper.lookupOrNull(operand.get()))
          operand.set(mapped);
      }
    });
  }
  return success();
}

static sde::SdeCuRegionOp
createFinalCombineCuAfter(Operation *anchor, ReductionCandidate &candidate,
                          Value partial) {
  sde::SdeCuRegionOp combineCu =
      createEmptySingleCuAfter(anchor, candidate.loop.getLoc());
  Block &body = combineCu.getBody().front();
  body.getTerminator()->erase();

  OpBuilder builder(combineCu.getContext());
  builder.setInsertionPointToStart(&body);
  createFinalCombine(candidate, partial, builder);
  builder.setInsertionPointToEnd(&body);
  sde::SdeYieldOp::create(builder, candidate.loop.getLoc(), ValueRange{});
  return combineCu;
}

static LogicalResult rewriteReduction(ReductionCandidate candidate) {
  ModuleOp module = candidate.loop->getParentOfType<ModuleOp>();
  if (!canRaiseSourceReadWindow(primarySourceLoadMemref(candidate), module))
    return success();

  sde::SdeCuRegionOp parentCu =
      candidate.loop->getParentOfType<sde::SdeCuRegionOp>();
  if (!canHoistProducerOutsideParentCu(candidate, parentCu))
    return success();

  Location loc = candidate.loop.getLoc();
  OpBuilder outerBuilder(parentCu);
  outerBuilder.setInsertionPointAfter(parentCu);

  MemRefType partialType =
      buildPartialType(outerBuilder.getContext(), candidate);
  Value partial;
  std::optional<int64_t> partialArrayId;
  if (candidate.sourceGeometry.rankExpandedMu) {
    partialArrayId = nextInternalArrayId(parentCu.getOperation());
    auto partialAlloc =
        sde::SdeMuAllocOp::create(outerBuilder, loc, partialType, ValueRange{});
    partial = partialAlloc.getMemref();
  } else {
    auto allocCu = sde::buildCuRegion(
        outerBuilder, loc,
        sde::SdeCuKindAttr::get(outerBuilder.getContext(),
                                sde::SdeCuKind::single),
        /*nowait=*/nullptr, /*iterArgs=*/ValueRange{},
        /*resultTypes=*/TypeRange{partialType});
    allocCu.setSerialReasonAttr(sde::SdeSerialReasonAttr::get(
        outerBuilder.getContext(), sde::SdeSerialReason::reduction_combine));
    Block &allocBody = sde::ensureBlock(allocCu.getBody());
    outerBuilder.setInsertionPointToStart(&allocBody);
    Value alloc =
        memref::AllocOp::create(outerBuilder, loc, partialType).getMemref();
    sde::SdeYieldOp::create(outerBuilder, loc, ValueRange{alloc});
    partial = allocCu.getResult(0);
    outerBuilder.setInsertionPointAfter(allocCu);
  }
  sde::SdeSuIterateOp producer =
      createProducer(candidate, partial, partialArrayId, outerBuilder);
  sde::SdeCuRegionOp combineCu =
      createFinalCombineCuAfter(producer.getOperation(), candidate, partial);
  sde::SdeCuRegionOp trailingCu;
  if (Operation *next = candidate.loop->getNextNode())
    if (!isa<sde::SdeYieldOp>(next)) {
      trailingCu = createEmptySingleCuAfter(combineCu.getOperation(), loc);
      if (failed(moveFollowingOpsToCu(candidate.loop.getOperation(), parentCu,
                                      trailingCu)))
        return failure();
    }
  candidate.loop.erase();
  if (!hasWorkBeforeTerminator(parentCu))
    parentCu.erase();
  return success();
}

struct SdeScalarBlockReductionPass
    : public sde::impl::SdeScalarBlockReductionBase<
          SdeScalarBlockReductionPass> {
  void runOnOperation() override {
    SmallVector<scf::ForOp, 8> loops;
    getOperation().walk([&](scf::ForOp loop) { loops.push_back(loop); });

    for (scf::ForOp loop : loops) {
      if (!loop || loop->getParentOfType<sde::SdeSuIterateOp>())
        continue;
      std::optional<ReductionCandidate> candidate = matchReductionLoop(loop);
      if (!candidate)
        continue;
      if (failed(rewriteReduction(*candidate))) {
        signalPassFailure();
        return;
      }
    }
    (void)sde::reconcileReaderArrayLayoutsWithCommittedWriterShapes(
        getOperation());
  }
};

} // namespace

namespace mlir::carts::sde {

std::unique_ptr<Pass> createSdeScalarBlockReductionPass() {
  return std::make_unique<SdeScalarBlockReductionPass>();
}

} // namespace mlir::carts::sde
