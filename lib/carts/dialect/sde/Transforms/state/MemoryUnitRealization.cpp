///==========================================================================///
/// File: MemoryUnitRealization.cpp
///
/// Realize SDE shared memrefs as SDE memory units.
///==========================================================================///

#include "carts/dialect/sde/Analysis/SdeAnalysisUtils.h"
#include "carts/dialect/sde/Analysis/SuLoopAccessAnalysis.h"
#include "carts/dialect/sde/Transforms/Passes.h"
#include "carts/dialect/sde/Utils/SdeCommittedFactUtils.h"
#include "carts/utils/ArrayAttrUtils.h"
namespace mlir::carts::sde {
#define GEN_PASS_DEF_MEMORYUNITREALIZATION
#include "carts/dialect/sde/Transforms/Passes.h.inc"
} // namespace mlir::carts::sde

#include "carts/utils/ValueAnalysis.h"

#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/PatternMatch.h"
#include "polygeist/Ops.h"
#include "llvm/ADT/SetVector.h"

using namespace mlir;
using namespace mlir::carts;

namespace {

static bool hasNestedMemrefElement(Value value) {
  auto type = dyn_cast<MemRefType>(value.getType());
  return type && isa<MemRefType>(type.getElementType());
}

static bool isMuRealizableAllocation(Value root) {
  if (!root || !isa<MemRefType>(root.getType()) || hasNestedMemrefElement(root))
    return false;
  if (cast<MemRefType>(root.getType()).getRank() == 0)
    return false;

  Operation *def = root.getDefiningOp();
  if (!def)
    return false;

  if (auto alloc = dyn_cast<memref::AllocOp>(def))
    return alloc.getSymbolOperands().empty();
  if (auto alloca = dyn_cast<memref::AllocaOp>(def))
    return alloca.getSymbolOperands().empty();
  return false;
}

static Value resolveRealizableStorageRoot(Value root) {
  root = ValueAnalysis::stripMemrefViewOps(root);
  SmallVector<Value, 4> seen;
  while (root) {
    if (llvm::is_contained(seen, root))
      return Value();
    seen.push_back(root);
    if (isMuRealizableAllocation(root))
      return root;

    auto result = dyn_cast<OpResult>(root);
    auto cu = result ? dyn_cast<sde::SdeCuRegionOp>(result.getOwner())
                     : sde::SdeCuRegionOp();
    if (!cu || cu.getBody().empty())
      return root;
    auto yield =
        dyn_cast_or_null<sde::SdeYieldOp>(cu.getBody().front().getTerminator());
    if (!yield || result.getResultNumber() >= yield.getValues().size())
      return root;
    root = ValueAnalysis::stripMemrefViewOps(
        yield.getValues()[result.getResultNumber()]);
  }
  return Value();
}

static bool isPrivateAllocationForSchedulingUnit(Value root,
                                                 sde::SdeSuIterateOp op) {
  if (!root || !op)
    return false;
  Operation *def = root.getDefiningOp();
  if (!isa_and_nonnull<memref::AllocOp, memref::AllocaOp>(def))
    return false;

  bool sawUse = false;
  for (Operation *user : root.getUsers()) {
    if (user == def)
      continue;
    sawUse = true;
    if (!op->isAncestor(user))
      return false;
  }
  return sawUse;
}

static bool hasPhysicalOwnerSliceLayout(sde::SdeSuIterateOp op) {
  return sde::hasCommittedWriterBlockLayout(op);
}

static std::optional<sde::LoopIndexedOutputShape>
getUnclassifiedOwnerSliceLayout(sde::SdeSuIterateOp op) {
  if (!op || !hasPhysicalOwnerSliceLayout(op) ||
      op.getStructuredClassificationAttr())
    return std::nullopt;

  std::optional<SmallVector<int64_t, 4>> committedOwnerDims;
  if (std::optional<sde::LayoutGraphFact> writeLayout =
          sde::findSingleCommittedWriterBlockLayout(op))
    committedOwnerDims = writeLayout->ownerDims;
  else
    committedOwnerDims = readI64ArrayAttr(op.getPhysicalOwnerDimsAttr());
  if (!committedOwnerDims || committedOwnerDims->empty())
    return std::nullopt;

  SmallVector<Value, 4> ownerIndexValues = sde::collectOwnerIndexValues(op);
  if (ownerIndexValues.size() < committedOwnerDims->size())
    return std::nullopt;

  sde::StructuredMemoryEffectSummary effects =
      sde::collectStructuredMemoryEffects(op.getBody());
  if (effects.hasUnknownEffects || effects.writes.empty())
    return std::nullopt;

  for (Value read : effects.reads) {
    if (sde::isDefinedInside(op.getOperation(), read))
      continue;
    if (!effects.writes.contains(read))
      return std::nullopt;
  }

  bool rejected = false;
  std::optional<sde::LoopIndexedOutputShape> outputShape;
  op.getBody().walk([&](memref::StoreOp storeOp) {
    if (rejected)
      return;
    Value base = ValueAnalysis::stripMemrefViewOps(storeOp.getMemref());
    if (!base || sde::isDefinedInside(op.getOperation(), base))
      return;
    auto memRefType = dyn_cast<MemRefType>(base.getType());
    if (!memRefType || memRefType.getRank() == 0 ||
        !memRefType.hasStaticShape() || storeOp.getIndices().empty()) {
      rejected = true;
      return;
    }

    if (storeOp.getIndices().size() <
        static_cast<size_t>(memRefType.getRank())) {
      rejected = true;
      return;
    }
    for (auto [slot, physicalDim] : llvm::enumerate(*committedOwnerDims)) {
      if (physicalDim < 0 ||
          static_cast<size_t>(physicalDim) >= storeOp.getIndices().size()) {
        rejected = true;
        return;
      }
      if (!sde::isOwnerDependentIndex(storeOp.getIndices()[physicalDim],
                                      ownerIndexValues[slot])) {
        rejected = true;
        return;
      }
    }

    SmallVector<int64_t, 4> shape(memRefType.getShape().begin(),
                                  memRefType.getShape().end());
    if (!outputShape) {
      outputShape = sde::LoopIndexedOutputShape{
          base, std::move(shape),
          SmallVector<int64_t, 4>(committedOwnerDims->begin(),
                                  committedOwnerDims->end())};
      return;
    }
    if (outputShape->root != base || outputShape->shape != shape)
      rejected = true;
  });

  if (rejected || !outputShape)
    return std::nullopt;
  return outputShape;
}

static bool canRealizeCommittedOwnerSlices(sde::SdeSuIterateOp op) {
  if (!hasPhysicalOwnerSliceLayout(op))
    return true;

  auto classification = op.getStructuredClassification();
  if (!classification)
    return getUnclassifiedOwnerSliceLayout(op).has_value();

  switch (*classification) {
  case sde::SdeStructuredClassification::matmul:
  case sde::SdeStructuredClassification::elementwise:
  case sde::SdeStructuredClassification::elementwise_pipeline:
  case sde::SdeStructuredClassification::stencil:
    return true;
  case sde::SdeStructuredClassification::reduction:
    return op.getReductionAccumulators().empty() &&
           (sde::findLoopIndexedOutputShape(op).has_value() ||
            sde::findConsistentLoopIndexedOutputShapeWithOwnerDims(op)
                .has_value());
  }
  return false;
}

static LogicalResult
rejectUnsupportedPhysicalStorageLayout(sde::SdeSuIterateOp op) {
  if (!op || !hasPhysicalOwnerSliceLayout(op) ||
      canRealizeCommittedOwnerSlices(op))
    return success();

  return op.emitOpError()
         << "has committed physical storage layout facts that this pass cannot "
            "realize; refusing to strip layout evidence";
}

static void collectSchedulingUnitMemrefRoots(
    sde::SdeSuIterateOp op, SetVector<Value> &roots,
    DenseMap<Value, SetVector<Value>> &aliasesByRoot) {
  if (!op || !canRealizeCommittedOwnerSlices(op))
    return;

  op.getBody().walk([&](Operation *nested) {
    Value memref;
    if (auto load = dyn_cast<memref::LoadOp>(nested)) {
      if (isa<MemRefType>(load.getResult().getType()))
        return;
      memref = load.getMemref();
    } else if (auto store = dyn_cast<memref::StoreOp>(nested)) {
      if (isa<MemRefType>(store.getValueToStore().getType()))
        return;
      memref = store.getMemref();
    } else {
      return;
    }

    Value root = ::mlir::carts::ValueAnalysis::stripMemrefViewOps(memref);
    if (!root || sde::isDefinedInside(op.getOperation(), root))
      return;
    if (isPrivateAllocationForSchedulingUnit(root, op))
      return;
    Value storageRoot = resolveRealizableStorageRoot(root);
    if (isMuRealizableAllocation(storageRoot)) {
      roots.insert(storageRoot);
      if (storageRoot != root) {
        aliasesByRoot[storageRoot].insert(root);
        auto result = dyn_cast<OpResult>(root);
        auto cu = result ? dyn_cast<sde::SdeCuRegionOp>(result.getOwner())
                         : sde::SdeCuRegionOp();
        if (cu)
          for (Value sibling : cu->getResults())
            if (isa<MemRefType>(sibling.getType()) &&
                resolveRealizableStorageRoot(sibling) == storageRoot)
              aliasesByRoot[storageRoot].insert(sibling);
      }
    }
  });
}

static void eraseDeallocUsers(Value root, PatternRewriter &rewriter) {
  SmallVector<memref::DeallocOp> deallocs;
  for (Operation *user : llvm::make_early_inc_range(root.getUsers()))
    if (auto dealloc = dyn_cast<memref::DeallocOp>(user))
      deallocs.push_back(dealloc);

  for (memref::DeallocOp dealloc : deallocs)
    rewriter.eraseOp(dealloc);
}

static bool isNullPointer(Value value) {
  return value && value.getDefiningOp<LLVM::ZeroOp>();
}

static std::optional<bool> foldMuNullComparison(LLVM::ICmpOp cmp,
                                                Value muPointer) {
  if (!cmp || !muPointer)
    return std::nullopt;
  bool comparesMuToNull =
      (cmp.getLhs() == muPointer && isNullPointer(cmp.getRhs())) ||
      (cmp.getRhs() == muPointer && isNullPointer(cmp.getLhs()));
  if (!comparesMuToNull)
    return std::nullopt;

  switch (cmp.getPredicate()) {
  case LLVM::ICmpPredicate::eq:
    return false;
  case LLVM::ICmpPredicate::ne:
    return true;
  default:
    return std::nullopt;
  }
}

static void foldMuAllocNullChecks(Value muMemref, PatternRewriter &rewriter) {
  SmallVector<polygeist::Memref2PointerOp, 4> pointers;
  for (Operation *user : llvm::make_early_inc_range(muMemref.getUsers()))
    if (auto m2p = dyn_cast<polygeist::Memref2PointerOp>(user))
      pointers.push_back(m2p);

  for (polygeist::Memref2PointerOp m2p : pointers) {
    bool sawUnsupportedUse = false;
    SmallVector<std::pair<LLVM::ICmpOp, bool>, 4> foldedCmps;
    for (Operation *user :
         llvm::make_early_inc_range(m2p.getResult().getUsers())) {
      auto cmp = dyn_cast<LLVM::ICmpOp>(user);
      std::optional<bool> folded = foldMuNullComparison(cmp, m2p.getResult());
      if (!folded) {
        sawUnsupportedUse = true;
        continue;
      }
      foldedCmps.push_back({cmp, *folded});
    }
    if (sawUnsupportedUse)
      continue;
    for (auto [cmp, value] : foldedCmps) {
      rewriter.setInsertionPoint(cmp);
      Value constant = arith::ConstantOp::create(rewriter, cmp.getLoc(),
                                                 rewriter.getBoolAttr(value));
      cmp.getResult().replaceAllUsesWith(constant);
      rewriter.eraseOp(cmp);
    }
    if (m2p->use_empty())
      rewriter.eraseOp(m2p);
  }
}

static Operation *findMuAllocInsertionPoint(Operation *def) {
  Operation *insertionPoint = def;
  for (Operation *parent = def->getParentOp(); parent;
       parent = parent->getParentOp())
    if (isa<sde::SdeCuRegionOp>(parent))
      insertionPoint = parent;
  return insertionPoint;
}

static Operation *
findDominanceSafeMuAllocInsertionPoint(Operation *def,
                                       ArrayRef<Value> dynamicSizes) {
  Operation *insertionPoint = findMuAllocInsertionPoint(def);
  if (insertionPoint == def)
    return insertionPoint;

  for (Value dynamicSize : dynamicSizes)
    if (sde::isDefinedInside(insertionPoint, dynamicSize))
      return def;
  return insertionPoint;
}

static FailureOr<Value> createMuAllocForRoot(Value root,
                                             ArrayRef<Value> aliases,
                                             PatternRewriter &rewriter) {
  Operation *def = root.getDefiningOp();
  if (!def)
    return failure();

  SmallVector<Value> dynamicSizes;
  if (auto alloc = dyn_cast<memref::AllocOp>(def)) {
    llvm::append_range(dynamicSizes, alloc.getDynamicSizes());
  } else if (auto alloca = dyn_cast<memref::AllocaOp>(def)) {
    llvm::append_range(dynamicSizes, alloca.getDynamicSizes());
  } else {
    return failure();
  }

  Operation *insertionPoint =
      findDominanceSafeMuAllocInsertionPoint(def, dynamicSizes);
  bool insertedAtRootDefinition = insertionPoint == def;

  auto memrefType = cast<MemRefType>(root.getType());
  OpBuilder::InsertionGuard guard(rewriter);
  rewriter.setInsertionPoint(insertionPoint);
  auto muAlloc = sde::SdeMuAllocOp::create(rewriter, def->getLoc(), memrefType,
                                           ValueRange(dynamicSizes));

  DenseMap<Type, Value> replacementByType;
  auto getReplacementForType = [&](Type type) -> FailureOr<Value> {
    if (type == muAlloc.getMemref().getType())
      return muAlloc.getMemref();
    auto sourceType = dyn_cast<MemRefType>(muAlloc.getMemref().getType());
    auto targetType = dyn_cast<MemRefType>(type);
    if (!sourceType || !targetType ||
        !memref::CastOp::areCastCompatible(sourceType, targetType))
      return failure();
    auto [it, inserted] = replacementByType.try_emplace(type, Value());
    if (inserted) {
      OpBuilder::InsertionGuard castGuard(rewriter);
      rewriter.setInsertionPointAfter(muAlloc);
      it->second = memref::CastOp::create(rewriter, def->getLoc(), targetType,
                                          muAlloc.getMemref());
    }
    return it->second;
  };

  for (Value alias : aliases)
    if (alias && alias != root)
      eraseDeallocUsers(alias, rewriter);
  eraseDeallocUsers(root, rewriter);
  if (!insertedAtRootDefinition) {
    for (Value alias : aliases) {
      if (!alias || alias == root)
        continue;
      FailureOr<Value> replacement = getReplacementForType(alias.getType());
      if (failed(replacement))
        return failure();
      alias.replaceAllUsesWith(*replacement);
    }
  }
  SmallVector<OpOperand *, 4> unusedYieldedRoots;
  if (!insertedAtRootDefinition)
    for (OpOperand &use : root.getUses()) {
      auto yield = dyn_cast<sde::SdeYieldOp>(use.getOwner());
      if (!yield)
        continue;
      auto cu = yield->getParentOfType<sde::SdeCuRegionOp>();
      unsigned resultNumber = use.getOperandNumber();
      if (cu && resultNumber < cu->getNumResults() &&
          cu->getResult(resultNumber).use_empty())
        unusedYieldedRoots.push_back(&use);
    }
  root.replaceAllUsesWith(muAlloc.getMemref());
  for (OpOperand *use : unusedYieldedRoots)
    use->set(root);
  foldMuAllocNullChecks(muAlloc.getMemref(), rewriter);
  if (def->use_empty())
    rewriter.eraseOp(def);
  return muAlloc.getMemref();
}

struct MemoryUnitRealizationPass
    : public sde::impl::MemoryUnitRealizationBase<MemoryUnitRealizationPass> {
  void runOnOperation() override {
    ModuleOp module = getOperation();

    SetVector<Value> roots;
    DenseMap<Value, SetVector<Value>> aliasesByRoot;
    bool failedUnsupportedLayout = false;
    module.walk([&](sde::SdeSuIterateOp op) {
      collectSchedulingUnitMemrefRoots(op, roots, aliasesByRoot);
      failedUnsupportedLayout |=
          failed(rejectUnsupportedPhysicalStorageLayout(op));
    });
    if (failedUnsupportedLayout) {
      signalPassFailure();
      return;
    }

    PatternRewriter rewriter(module.getContext());
    for (Value root : roots) {
      if (!isMuRealizableAllocation(root))
        continue;
      SmallVector<Value, 4> aliases;
      if (auto it = aliasesByRoot.find(root); it != aliasesByRoot.end())
        aliases.append(it->second.begin(), it->second.end());
      if (failed(createMuAllocForRoot(root, aliases, rewriter))) {
        if (Operation *def = root.getDefiningOp())
          def->emitError("failed to realize SDE memory unit");
        signalPassFailure();
        return;
      }
    }
  }
};

} // namespace

namespace mlir::carts::sde {

std::unique_ptr<Pass> createMemoryUnitRealizationPass() {
  return std::make_unique<MemoryUnitRealizationPass>();
}

} // namespace mlir::carts::sde
