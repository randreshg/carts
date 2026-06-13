///==========================================================================///
/// File: ScalarForwarding.cpp
///
/// Forward constant-initialized rank-0 memref allocas across region
/// boundaries that block standard mem2reg (omp.parallel, cu_region, etc.).
/// It also removes exact rank-0 scratch store/load pairs left by frontend
/// scalar temporaries, exposing the SSA value to SDE affine access analysis.
///
/// For each rank-0 memref.alloca initialized with a constant C:
///   (A) loads inside nested regions are replaced with C if all stores
///       inside that region also write C;
///   (B) same-scope loads are replaced with C if all stores before the
///       load (including those inside intervening region-holding ops)
///       also write C.
/// Subsequent canonicalize/DCE passes then fold the always-true/false
/// scf.if conditions and remove dead stores.
///==========================================================================///

#include "carts/dialect/sde/Transforms/Passes.h"
namespace mlir::carts::sde {
#define GEN_PASS_DEF_SCALARFORWARDING
#include "carts/dialect/sde/Transforms/Passes.h.inc"
} // namespace mlir::carts::sde

#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"

#include "carts/utils/Debug.h"
#include "carts/utils/ValueAnalysis.h"
ARTS_DEBUG_SETUP(scalar_forwarding);

using namespace mlir;
using namespace mlir::carts;

namespace {

/// Return the constant attribute if \p value is defined by arith.constant.
static Attribute getConstantAttr(Value value) {
  if (auto cst = value.getDefiningOp<arith::ConstantOp>())
    return cst.getValue();
  return {};
}

static bool isScalarValueType(Type type) {
  return type.isIndex() || isa<IntegerType, FloatType>(type);
}

static bool sameScalarValue(Value lhs, Value rhs) {
  if (lhs == rhs)
    return true;
  Attribute lhsAttr = getConstantAttr(lhs);
  Attribute rhsAttr = getConstantAttr(rhs);
  if (lhsAttr && rhsAttr && lhsAttr == rhsAttr)
    return true;
  return ValueAnalysis::sameValue(lhs, rhs);
}

static bool opIsOrIsNestedIn(Operation *scope, Operation *op) {
  return scope && op && (scope == op || scope->isAncestor(op));
}

static bool isRankZeroAllocaRoot(Value memref, Value alloca) {
  return ValueAnalysis::stripMemrefViewOps(memref) == alloca;
}

static bool getStoreToAlloca(Operation *op, Value alloca, Value &stored) {
  auto store = dyn_cast_or_null<memref::StoreOp>(op);
  if (!store || !isRankZeroAllocaRoot(store.getMemref(), alloca))
    return false;
  stored = store.getValueToStore();
  return true;
}

static bool isLoadFromAlloca(Operation *op, Value alloca) {
  auto load = dyn_cast<memref::LoadOp>(op);
  return load && isRankZeroAllocaRoot(load.getMemref(), alloca);
}

static bool valueIsUsableAfter(Operation *scope, Value value) {
  if (!value)
    return false;
  if (Operation *def = value.getDefiningOp())
    return !opIsOrIsNestedIn(scope, def);
  auto blockArg = dyn_cast<BlockArgument>(value);
  if (!blockArg)
    return true;
  Operation *owner =
      blockArg.getOwner() ? blockArg.getOwner()->getParentOp() : nullptr;
  return !opIsOrIsNestedIn(scope, owner);
}

static bool hasUnexpectedAllocaOperand(Operation *op, Value alloca) {
  for (Value operand : op->getOperands())
    if (isa<MemRefType>(operand.getType()) &&
        isRankZeroAllocaRoot(operand, alloca))
      return true;
  return false;
}

static FailureOr<Value> computeBlockExitState(Block &block,
                                              Operation *stopBefore,
                                              Value alloca, Value entryState,
                                              OpBuilder &builder);

static FailureOr<Value> computeRegionExitState(Region &region, Value alloca,
                                               Value entryState,
                                               OpBuilder &builder) {
  if (region.empty())
    return entryState;
  if (!region.hasOneBlock())
    return failure();
  return computeBlockExitState(region.front(), /*stopBefore=*/nullptr, alloca,
                               entryState, builder);
}

static FailureOr<Value> buildBranchJoinState(scf::IfOp ifOp, Value thenState,
                                             Value elseState,
                                             OpBuilder &builder) {
  auto makeUsableAfter = [&](Value value) -> FailureOr<Value> {
    if (valueIsUsableAfter(ifOp.getOperation(), value))
      return value;
    auto constant = value.getDefiningOp<arith::ConstantOp>();
    if (!constant)
      return failure();
    return arith::ConstantOp::create(builder, ifOp.getLoc(), constant.getType(),
                                     constant.getValue())
        .getResult();
  };

  if (sameScalarValue(thenState, elseState)) {
    FailureOr<Value> usable = makeUsableAfter(thenState);
    if (succeeded(usable))
      return usable;
    return makeUsableAfter(elseState);
  }

  if (thenState.getType() != elseState.getType() ||
      !isScalarValueType(thenState.getType()))
    return failure();
  FailureOr<Value> usableThen = makeUsableAfter(thenState);
  FailureOr<Value> usableElse = makeUsableAfter(elseState);
  if (failed(usableThen) || failed(usableElse))
    return failure();

  return arith::SelectOp::create(builder, ifOp.getLoc(), ifOp.getCondition(),
                                 *usableThen, *usableElse)
      .getResult();
}

static FailureOr<Value> computeBlockExitState(Block &block,
                                              Operation *stopBefore,
                                              Value alloca, Value entryState,
                                              OpBuilder &builder) {
  Value state = entryState;
  for (Operation &op : block) {
    if (&op == stopBefore)
      break;
    if (op.hasTrait<OpTrait::IsTerminator>())
      break;

    Value stored;
    if (getStoreToAlloca(&op, alloca, stored)) {
      state = stored;
      continue;
    }
    if (isLoadFromAlloca(&op, alloca))
      continue;

    if (auto ifOp = dyn_cast<scf::IfOp>(&op)) {
      FailureOr<Value> thenState =
          computeRegionExitState(ifOp.getThenRegion(), alloca, state, builder);
      if (failed(thenState))
        return failure();
      FailureOr<Value> elseState =
          ifOp.getElseRegion().empty()
              ? FailureOr<Value>(state)
              : computeRegionExitState(ifOp.getElseRegion(), alloca, state,
                                       builder);
      if (failed(elseState))
        return failure();
      FailureOr<Value> joined =
          buildBranchJoinState(ifOp, *thenState, *elseState, builder);
      if (failed(joined))
        return failure();
      state = *joined;
      continue;
    }

    if (auto forOp = dyn_cast<scf::ForOp>(&op)) {
      FailureOr<Value> bodyState = computeBlockExitState(
          *forOp.getBody(), /*stopBefore=*/nullptr, alloca, state, builder);
      if (failed(bodyState) || !sameScalarValue(*bodyState, state))
        return failure();
      continue;
    }

    bool regionsPreserveState = true;
    for (Region &region : op.getRegions()) {
      FailureOr<Value> regionState =
          computeRegionExitState(region, alloca, state, builder);
      if (failed(regionState) || !sameScalarValue(*regionState, state)) {
        regionsPreserveState = false;
        break;
      }
    }
    if (!regionsPreserveState || hasUnexpectedAllocaOperand(&op, alloca))
      return failure();
  }
  return state;
}

static void collectStructuredSameBlockLoads(
    Value allocaVal, memref::StoreOp initStore,
    SmallVectorImpl<std::pair<memref::LoadOp, Value>> &toForward) {
  Block *allocaBlock = initStore->getBlock();
  Value initValue = initStore.getValueToStore();
  for (Operation *user : allocaVal.getUsers()) {
    auto load = dyn_cast<memref::LoadOp>(user);
    if (!load || load->getBlock() != allocaBlock)
      continue;
    if (!initStore->isBeforeInBlock(load))
      continue;

    OpBuilder builder(load);
    FailureOr<Value> state = computeBlockExitState(
        *allocaBlock, load.getOperation(), allocaVal, initValue, builder);
    if (failed(state))
      continue;
    toForward.push_back({load, *state});
  }
}

static FailureOr<Value> findNearestStraightLineStore(memref::LoadOp load,
                                                     Value alloca) {
  for (Operation *cursor = load->getPrevNode(); cursor;
       cursor = cursor->getPrevNode()) {
    Value stored;
    if (getStoreToAlloca(cursor, alloca, stored))
      return stored;
    if (isLoadFromAlloca(cursor, alloca))
      continue;
    if (!cursor->getRegions().empty() ||
        hasUnexpectedAllocaOperand(cursor, alloca))
      return failure();
  }
  return failure();
}

static void collectImmediateScratchLoads(
    ModuleOp module,
    SmallVectorImpl<std::pair<memref::LoadOp, Value>> &toForward) {
  module.walk([&](memref::LoadOp load) {
    if (!isScalarValueType(load.getResult().getType()))
      return;

    Value root = ValueAnalysis::stripMemrefViewOps(load.getMemref());
    auto alloca = root ? root.getDefiningOp<memref::AllocaOp>() : nullptr;
    if (!alloca || alloca.getType().getRank() != 0)
      return;

    FailureOr<Value> stored = findNearestStraightLineStore(load, root);
    if (failed(stored))
      return;
    if ((*stored).getType() != load.getResult().getType())
      return;
    toForward.push_back({load, *stored});
  });
}

/// Check whether every memref.store to \p alloca inside \p region writes
/// a value whose constant attribute equals \p expected.
static bool allStoresInRegionMatch(Value alloca, Region &region,
                                   Attribute expected) {
  bool ok = true;
  region.walk([&](memref::StoreOp store) {
    if (!ok)
      return;
    if (store.getMemref() != alloca)
      return;
    Attribute attr = getConstantAttr(store.getValueToStore());
    if (!attr || attr != expected)
      ok = false;
  });
  return ok;
}

/// Find the outermost region between \p inner and \p allocaRegion.
/// Returns nullptr if \p inner IS \p allocaRegion.
static Region *getOutermostEnclosingRegion(Region *inner,
                                           Region *allocaRegion) {
  if (inner == allocaRegion)
    return nullptr;
  Region *prev = inner;
  while (prev->getParentRegion() != allocaRegion) {
    prev = prev->getParentRegion();
    if (!prev)
      return nullptr;
  }
  return prev;
}

struct ScalarForwardingPass
    : public sde::impl::ScalarForwardingBase<ScalarForwardingPass> {
  void runOnOperation() override {
    ModuleOp module = getOperation();
    unsigned forwarded = 0;

    SmallVector<std::pair<memref::LoadOp, Value>> immediateScratchLoads;
    collectImmediateScratchLoads(module, immediateScratchLoads);
    for (auto [load, value] : immediateScratchLoads) {
      load.getResult().replaceAllUsesWith(value);
      load.erase();
      ++forwarded;
    }

    module.walk([&](memref::AllocaOp alloca) {
      if (alloca.getType().getRank() != 0)
        return;

      Value allocaVal = alloca.getResult();
      Region *allocaRegion = alloca->getParentRegion();
      Block *allocaBlock = alloca->getBlock();

      // Find the last store in the alloca's own block that is before any
      // region-holding op and writes a constant.  We walk all users and
      // keep the store that (a) is in the same block, (b) is after the
      // alloca, and (c) is earliest — i.e. the initializing store.
      memref::StoreOp initStore = nullptr;
      for (Operation *user : allocaVal.getUsers()) {
        auto store = dyn_cast<memref::StoreOp>(user);
        if (!store || store->getBlock() != allocaBlock)
          continue;
        if (store->isBeforeInBlock(alloca))
          continue;
        if (!initStore || store->isBeforeInBlock(initStore))
          initStore = store;
      }

      if (!initStore)
        return;

      Attribute initAttr = getConstantAttr(initStore.getValueToStore());
      if (!initAttr)
        return;

      SmallVector<std::pair<memref::LoadOp, Value>> structuredLoads;
      collectStructuredSameBlockLoads(allocaVal, initStore, structuredLoads);
      for (auto [load, value] : structuredLoads) {
        ARTS_DEBUG("forwarding structured scalar to load: " << *load);
        load.getResult().replaceAllUsesWith(value);
        load.erase();
        ++forwarded;
      }

      // For each remaining load, check if forwarding is safe.  Handles two
      // cases: (A) Loads inside nested regions (omp.parallel, cu_region, etc.)
      // (B) Same-scope loads where intervening region ops block mem2reg
      SmallVector<memref::LoadOp> toForward;
      for (Operation *user : allocaVal.getUsers()) {
        auto load = dyn_cast<memref::LoadOp>(user);
        if (!load)
          continue;

        Region *loadRegion = load->getParentRegion();

        // Case B: same-scope load blocked by intervening region ops.
        if (loadRegion == allocaRegion) {
          if (load->getBlock() != allocaBlock)
            continue;
          if (!initStore->isBeforeInBlock(load))
            continue;

          // All same-block stores before the load must write initAttr.
          bool allMatch = true;
          for (Operation *u : allocaVal.getUsers()) {
            auto st = dyn_cast<memref::StoreOp>(u);
            if (!st || st->getBlock() != allocaBlock)
              continue;
            if (!st->isBeforeInBlock(load))
              continue;
            Attribute a = getConstantAttr(st.getValueToStore());
            if (!a || a != initAttr) {
              allMatch = false;
              break;
            }
          }
          if (!allMatch)
            continue;

          // All stores inside any region-holding op between init and load
          // must also write initAttr.
          bool regionsOk = true;
          for (Operation &op : *allocaBlock) {
            if (!initStore->isBeforeInBlock(&op))
              continue;
            if (!op.isBeforeInBlock(load))
              continue;
            for (Region &r : op.getRegions()) {
              if (!allStoresInRegionMatch(allocaVal, r, initAttr)) {
                regionsOk = false;
                break;
              }
            }
            if (!regionsOk)
              break;
          }
          if (!regionsOk)
            continue;

          toForward.push_back(load);
          continue;
        }

        // Case A: load inside a nested region.
        Region *outerRegion =
            getOutermostEnclosingRegion(loadRegion, allocaRegion);
        if (!outerRegion)
          continue;

        Operation *regionOp = outerRegion->getParentOp();
        if (regionOp->getBlock() != allocaBlock)
          continue;

        // The init store must be before the region entry.
        if (!initStore->isBeforeInBlock(regionOp))
          continue;

        // Check: no store between initStore and regionOp writes a
        // different constant.  We find the LAST store before regionOp
        // and verify it writes the same constant.
        memref::StoreOp lastBefore = nullptr;
        for (Operation *u : allocaVal.getUsers()) {
          auto st = dyn_cast<memref::StoreOp>(u);
          if (!st || st->getBlock() != allocaBlock)
            continue;
          if (!st->isBeforeInBlock(regionOp))
            continue;
          if (!lastBefore || lastBefore->isBeforeInBlock(st))
            lastBefore = st;
        }
        if (lastBefore) {
          Attribute lastAttr = getConstantAttr(lastBefore.getValueToStore());
          if (!lastAttr || lastAttr != initAttr)
            continue;
        }

        // All stores inside the region must write the same constant.
        if (!allStoresInRegionMatch(allocaVal, *outerRegion, initAttr))
          continue;

        toForward.push_back(load);
      }

      if (toForward.empty())
        return;

      Value constVal = initStore.getValueToStore();
      for (memref::LoadOp load : toForward) {
        ARTS_DEBUG("forwarding constant to load: " << *load);
        load.getResult().replaceAllUsesWith(constVal);
        load.erase();
        ++forwarded;
      }
    });

    ARTS_INFO("ScalarForwarding: forwarded " << forwarded << " load(s)");
  }
};

} // namespace

namespace mlir::carts::sde {

std::unique_ptr<Pass> createScalarForwardingPass() {
  return std::make_unique<ScalarForwardingPass>();
}

} // namespace mlir::carts::sde
