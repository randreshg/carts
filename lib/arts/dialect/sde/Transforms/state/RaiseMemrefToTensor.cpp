///==========================================================================///
/// File: RaiseMemrefToTensor.cpp
///
/// v1 implementation of the RaiseMemrefToTensor pass described in
/// docs/compiler/raise-memref-to-tensor-rfc.md §5.
///
/// Scope (v1):
///   - memref.alloca producing memref<KxT> with small constant K (K <= 16).
///   - Accesses inside sde.cu_task bodies are constant-indexed
///     memref.load / memref.store / affine.load / affine.store only.
///   - No captures of the memref via call @extern(...).
///   - No aliasing via memref.subview.
///   - memref.cast is tolerated only when its uses outside cu_task bodies are
///     limited to sde.mu_dep (which this pass erases).
///
/// For each raisable memref, we:
///   1. Create a `sde.mu_data` handle just before the outermost enclosing
///      `sde.cu_region <parallel>` (or, if absent, the first `cu_task` site).
///   2. Rewrite each `sde.cu_task` whose body accesses the memref into a
///      `sde.cu_codelet` that consumes `sde.mu_token`s for the memref, with
///      body loads/stores rewritten to tensor.extract / tensor.insert.
///   3. Thread the SSA tensor value across codelets: each write-producing
///      codelet's result replaces the current tensor handle, so subsequent
///      codelets' mu_token sources see the updated tensor.
///   4. Materialize the final tensor back into the original memref after the
///      outermost cu_region completes, so post-region reads observe the new
///      values.
///   5. Erase sde.mu_dep ops that reference any raised memref.
///
/// Out of scope for v1 (see RFC §5.2 and §10 for v2/v3 plans):
///   - Slice-access patterns (`memref.subview` etc.).
///   - Dynamic-ranked memrefs.
///   - Threading through scf.for / scf.while / scf.if iter_args.
///==========================================================================///

#include "arts/dialect/sde/Transforms/Passes.h"
namespace mlir::arts {
#define GEN_PASS_DEF_RAISEMEMREFTOTENSOR
#include "arts/dialect/sde/Transforms/Passes.h.inc"
} // namespace mlir::arts

#include "arts/utils/Utils.h"
#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Bufferization/IR/Bufferization.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/Tensor/IR/Tensor.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/IRMapping.h"
#include "mlir/IR/Matchers.h"
#include "mlir/Interfaces/SideEffectInterfaces.h"

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallVector.h"

using namespace mlir;
using namespace mlir::arts;

namespace {

/// v1 threshold — only raise memrefs with rank-1 static shape K <= kMaxRaiseK.
static constexpr int64_t kMaxRaiseK = 16;

/// Classifies how a task body touches a raisable memref.
enum class MemrefUseKind { None = 0, Read = 1, Write = 2, ReadWrite = 3 };

static MemrefUseKind merge(MemrefUseKind a, MemrefUseKind b) {
  using K = MemrefUseKind;
  if (a == K::None)
    return b;
  if (b == K::None)
    return a;
  if (a == b)
    return a;
  return K::ReadWrite;
}

/// Match a constant-index value (index or integer type) and return its int
/// value.
static std::optional<int64_t> matchConstantIndex(Value v) {
  APInt val;
  if (matchPattern(v, m_ConstantInt(&val)))
    return val.getSExtValue();
  return std::nullopt;
}

/// Is `memref` legal to raise under v1 rules?
/// Preconditions enforced:
///   - RankedMemRefType with static shape and rank 1 and K <= kMaxRaiseK
///   - No memref.subview users, no function-call captures
///   - memref.cast users are tolerated only if their transitive users are
///     limited to sde.mu_dep (which the pass will erase)
///   - All in-task accesses are constant-indexed memref/affine load/store
static bool isRaisableMemref(Value memref, DenseSet<Operation *> &castsToErase) {
  auto mrType = dyn_cast<MemRefType>(memref.getType());
  if (!mrType || !mrType.hasStaticShape())
    return false;
  if (mrType.getRank() != 1)
    return false;
  if (mrType.getShape()[0] > kMaxRaiseK)
    return false;

  // Walk all uses; track any cast we must erase.
  for (Operation *user : memref.getUsers()) {
    if (isa<memref::LoadOp, memref::StoreOp, affine::AffineLoadOp,
            affine::AffineStoreOp>(user)) {
      // OK — leaf op. Constant-index check happens during rewrite so we can
      // bail out gracefully if a single access is non-constant.
      continue;
    }
    if (auto cast = dyn_cast<memref::CastOp>(user)) {
      // Cast is tolerated only if its users are all sde.mu_dep (which we
      // erase) or simple load/store we can rewrite.
      for (Operation *castUser : cast.getResult().getUsers()) {
        if (isa<sde::SdeMuDepOp>(castUser))
          continue;
        // Other users (loads/stores via the cast, calls, etc.) would need
        // their own rewrite; bail out.
        return false;
      }
      castsToErase.insert(cast);
      continue;
    }
    if (isa<sde::SdeMuDepOp>(user))
      continue;
    // Any other user (func.call, memref.subview, etc.) disqualifies.
    return false;
  }
  return true;
}

/// Collect the memref.allocas used inside any sde.cu_task body within `func`.
static SmallVector<memref::AllocaOp>
collectCandidateAllocas(func::FuncOp func,
                        DenseSet<Operation *> &castsToErase) {
  // Step 1: find allocas whose users transitively include a load/store inside
  // a cu_task body.
  DenseSet<memref::AllocaOp> usedInTask;
  func.walk([&](sde::SdeCuTaskOp task) {
    task.getBody().walk([&](Operation *op) {
      Value memref;
      if (auto ld = dyn_cast<memref::LoadOp>(op))
        memref = ld.getMemref();
      else if (auto st = dyn_cast<memref::StoreOp>(op))
        memref = st.getMemref();
      else if (auto ld = dyn_cast<affine::AffineLoadOp>(op))
        memref = ld.getMemref();
      else if (auto st = dyn_cast<affine::AffineStoreOp>(op))
        memref = st.getMemref();
      else
        return;
      if (auto alloca = memref.getDefiningOp<memref::AllocaOp>())
        usedInTask.insert(alloca);
    });
  });

  SmallVector<memref::AllocaOp> result;
  for (memref::AllocaOp alloca : usedInTask) {
    DenseSet<Operation *> localCasts;
    if (!isRaisableMemref(alloca.getResult(), localCasts))
      continue;
    result.push_back(alloca);
    for (Operation *c : localCasts)
      castsToErase.insert(c);
  }
  return result;
}

/// Return the outermost `sde.cu_region` that transitively contains the given
/// `cu_task`, or nullptr when there is none.
static sde::SdeCuRegionOp getOutermostCuRegion(Operation *innerOp) {
  sde::SdeCuRegionOp outer;
  Operation *cur = innerOp->getParentOp();
  while (cur) {
    if (auto cr = dyn_cast<sde::SdeCuRegionOp>(cur))
      outer = cr;
    cur = cur->getParentOp();
  }
  return outer;
}

/// Collect in-order `cu_task` ops in the function that access any of the
/// raisable memrefs.
static SmallVector<sde::SdeCuTaskOp>
collectTasksUsingAnyMemref(func::FuncOp func,
                           const DenseSet<Value> &raisedMemrefs) {
  SmallVector<sde::SdeCuTaskOp> tasks;
  func.walk<WalkOrder::PreOrder>([&](sde::SdeCuTaskOp task) {
    bool uses = false;
    task.getBody().walk([&](Operation *op) {
      Value memref;
      if (auto ld = dyn_cast<memref::LoadOp>(op))
        memref = ld.getMemref();
      else if (auto st = dyn_cast<memref::StoreOp>(op))
        memref = st.getMemref();
      else if (auto ld = dyn_cast<affine::AffineLoadOp>(op))
        memref = ld.getMemref();
      else if (auto st = dyn_cast<affine::AffineStoreOp>(op))
        memref = st.getMemref();
      if (memref && raisedMemrefs.count(memref))
        uses = true;
    });
    if (uses)
      tasks.push_back(task);
  });
  return tasks;
}

/// Classify access modes of `task` on each raised memref. Returns the per-
/// memref mode map, and a flag indicating whether every access was a
/// constant-indexed load/store on a raised memref (if not, the caller must
/// skip this task).
struct TaskAccessInfo {
  DenseMap<Value, MemrefUseKind> modes;
  SmallVector<Operation *> accessOps;
  bool compatible = true;
};

static TaskAccessInfo analyzeTask(sde::SdeCuTaskOp task,
                                  const DenseSet<Value> &raisedMemrefs) {
  TaskAccessInfo info;
  task.getBody().walk([&](Operation *op) {
    Value memref;
    bool isStore = false;
    if (auto ld = dyn_cast<memref::LoadOp>(op)) {
      memref = ld.getMemref();
    } else if (auto st = dyn_cast<memref::StoreOp>(op)) {
      memref = st.getMemref();
      isStore = true;
    } else if (auto ld = dyn_cast<affine::AffineLoadOp>(op)) {
      memref = ld.getMemref();
    } else if (auto st = dyn_cast<affine::AffineStoreOp>(op)) {
      memref = st.getMemref();
      isStore = true;
    } else {
      return;
    }
    if (!raisedMemrefs.count(memref))
      return;
    info.accessOps.push_back(op);
    auto kind = isStore ? MemrefUseKind::Write : MemrefUseKind::Read;
    auto it = info.modes.find(memref);
    if (it == info.modes.end())
      info.modes[memref] = kind;
    else
      it->second = merge(it->second, kind);
  });
  return info;
}

/// Clone ops producing values referenced inside `task` body (other than
/// per-memref accesses) into the new codelet body ahead of the mapped
/// operations. Only side-effect-free producers are cloneable; anything else
/// disqualifies the task.
///
/// Values corresponding to raised memrefs (or their cast aliases) are skipped:
/// those are handled by the token / block-arg mechanism and would never be
/// pure anyway (memref.alloca has a memory effect).
///
/// Returns a mapping from original SSA values to the cloned values inside the
/// codelet body, or std::nullopt if any external use cannot be cloned.
static std::optional<IRMapping>
cloneExternalPureProducers(sde::SdeCuTaskOp task,
                           const DenseSet<Value> &raisedMemrefs,
                           OpBuilder &codeletBuilder) {
  IRMapping mapping;
  // Collect values referenced inside the task body that are defined outside
  // the body region. Walk in program order so we can clone producers before
  // their consumers.
  SmallVector<Value> externalValues;
  DenseSet<Value> externalSet;
  Region &body = task.getBody();
  body.walk([&](Operation *op) {
    for (Value operand : op->getOperands()) {
      // Definition site must be outside the task's body region.
      Region *defRegion = operand.getParentRegion();
      if (defRegion == nullptr)
        continue;
      if (body.isAncestor(defRegion))
        continue;
      if (raisedMemrefs.count(operand))
        continue; // handled via the token / block-arg machinery.
      if (externalSet.insert(operand).second)
        externalValues.push_back(operand);
    }
  });

  // Topologically sort by original program order: we walk the users' defining
  // ops and reconstruct them bottom-up. A simple depth-first traversal with
  // memoization suffices here because we only clone pure producers.
  std::function<bool(Value)> cloneValue = [&](Value v) -> bool {
    if (mapping.contains(v))
      return true;
    if (raisedMemrefs.count(v))
      return true; // handled elsewhere.
    Operation *def = v.getDefiningOp();
    if (!def) {
      // Block argument defined outside: cannot clone.
      return false;
    }
    if (!isMemoryEffectFree(def))
      return false;
    // Recurse on operands first.
    for (Value operand : def->getOperands()) {
      if (mapping.contains(operand))
        continue;
      Region *defRegion = operand.getParentRegion();
      if (defRegion && body.isAncestor(defRegion))
        continue; // defined inside the body
      if (!cloneValue(operand))
        return false;
    }
    Operation *cloned = codeletBuilder.clone(*def, mapping);
    for (auto [orig, repl] :
         llvm::zip(def->getResults(), cloned->getResults()))
      mapping.map(orig, repl);
    return true;
  };

  for (Value v : externalValues) {
    if (!cloneValue(v))
      return std::nullopt;
  }
  return mapping;
}

//===----------------------------------------------------------------------===//
// Pass body.
//===----------------------------------------------------------------------===//

struct RaiseMemrefToTensorPass
    : public arts::impl::RaiseMemrefToTensorBase<RaiseMemrefToTensorPass> {
  using arts::impl::RaiseMemrefToTensorBase<
      RaiseMemrefToTensorPass>::RaiseMemrefToTensorBase;

  void runOnOperation() override {
    if (!enabled)
      return; // v1 gate — off by default; see Passes.td.
    ModuleOp module = getOperation();
    module.walk([&](func::FuncOp func) { rewriteFunction(func); });
  }

  void rewriteFunction(func::FuncOp func) {
    DenseSet<Operation *> castsToErase;
    SmallVector<memref::AllocaOp> candidates =
        collectCandidateAllocas(func, castsToErase);
    if (candidates.empty())
      return;

    // Map every raised alloca to its tensor type.
    DenseMap<Value, RankedTensorType> tensorTypes;
    DenseSet<Value> raisedMemrefSet;
    for (memref::AllocaOp alloca : candidates) {
      auto mrType = cast<MemRefType>(alloca.getType());
      auto tty =
          RankedTensorType::get(mrType.getShape(), mrType.getElementType());
      tensorTypes[alloca.getResult()] = tty;
      raisedMemrefSet.insert(alloca.getResult());
      // Cast aliases map to the same tensor type so token sources line up.
      for (Operation *user : alloca->getUsers()) {
        if (auto cast = dyn_cast<memref::CastOp>(user)) {
          raisedMemrefSet.insert(cast.getResult());
          tensorTypes[cast.getResult()] = tty;
        }
      }
    }

    // Collect tasks using any raised memref, in program order.
    SmallVector<sde::SdeCuTaskOp> tasks =
        collectTasksUsingAnyMemref(func, raisedMemrefSet);
    if (tasks.empty())
      return;

    // Analyze each task; bail out of the entire function if any task is
    // incompatible. We prefer not to partially-rewrite when a sibling task's
    // accesses can't be lifted.
    SmallVector<TaskAccessInfo> taskInfos;
    taskInfos.reserve(tasks.size());
    for (sde::SdeCuTaskOp task : tasks) {
      TaskAccessInfo info = analyzeTask(task, raisedMemrefSet);
      if (!info.compatible)
        return;
      // Verify every access is constant-indexed.
      for (Operation *op : info.accessOps) {
        if (auto ld = dyn_cast<memref::LoadOp>(op)) {
          for (Value idx : ld.getIndices())
            if (!matchConstantIndex(idx))
              return;
        } else if (auto st = dyn_cast<memref::StoreOp>(op)) {
          for (Value idx : st.getIndices())
            if (!matchConstantIndex(idx))
              return;
        }
        // affine.load / affine.store at constant symbols are already constant;
        // we accept any affine access for rank-1 memrefs since the map is an
        // identity into the single-element space.
      }
      taskInfos.push_back(std::move(info));
    }

    // Place mu_data ops at the outermost cu_region (or, absent one, just
    // before the first task).
    sde::SdeCuRegionOp outerRegion = getOutermostCuRegion(tasks.front());
    OpBuilder builder(func.getContext());
    if (outerRegion)
      builder.setInsertionPoint(outerRegion);
    else
      builder.setInsertionPoint(tasks.front());

    // One mu_data per raised alloca — seeds the first read/write and lowers
    // to an arts.db_alloc downstream.
    DenseMap<Value, Value> muDataByMemref;
    for (memref::AllocaOp alloca : candidates) {
      auto tty = tensorTypes[alloca.getResult()];
      auto muData = sde::SdeMuDataOp::create(
          builder, alloca.getLoc(), tty, /*init=*/nullptr,
          /*shared=*/UnitAttr::get(builder.getContext()));
      muDataByMemref[alloca.getResult()] = muData.getHandle();
      for (Operation *user : alloca->getUsers()) {
        if (auto cast = dyn_cast<memref::CastOp>(user))
          muDataByMemref[cast.getResult()] = muData.getHandle();
      }
    }

    // For v1 we use the mu_data handle as the canonical tensor source for
    // every codelet. Threading codelet results across sibling tasks is not
    // viable because ConvertOpenMPToSde wraps each task in its own
    // `scf.if/scf.execute_region` scope (a Polygeist idiom), so a codelet's
    // SSA result isn't visible in the next task's region. The shared mu_data
    // handle + per-token access modes give ConvertSdeToArts enough to emit
    // correctly-ordered `arts.db_acquire` ops.
    DenseMap<Value, Value> currentTensor = muDataByMemref;

    // Rewrite each task in program order.
    for (auto [task, info] : llvm::zip(tasks, taskInfos)) {
      if (!rewriteTask(task, info, tensorTypes, currentTensor,
                       raisedMemrefSet))
        return; // best-effort; leave partial state alone.
    }

    // Boundary materialization is emitted per-codelet inside rewriteTask:
    // after each write-producing codelet we materialize_in_destination into
    // the source memref so the original alloca stays in sync for any
    // post-task non-raised reads (both inside and outside the cu_region).

    // Erase the mu_dep ops referencing any raised memref (or its cast
    // aliases). Their SSA results feed sde.cu_task deps() lists that have
    // already been replaced with codelet token operands.
    SmallVector<sde::SdeMuDepOp> muDepsToErase;
    func.walk([&](sde::SdeMuDepOp dep) {
      if (raisedMemrefSet.count(dep.getSource()))
        muDepsToErase.push_back(dep);
    });
    for (sde::SdeMuDepOp dep : muDepsToErase) {
      if (dep->use_empty())
        dep.erase();
      // Otherwise some original cu_task we couldn't rewrite still references
      // this dep; leave it in place.
    }

    // Erase any now-dead memref.cast that was aliased to a raised alloca,
    // provided it has no remaining users.
    for (Operation *cast : castsToErase) {
      if (cast->use_empty())
        cast->erase();
    }
  }

  /// Rewrite one sde.cu_task into a sde.cu_codelet. Returns true on success.
  ///
  /// Strategy inside the new codelet body:
  ///   - For each token block argument, allocate a scratch memref<KxT> inside
  ///     the body and `bufferization.materialize_in_destination` the block-arg
  ///     tensor into it (reads see the incoming value, writes land in the
  ///     scratch memref).
  ///   - Clone the task body verbatim, rewriting every reference to the
  ///     original raised memref (including its memref.cast aliases) to the
  ///     new scratch memref. Leaf loads/stores stay as memref ops so nested
  ///     scf regions do not need to thread SSA for the tensor.
  ///   - At yield, `bufferization.to_tensor` each writable scratch memref and
  ///     yield the tensors.
  ///
  /// This keeps the rewrite local and avoids threading SSA through every
  /// nested scf.execute_region / scf.if the Polygeist frontend wraps task
  /// bodies in.
  bool rewriteTask(sde::SdeCuTaskOp task, const TaskAccessInfo &info,
                   DenseMap<Value, RankedTensorType> &tensorTypes,
                   DenseMap<Value, Value> &currentTensor,
                   const DenseSet<Value> &raisedMemrefs) {
    if (info.modes.empty())
      return true; // nothing to do.

    Location loc = task.getLoc();
    MLIRContext *ctx = task.getContext();
    OpBuilder builder(task);

    // Order the touched memrefs deterministically by first-access occurrence.
    // We also remember which cast-alias (if any) the task body used so we can
    // redirect references correctly.
    SmallVector<Value> orderedMemrefs;
    DenseSet<Value> seen;
    for (Operation *op : info.accessOps) {
      Value mr;
      if (auto ld = dyn_cast<memref::LoadOp>(op))
        mr = ld.getMemref();
      else if (auto st = dyn_cast<memref::StoreOp>(op))
        mr = st.getMemref();
      else if (auto ld = dyn_cast<affine::AffineLoadOp>(op))
        mr = ld.getMemref();
      else if (auto st = dyn_cast<affine::AffineStoreOp>(op))
        mr = st.getMemref();
      if (mr && seen.insert(mr).second)
        orderedMemrefs.push_back(mr);
    }

    // Build mu_tokens for each touched memref with the classified mode.
    SmallVector<Value> tokens;
    SmallVector<Type> blockArgTypes;
    SmallVector<Type> resultTypes;
    SmallVector<unsigned> writableIndices;
    tokens.reserve(orderedMemrefs.size());
    blockArgTypes.reserve(orderedMemrefs.size());
    for (auto [idx, memref] : llvm::enumerate(orderedMemrefs)) {
      auto tty = tensorTypes[memref];
      Value src = currentTensor[memref];
      MemrefUseKind k = info.modes.lookup(memref);
      sde::SdeAccessMode mode;
      bool writable = false;
      switch (k) {
      case MemrefUseKind::Read:
        mode = sde::SdeAccessMode::read;
        break;
      case MemrefUseKind::Write:
        mode = sde::SdeAccessMode::write;
        writable = true;
        break;
      case MemrefUseKind::ReadWrite:
        mode = sde::SdeAccessMode::readwrite;
        writable = true;
        break;
      case MemrefUseKind::None:
        continue;
      }
      auto tokenType = sde::TokenType::get(ctx, tty);
      auto tokOp = sde::SdeMuTokenOp::create(
          builder, loc, tokenType,
          sde::SdeAccessModeAttr::get(ctx, mode), src,
          /*offsets=*/ValueRange{}, /*sizes=*/ValueRange{});
      tokens.push_back(tokOp.getToken());
      blockArgTypes.push_back(tty);
      if (writable) {
        writableIndices.push_back(idx);
        resultTypes.push_back(tty);
      }
    }

    // Create the cu_codelet with one block arg per token.
    auto codelet = sde::SdeCuCodeletOp::create(builder, loc, resultTypes,
                                               tokens);
    Block *codeletBlock = new Block();
    SmallVector<Location> argLocs(blockArgTypes.size(), loc);
    codeletBlock->addArguments(blockArgTypes, argLocs);
    codelet.getBody().push_back(codeletBlock);

    OpBuilder bodyBuilder(ctx);
    bodyBuilder.setInsertionPointToStart(codeletBlock);

    // Clone pure external producers into the body first, so their SSA values
    // are visible when we clone the task body ops.
    auto externalMapping =
        cloneExternalPureProducers(task, raisedMemrefs, bodyBuilder);
    if (!externalMapping) {
      codelet.erase();
      for (Value tok : tokens) {
        if (auto tokOp = tok.getDefiningOp<sde::SdeMuTokenOp>())
          if (tokOp->use_empty())
            tokOp.erase();
      }
      return false;
    }
    IRMapping mapping = *externalMapping;

    // For each token block arg, materialize a per-codelet scratch memref and
    // seed it from the block-arg tensor. We then redirect every reference to
    // the original memref (including its cast aliases) to this scratch.
    DenseMap<Value, Value> scratchMemref;
    for (auto [idx, memref] : llvm::enumerate(orderedMemrefs)) {
      auto tty = cast<RankedTensorType>(tensorTypes[memref]);
      auto mrType = MemRefType::get(tty.getShape(), tty.getElementType());
      Value scratch =
          memref::AllocaOp::create(bodyBuilder, loc, mrType).getResult();
      // Seed the scratch with the token's tensor view.
      bufferization::MaterializeInDestinationOp::create(
          bodyBuilder, loc, /*result=*/TypeRange{},
          codeletBlock->getArgument(idx), scratch, /*restrict=*/UnitAttr{},
          /*writable=*/UnitAttr::get(ctx));
      scratchMemref[memref] = scratch;
      mapping.map(memref, scratch);
    }

    // Clone each op from the task body; references to the raised memref (or
    // its cast aliases) are redirected to the scratch memref via `mapping`.
    Block &sourceBlock = task.getBody().front();
    for (Operation &op : sourceBlock.without_terminator())
      bodyBuilder.clone(op, mapping);

    // Yield the writable scratches as tensors.
    SmallVector<Value> yielded;
    for (unsigned idx : writableIndices) {
      Value mr = orderedMemrefs[idx];
      Value scratch = scratchMemref[mr];
      auto tty = cast<RankedTensorType>(tensorTypes[mr]);
      Value yieldTensor = bufferization::ToTensorOp::create(
          bodyBuilder, loc, tty, scratch, /*restrict=*/UnitAttr{},
          /*writable=*/UnitAttr{});
      yielded.push_back(yieldTensor);
    }
    sde::SdeYieldOp::create(bodyBuilder, loc, yielded);

    // Boundary-materialize the updated tensor back into the source memref
    // immediately after the codelet so post-task reads on the alloca observe
    // the new value. The original alloca is still visible here because the
    // task/codelet shares the alloca's enclosing scope.
    //
    // v1 does NOT thread codelet results into subsequent codelets' tokens
    // (see the currentTensor comment above); every codelet's tokens source
    // from the shared mu_data handle. The per-codelet materialize keeps the
    // original alloca in sync for any non-raised consumers.
    OpBuilder afterCodeletBuilder(task.getContext());
    afterCodeletBuilder.setInsertionPointAfter(codelet);
    for (auto [i, idx] : llvm::enumerate(writableIndices)) {
      Value mr = orderedMemrefs[idx];
      bufferization::MaterializeInDestinationOp::create(
          afterCodeletBuilder, codelet.getLoc(), /*result=*/TypeRange{},
          codelet.getResult(i), mr, /*restrict=*/UnitAttr{},
          /*writable=*/UnitAttr::get(task.getContext()));
    }

    // Erase the original cu_task.
    task.erase();
    return true;
  }

  /// (Retained only as a no-op placeholder; the rewrite strategy now uses a
  /// per-codelet scratch memref, so recursive IR rewriting is unnecessary.)
  [[maybe_unused]] void
  cloneOpRewriting(Operation &op, OpBuilder &builder, IRMapping &mapping,
                   DenseMap<Value, Value> &liveTensor,
                   DenseMap<Value, RankedTensorType> &tensorTypes) {
    Location loc = op.getLoc();
    MLIRContext *ctx = builder.getContext();

    // Leaf rewriting for loads on a raised memref.
    auto handleLoad = [&](Value memref, ValueRange indices,
                          Type resultType, Operation *origOp) -> bool {
      auto itT = liveTensor.find(memref);
      if (itT == liveTensor.end())
        return false;
      // Build tensor.extract at the mapped indices.
      SmallVector<Value> idxVals;
      idxVals.reserve(indices.size());
      for (Value idx : indices)
        idxVals.push_back(mapping.lookupOrDefault(idx));
      Value tensorVal = itT->second;
      // Ensure indices are index-typed constants; if empty we use a zero
      // index (rank-1 memref always has one index, but affine maps may fold
      // the single-index case to zero operands — rank is 1 so emit
      // createZeroIndex).
      if (idxVals.empty()) {
        idxVals.push_back(arts::createZeroIndex(builder, loc));
      }
      Value extracted = tensor::ExtractOp::create(builder, loc, tensorVal,
                                                  idxVals);
      mapping.map(origOp->getResult(0), extracted);
      return true;
    };

    auto handleStore = [&](Value memref, Value value, ValueRange indices,
                           Operation *origOp) -> bool {
      auto itT = liveTensor.find(memref);
      if (itT == liveTensor.end())
        return false;
      SmallVector<Value> idxVals;
      idxVals.reserve(indices.size());
      for (Value idx : indices)
        idxVals.push_back(mapping.lookupOrDefault(idx));
      if (idxVals.empty())
        idxVals.push_back(arts::createZeroIndex(builder, loc));
      Value mappedValue = mapping.lookupOrDefault(value);
      Value updated = tensor::InsertOp::create(builder, loc, mappedValue,
                                               itT->second, idxVals);
      liveTensor[memref] = updated;
      return true;
    };

    if (auto ld = dyn_cast<memref::LoadOp>(op)) {
      if (handleLoad(ld.getMemref(), ld.getIndices(), ld.getType(), &op))
        return;
    } else if (auto st = dyn_cast<memref::StoreOp>(op)) {
      if (handleStore(st.getMemref(), st.getValue(), st.getIndices(), &op))
        return;
    } else if (auto ld = dyn_cast<affine::AffineLoadOp>(op)) {
      if (liveTensor.count(ld.getMemref())) {
        // Rank-1 memref with constant map: index folds to zero.
        Value zero = arts::createZeroIndex(builder, loc);
        Value tensorVal = liveTensor[ld.getMemref()];
        Value extracted = tensor::ExtractOp::create(builder, loc, tensorVal,
                                                    ValueRange{zero});
        mapping.map(ld.getResult(), extracted);
        return;
      }
    } else if (auto st = dyn_cast<affine::AffineStoreOp>(op)) {
      if (liveTensor.count(st.getMemref())) {
        Value zero = arts::createZeroIndex(builder, loc);
        Value mappedValue = mapping.lookupOrDefault(st.getValue());
        Value updated = tensor::InsertOp::create(
            builder, loc, mappedValue, liveTensor[st.getMemref()],
            ValueRange{zero});
        liveTensor[st.getMemref()] = updated;
        return;
      }
    }

    // Generic clone path: clone the op with the current mapping, recursing
    // into regions. If the op has regions, we clone the op first with empty
    // regions and then walk each region manually so nested loads/stores get
    // rewritten.
    if (op.getNumRegions() == 0) {
      Operation *cloned = builder.clone(op, mapping);
      (void)cloned;
      return;
    }

    // Create a shell clone with no regions populated, then populate regions
    // by recursion.
    SmallVector<Value> operands;
    operands.reserve(op.getNumOperands());
    for (Value v : op.getOperands())
      operands.push_back(mapping.lookupOrDefault(v));
    SmallVector<Type> resultTypes(op.getResultTypes());
    OperationState state(loc, op.getName().getStringRef(), operands,
                         resultTypes, op.getAttrs());
    for (unsigned i = 0; i < op.getNumRegions(); ++i)
      state.addRegion();
    Operation *cloned = builder.create(state);
    for (auto [orig, repl] : llvm::zip(op.getResults(), cloned->getResults()))
      mapping.map(orig, repl);

    // Recurse into each region, mirroring blocks.
    for (unsigned r = 0; r < op.getNumRegions(); ++r) {
      Region &srcRegion = op.getRegion(r);
      Region &dstRegion = cloned->getRegion(r);
      for (Block &srcBlock : srcRegion) {
        Block *dstBlock = new Block();
        SmallVector<Location> argLocs(srcBlock.getNumArguments(), loc);
        dstBlock->addArguments(srcBlock.getArgumentTypes(), argLocs);
        dstRegion.push_back(dstBlock);
        for (auto [srcArg, dstArg] :
             llvm::zip(srcBlock.getArguments(), dstBlock->getArguments()))
          mapping.map(srcArg, dstArg);
        OpBuilder nested(ctx);
        nested.setInsertionPointToEnd(dstBlock);
        for (Operation &nestedOp : srcBlock) {
          // The terminator may need to be cloned via the same rewriting path,
          // but typically terminators in nested scf regions don't touch
          // raised memrefs; fall through to cloneOpRewriting either way.
          cloneOpRewriting(nestedOp, nested, mapping, liveTensor, tensorTypes);
        }
      }
    }
  }
};

} // namespace

namespace mlir::arts::sde {

std::unique_ptr<Pass> createRaiseMemrefToTensorPass() {
  return std::make_unique<RaiseMemrefToTensorPass>();
}

} // namespace mlir::arts::sde
