///==========================================================================///
/// File: EpochTailContinuation.cpp
///
/// Outline final post-epoch host tails into real ARTS continuation EDTs.
///
/// This pass is intentionally conservative: it targets a final main-function
/// tail after a top-level arts.epoch, rewrites DB payload captures into
/// explicit read-only dependencies, carries nonconstant scalars as EDT params,
/// and makes the continuation own runtime shutdown with arts.shutdown.
///==========================================================================///

#include "carts/dialect/arts/IR/ArtsDialect.h"
#include "carts/dialect/arts/Utils/DbUtils.h"
#include "carts/dialect/arts/Utils/OperationAttributes.h"
#include "carts/passes/Passes.h"
#include "carts/utils/LoopUtils.h"
#include "carts/utils/Utils.h"
#include "carts/utils/ValueAnalysis.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/Dominance.h"
#include "mlir/IR/IRMapping.h"
#include "mlir/IR/Operation.h"
#include "mlir/Interfaces/SideEffectInterfaces.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Transforms/RegionUtils.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/SetVector.h"
#include <algorithm>

#include "carts/utils/Debug.h"
#include "llvm/ADT/Statistic.h"
ARTS_DEBUG_SETUP(epoch_tail_continuation);

#define GEN_PASS_DEF_EPOCHTAILCONTINUATION
#include "carts/passes/Passes.h.inc"

static llvm::Statistic numContinuationsAuthored{
    "epoch_tail_continuation", "NumContinuationsAuthored",
    "Number of post-epoch tails outlined into continuation EDTs"};
static llvm::Statistic numContinuationsSkipped{
    "epoch_tail_continuation", "NumContinuationsSkipped",
    "Number of final epoch tails skipped by the conservative outliner"};

using namespace mlir;
using namespace mlir::carts;
using namespace mlir::carts::arts;

namespace {

struct DbCapture {
  Value captured;
  DbRefOp ref;
  DbAcquireOp acquire;
  SmallVector<Value> bodyRefIndices;
};

struct TailWork {
  Operation *frontier = nullptr;
  func::ReturnOp returnOp;
  SmallVector<Operation *, 8> tailOps;
  llvm::SetVector<Value> dbCaptures;
  llvm::SetVector<Value> scalarParams;
  llvm::SetVector<Value> rematerializedCaptures;
};

static bool hasAncestorInSet(Operation *op,
                             const llvm::DenseSet<Operation *> &ops) {
  for (Operation *cur = op; cur; cur = cur->getParentOp())
    if (ops.contains(cur))
      return true;
  return false;
}

static bool isDefinedInside(Value value,
                            const llvm::DenseSet<Operation *> &ops) {
  if (auto blockArg = dyn_cast<BlockArgument>(value)) {
    Operation *parent = blockArg.getOwner()->getParentOp();
    return parent && hasAncestorInSet(parent, ops);
  }
  Operation *def = value.getDefiningOp();
  return def && hasAncestorInSet(def, ops);
}

static bool isOperationInside(Operation *op,
                              const llvm::DenseSet<Operation *> &ops) {
  return op && hasAncestorInSet(op, ops);
}

static bool isConstantLike(Value value) {
  Operation *def = value.getDefiningOp();
  return def && def->hasTrait<OpTrait::ConstantLike>();
}

static bool isExternalStackAlloca(Value value) {
  return isa_and_nonnull<memref::AllocaOp>(value.getDefiningOp());
}

static bool isImplicitNonMemrefCaptureAllowed(Value value) {
  if (isConstantLike(value))
    return true;
  Operation *def = value.getDefiningOp();
  return def && isa<UndefOp>(def);
}

static bool isRematerializableNonMemrefCapture(Value value) {
  Operation *def = value.getDefiningOp();
  return def && isa<LLVM::AddressOfOp>(def);
}

static bool containsAsyncFrontier(Operation *op) {
  if (isa<EpochOp, EdtOp>(op))
    return true;
  bool found = false;
  op->walk([&](Operation *nested) {
    if (nested == op)
      return WalkResult::advance();
    if (isa<EpochOp, EdtOp>(nested)) {
      found = true;
      return WalkResult::interrupt();
    }
    return WalkResult::advance();
  });
  return found;
}

static bool containsEpochFrontier(Operation *op) {
  if (isa<EpochOp>(op))
    return true;
  bool found = false;
  op->walk([&](EpochOp) {
    found = true;
    return WalkResult::interrupt();
  });
  return found;
}

static LogicalResult collectFinalTail(func::FuncOp func, TailWork &work) {
  if (func.getSymName() != "main")
    return failure();
  if (func.getBody().empty() || !llvm::hasSingleElement(func.getBody()))
    return failure();

  Block &entry = func.getBody().front();
  if (entry.empty())
    return failure();

  auto returnOp = dyn_cast<func::ReturnOp>(entry.back());
  if (!returnOp)
    return failure();
  work.returnOp = returnOp;

  SmallVector<Operation *, 8> reversedTail;
  auto it = Block::iterator(returnOp.getOperation());
  while (it != entry.begin()) {
    --it;
    if (containsAsyncFrontier(&*it)) {
      work.frontier = &*it;
      break;
    }
    reversedTail.push_back(&*it);
  }

  if (!work.frontier || reversedTail.empty())
    return failure();

  work.tailOps.assign(reversedTail.rbegin(), reversedTail.rend());
  return success();
}

static bool isDirectRankZeroAlloca(Value value) {
  auto alloca = value.getDefiningOp<memref::AllocaOp>();
  if (!alloca)
    return false;
  auto type = dyn_cast<MemRefType>(alloca.getType());
  return type && type.getRank() == 0;
}

static bool
allStoresToAllocaAreTrueWithDominatingStore(Value memref, memref::LoadOp load,
                                            DominanceInfo &domInfo) {
  if (!isDirectRankZeroAlloca(memref))
    return false;

  bool sawStore = false;
  bool sawDominatingStore = false;
  for (Operation *user : memref.getUsers()) {
    if (auto load = dyn_cast<memref::LoadOp>(user)) {
      if (load.getMemRef() == memref && load.getIndices().empty())
        continue;
      return false;
    }

    if (auto store = dyn_cast<memref::StoreOp>(user)) {
      if (store.getMemRef() != memref || !store.getIndices().empty())
        return false;
      if (!ValueAnalysis::isTrueConstant(store.getValue()))
        return false;
      if (domInfo.dominates(store.getOperation(), load.getOperation()))
        sawDominatingStore = true;
      sawStore = true;
      continue;
    }

    return false;
  }

  return sawStore && sawDominatingStore;
}

static bool canProveTrue(Value value, DominanceInfo &domInfo,
                         unsigned depth = 0) {
  if (!value || depth > 16)
    return false;

  value = ValueAnalysis::stripNumericCasts(value);
  if (ValueAnalysis::isTrueConstant(value))
    return true;
  if (auto folded = ValueAnalysis::tryFoldConstantIndex(value))
    return *folded != 0;

  Operation *def = value.getDefiningOp();
  if (!def)
    return false;

  if (auto load = dyn_cast<memref::LoadOp>(def))
    return load.getIndices().empty() &&
           allStoresToAllocaAreTrueWithDominatingStore(load.getMemRef(), load,
                                                       domInfo);

  if (auto select = dyn_cast<arith::SelectOp>(def)) {
    if (canProveTrue(select.getTrueValue(), domInfo, depth + 1) &&
        canProveTrue(select.getFalseValue(), domInfo, depth + 1))
      return true;
    if (canProveTrue(select.getCondition(), domInfo, depth + 1) &&
        canProveTrue(select.getTrueValue(), domInfo, depth + 1))
      return true;
    return false;
  }

  if (auto andOp = dyn_cast<arith::AndIOp>(def))
    return canProveTrue(andOp.getLhs(), domInfo, depth + 1) &&
           canProveTrue(andOp.getRhs(), domInfo, depth + 1);

  if (auto orOp = dyn_cast<arith::OrIOp>(def))
    return canProveTrue(orOp.getLhs(), domInfo, depth + 1) ||
           canProveTrue(orOp.getRhs(), domInfo, depth + 1);

  return false;
}

static std::optional<int64_t> getConstantTripCount(scf::ForOp loop) {
  if (std::optional<int64_t> tripCount =
          getStaticTripCount(loop.getOperation()))
    return tripCount;

  std::optional<int64_t> lower =
      ValueAnalysis::tryFoldConstantIndex(loop.getLowerBound());
  std::optional<int64_t> upper =
      ValueAnalysis::tryFoldConstantIndex(loop.getUpperBound());
  std::optional<int64_t> step =
      ValueAnalysis::tryFoldConstantIndex(loop.getStep());
  if (!lower || !upper || !step || *step <= 0)
    return std::nullopt;

  int64_t span = std::max<int64_t>(0, *upper - *lower);
  return (span + *step - 1) / *step;
}

static LogicalResult peelFinalIteration(scf::ForOp loop) {
  if (loop.getNumResults() != 0 || !loop.getInitArgs().empty())
    return failure();

  std::optional<int64_t> tripCount = getConstantTripCount(loop);
  if (!tripCount || *tripCount <= 0)
    return failure();

  Location loc = loop.getLoc();
  OpBuilder builder(loop);
  Value finalIv = loop.getLowerBound();

  if (*tripCount > 1) {
    Value tripMinusOne =
        arith::ConstantIndexOp::create(builder, loc, *tripCount - 1);
    Value offset =
        arith::MulIOp::create(builder, loc, loop.getStep(), tripMinusOne);
    finalIv = arith::AddIOp::create(builder, loc, loop.getLowerBound(), offset);

    auto prefixLoop = cast<scf::ForOp>(builder.clone(*loop.getOperation()));
    prefixLoop.setUpperBound(finalIv);
    builder.setInsertionPointAfter(prefixLoop);
  }

  IRMapping mapper;
  mapper.map(loop.getInductionVar(), finalIv);
  for (Operation &op : loop.getBody()->without_terminator())
    builder.clone(op, mapper);

  loop.erase();
  return success();
}

static LogicalResult inlineProvenTrueIf(scf::IfOp ifOp) {
  auto func = ifOp->getParentOfType<func::FuncOp>();
  if (!func)
    return failure();

  DominanceInfo domInfo(func);
  if (ifOp.getNumResults() != 0 || !canProveTrue(ifOp.getCondition(), domInfo))
    return failure();

  Block &thenBlock = ifOp.getThenRegion().front();
  SmallVector<Operation *, 8> opsToMove;
  for (Operation &op : thenBlock.without_terminator())
    opsToMove.push_back(&op);
  for (Operation *op : opsToMove)
    op->moveBefore(ifOp);

  ifOp.erase();
  return success();
}

static LogicalResult exposeFinalNestedEpoch(func::FuncOp func, TailWork &work) {
  if (!work.frontier)
    return failure();
  if (isa<EpochOp>(work.frontier))
    return success();
  if (!containsEpochFrontier(work.frontier))
    return failure();

  for (unsigned depth = 0; depth < 16; ++depth) {
    if (isa<EpochOp>(work.frontier))
      return success();

    if (auto loop = dyn_cast<scf::ForOp>(work.frontier)) {
      if (failed(peelFinalIteration(loop)))
        return failure();
    } else if (auto ifOp = dyn_cast<scf::IfOp>(work.frontier)) {
      if (failed(inlineProvenTrueIf(ifOp)))
        return failure();
    } else {
      return failure();
    }

    TailWork refreshed;
    if (failed(collectFinalTail(func, refreshed)))
      return failure();
    work = std::move(refreshed);
  }

  return isa_and_nonnull<EpochOp>(work.frontier) ? success() : failure();
}

static LogicalResult validateTailIsolation(TailWork &work) {
  llvm::DenseSet<Operation *> tailSet(work.tailOps.begin(), work.tailOps.end());

  for (Value operand : work.returnOp.getOperands())
    if (isDefinedInside(operand, tailSet))
      return failure();

  for (Operation *top : work.tailOps) {
    WalkResult result = top->walk([&](Operation *nested) {
      for (Value result : nested->getResults()) {
        for (Operation *user : result.getUsers()) {
          if (!isOperationInside(user, tailSet))
            return WalkResult::interrupt();
        }
      }
      return WalkResult::advance();
    });
    if (result.wasInterrupted())
      return failure();
  }

  return success();
}

static Operation *getAncestorInBlock(Operation *op, Block *block) {
  Operation *cur = op;
  while (cur && cur->getBlock() != block)
    cur = cur->getParentOp();
  return cur && cur->getBlock() == block ? cur : nullptr;
}

static bool canEraseDeadExternalStackStore(memref::StoreOp store) {
  Value memref = store.getMemRef();
  if (!isExternalStackAlloca(memref) || !store.getIndices().empty())
    return false;

  Block *storeBlock = store->getBlock();
  if (!storeBlock)
    return false;

  for (Operation *user : memref.getUsers()) {
    if (auto load = dyn_cast<memref::LoadOp>(user)) {
      if (load.getMemRef() != memref || !load.getIndices().empty())
        return false;
      Operation *loadAncestor =
          getAncestorInBlock(load.getOperation(), storeBlock);
      if (!loadAncestor)
        return false;
      if (store->isBeforeInBlock(loadAncestor))
        return false;
      continue;
    }

    if (auto otherStore = dyn_cast<memref::StoreOp>(user)) {
      if (otherStore.getMemRef() == memref && otherStore.getIndices().empty())
        continue;
      return false;
    }

    return false;
  }

  return true;
}

static void pruneDeadExternalStackStores(TailWork &work) {
  SmallVector<Operation *, 8> pruned;
  pruned.reserve(work.tailOps.size());
  for (Operation *op : work.tailOps) {
    auto store = dyn_cast<memref::StoreOp>(op);
    if (store && canEraseDeadExternalStackStore(store)) {
      store.erase();
      continue;
    }
    pruned.push_back(op);
  }
  work.tailOps = std::move(pruned);
}

static bool
canRematerializeExternalStackAlloca(Value memref,
                                    const llvm::DenseSet<Operation *> &tailSet,
                                    DominanceInfo &domInfo) {
  auto alloca = memref.getDefiningOp<memref::AllocaOp>();
  if (!alloca || alloca->getNumOperands() != 0)
    return false;

  for (Operation *user : memref.getUsers()) {
    if (auto load = dyn_cast<memref::LoadOp>(user)) {
      if (load.getMemRef() != memref || !load.getIndices().empty())
        return false;
      if (!isOperationInside(load.getOperation(), tailSet))
        return false;

      bool dominatedByTailStore = false;
      for (Operation *maybeStore : memref.getUsers()) {
        auto store = dyn_cast<memref::StoreOp>(maybeStore);
        if (!store || store.getMemRef() != memref ||
            !store.getIndices().empty() ||
            !isOperationInside(store.getOperation(), tailSet))
          continue;
        if (domInfo.dominates(store.getOperation(), load.getOperation())) {
          dominatedByTailStore = true;
          break;
        }
      }
      if (!dominatedByTailStore)
        return false;
      continue;
    }

    if (auto store = dyn_cast<memref::StoreOp>(user)) {
      if (store.getMemRef() == memref && store.getIndices().empty())
        continue;
      return false;
    }

    return false;
  }

  return true;
}

static LogicalResult classifyCaptures(TailWork &work) {
  llvm::DenseSet<Operation *> tailSet(work.tailOps.begin(), work.tailOps.end());
  auto func = work.frontier->getParentOfType<func::FuncOp>();
  if (!func)
    return failure();
  DominanceInfo domInfo(func);

  for (Operation *top : work.tailOps) {
    WalkResult result = top->walk([&](Operation *nested) {
      for (Value operand : nested->getOperands()) {
        if (isDefinedInside(operand, tailSet))
          continue;

        Type type = operand.getType();
        if (type.isIntOrIndexOrFloat()) {
          if (!isConstantLike(operand))
            work.scalarParams.insert(operand);
          continue;
        }

        if (isa<BaseMemRefType>(type)) {
          if (isExternalStackAlloca(operand)) {
            if (canRematerializeExternalStackAlloca(operand, tailSet,
                                                    domInfo)) {
              work.rematerializedCaptures.insert(operand);
              continue;
            }
            return WalkResult::interrupt();
          }
          if (operand.getDefiningOp<DbRefOp>()) {
            work.dbCaptures.insert(operand);
            continue;
          }
          return WalkResult::interrupt();
        }

        if (isImplicitNonMemrefCaptureAllowed(operand))
          continue;
        if (isRematerializableNonMemrefCapture(operand)) {
          work.rematerializedCaptures.insert(operand);
          continue;
        }

        return WalkResult::interrupt();
      }
      return WalkResult::advance();
    });
    if (result.wasInterrupted())
      return failure();
  }

  return success();
}

static Value getGuidForDbSource(Value sourcePtr) {
  Operation *source = DbUtils::getUnderlyingDb(sourcePtr);
  if (auto alloc = dyn_cast_or_null<DbAllocOp>(source))
    return alloc.getGuid();
  if (auto acquire = dyn_cast_or_null<DbAcquireOp>(source))
    return acquire.getGuid();
  return Value();
}

static std::optional<PartitionMode> getPartitionModeForSource(Value sourcePtr) {
  Operation *source = DbUtils::getUnderlyingDb(sourcePtr);
  if (auto alloc = dyn_cast_or_null<DbAllocOp>(source))
    return alloc.getPartitionMode();
  if (auto acquire = dyn_cast_or_null<DbAcquireOp>(source))
    return acquire.getPartitionMode();
  return std::nullopt;
}

static FailureOr<DbCapture>
createDbCaptureAcquire(OpBuilder &builder, Location loc, Value captured) {
  auto ref = captured.getDefiningOp<DbRefOp>();
  if (!ref)
    return failure();

  Value sourcePtr = ref.getSource();
  Value sourceGuid = getGuidForDbSource(sourcePtr);
  if (!sourceGuid)
    return failure();

  SmallVector<Value> offsets(ref.getIndices().begin(), ref.getIndices().end());
  SmallVector<Value> sizes;
  sizes.reserve(offsets.size());
  for (size_t i = 0; i < offsets.size(); ++i)
    sizes.push_back(createOneIndex(builder, loc));

  auto acquire = DbAcquireOp::create(
      builder, loc, ArtsMode::in, sourceGuid, sourcePtr, sourcePtr.getType(),
      getPartitionModeForSource(sourcePtr), /*indices=*/SmallVector<Value>{},
      offsets, sizes, /*partitionIndices=*/SmallVector<Value>{},
      /*partitionOffsets=*/SmallVector<Value>{},
      /*partitionSizes=*/SmallVector<Value>{}, /*boundsValid=*/Value{},
      /*elementOffsets=*/SmallVector<Value>{},
      /*elementSizes=*/SmallVector<Value>{});
  acquire.setRuntimeDbModeAttr(RuntimeDbModeAttr::get(
      acquire.getContext(), DbUtils::orderedRuntimeDbMode(ArtsMode::in)));

  if (Operation *source = DbUtils::getUnderlyingDb(sourcePtr))
    inheritDistributionAttrs(source, acquire.getOperation());

  DbCapture capture;
  capture.captured = captured;
  capture.ref = ref;
  capture.acquire = acquire;
  capture.bodyRefIndices.reserve(offsets.size());
  for (size_t i = 0; i < offsets.size(); ++i)
    capture.bodyRefIndices.push_back(createZeroIndex(builder, loc));
  return capture;
}

static void replaceUsesInMovedOp(Operation *op,
                                 const DenseMap<Value, Value> &mapping) {
  for (const auto &entry : mapping) {
    Value oldValue = entry.first;
    Value newValue = entry.second;
    if (!oldValue || !newValue || oldValue == newValue)
      continue;
    op->replaceUsesOfWith(oldValue, newValue);
    for (Region &region : op->getRegions())
      replaceAllUsesInRegionWith(oldValue, newValue, region);
  }
}

static LogicalResult outlineTail(TailWork &work) {
  pruneDeadExternalStackStores(work);
  if (work.tailOps.empty())
    return failure();
  if (failed(validateTailIsolation(work)))
    return failure();
  if (failed(classifyCaptures(work)))
    return failure();

  Location loc = work.frontier->getLoc();
  OpBuilder builder(work.tailOps.front());

  SmallVector<DbCapture> dbCaptures;
  SmallVector<Value> deps;
  for (Value capture : work.dbCaptures) {
    FailureOr<DbCapture> dbCapture =
        createDbCaptureAcquire(builder, loc, capture);
    if (failed(dbCapture))
      return failure();
    deps.push_back(dbCapture->acquire.getPtr());
    dbCaptures.push_back(*dbCapture);
  }

  SmallVector<Value> params(work.scalarParams.begin(), work.scalarParams.end());
  auto continuation =
      EdtOp::create(builder, loc, EdtType::task, EdtConcurrency::intranode,
                    ValueRange(deps), ValueRange(params));

  Block &body = continuation.getBody().front();
  for (Value dep : deps)
    body.addArgument(dep.getType(), loc);
  for (Value param : params)
    body.addArgument(param.getType(), loc);

  DenseMap<Value, Value> mapping;
  {
    OpBuilder::InsertionGuard guard(builder);
    builder.setInsertionPointToStart(&body);
    IRMapping rematMapping;
    for (Value capture : work.rematerializedCaptures) {
      Operation *def = capture.getDefiningOp();
      if (!def)
        return failure();
      Operation *cloned = builder.clone(*def, rematMapping);
      if (def->getNumResults() != cloned->getNumResults())
        return failure();
      for (auto [oldValue, newValue] :
           llvm::zip(def->getResults(), cloned->getResults())) {
        rematMapping.map(oldValue, newValue);
        mapping[oldValue] = newValue;
      }
    }
    for (auto [index, capture] : llvm::enumerate(dbCaptures)) {
      Value bodyDep = body.getArgument(index);
      Value payload =
          DbRefOp::create(builder, loc, bodyDep, capture.bodyRefIndices)
              .getResult();
      mapping[capture.captured] = payload;
    }
  }

  unsigned paramBase = deps.size();
  for (auto [index, param] : llvm::enumerate(params))
    mapping[param] = body.getArgument(paramBase + index);

  for (Operation *op : work.tailOps) {
    replaceUsesInMovedOp(op, mapping);
    op->moveBefore(&body, body.end());
  }

  builder.setInsertionPointToEnd(&body);
  ShutdownOp::create(builder, loc);
  YieldOp::create(builder, loc);
  ++numContinuationsAuthored;
  return success();
}

struct EpochTailContinuationPass
    : public ::impl::EpochTailContinuationBase<EpochTailContinuationPass> {
  void runOnOperation() override {
    ModuleOp module = getOperation();
    ARTS_INFO_HEADER(EpochTailContinuationPass);
    ARTS_DEBUG_REGION(module.dump(););

    SmallVector<func::FuncOp> funcs;
    module.walk([&](func::FuncOp func) { funcs.push_back(func); });

    for (func::FuncOp func : funcs) {
      TailWork work;
      if (failed(collectFinalTail(func, work)))
        continue;
      if (failed(exposeFinalNestedEpoch(func, work))) {
        ++numContinuationsSkipped;
        continue;
      }
      if (failed(outlineTail(work))) {
        ++numContinuationsSkipped;
        continue;
      }
    }

    ARTS_INFO_FOOTER(EpochTailContinuationPass);
    ARTS_DEBUG_REGION(module.dump(););
  }
};

} // namespace

namespace mlir {
namespace carts::arts {
std::unique_ptr<Pass> createEpochTailContinuationPass() {
  return std::make_unique<EpochTailContinuationPass>();
}
} // namespace carts::arts
} // namespace mlir
