///==========================================================================///
/// File: DbOpts.cpp
///
/// DB-local cleanup optimizations that run after DbPass / DbPartitioning have
/// established stable acquire and dependency structure.
///==========================================================================///

#include "../ArtsPassDetails.h"
#include "arts/ArtsDialect.h"
#include "arts/Passes/ArtsPasses.h"
#include "arts/Utils/DbUtils.h"
#include "arts/Utils/EdtUtils.h"
#include "arts/Utils/OperationAttributes.h"
#include "arts/Utils/ValueUtils.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/Dominance.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallPtrSet.h"
#include <algorithm>

#include "arts/Utils/ArtsDebug.h"
ARTS_DEBUG_SETUP(db_opts);

using namespace mlir;
using namespace mlir::arts;

namespace {

struct ScratchUse {
  DbAcquireOp acquire;
  EdtOp edt;
  BlockArgument blockArg;
  DbRefOp dbRef;
};

struct ScratchCandidate {
  DbAllocOp alloc;
  DbRefOp initRef;
  Operation *initStore = nullptr;
  SmallVector<DbFreeOp, 2> freeOps;
  SmallVector<ScratchUse, 4> uses;
};

static bool isSingleSizeOne(ValueRange sizes) {
  return sizes.size() == 1 && ValueUtils::isOneConstant(sizes.front());
}

static bool isZeroIndexList(ValueRange values) {
  return !values.empty() &&
         llvm::all_of(values,
                      [](Value v) { return ValueUtils::isZeroConstant(v); });
}

static SmallVector<Value> getDynamicAllocaSizes(DbAllocOp alloc,
                                                MemRefType refType) {
  SmallVector<Value> dynSizes;
  ValueRange elementSizes = alloc.getElementSizes();
  if (refType.getRank() != static_cast<int64_t>(elementSizes.size()))
    return {};

  for (auto [dim, extent] : llvm::enumerate(refType.getShape()))
    if (ShapedType::isDynamic(extent))
      dynSizes.push_back(elementSizes[dim]);
  return dynSizes;
}

static bool matchInitStore(DbRefOp initRef, Operation *&initStore) {
  initStore = nullptr;
  if (!initRef)
    return true;
  if (!isZeroIndexList(initRef.getIndices()))
    return false;
  if (!initRef->hasOneUse())
    return false;

  Operation *user = *initRef->user_begin();
  auto access = DbUtils::getMemoryAccessInfo(user);
  if (!access || !access->isWrite() || access->memref != initRef.getResult())
    return false;

  initStore = user;
  return true;
}

static bool matchScratchDbRef(BlockArgument arg, DbRefOp &dbRef) {
  dbRef = nullptr;
  for (Operation *user : arg.getUsers()) {
    if (isa<DbReleaseOp>(user))
      continue;
    auto ref = dyn_cast<DbRefOp>(user);
    if (!ref || dbRef)
      return false;
    if (!isZeroIndexList(ref.getIndices()))
      return false;
    dbRef = ref;
  }
  return static_cast<bool>(dbRef);
}

static bool loadsAreInitializedByTask(DbRefOp dbRef, EdtOp edt,
                                      DominanceInfo &domInfo) {
  SmallVector<Operation *, 4> stores;
  SmallVector<Operation *, 4> loads;
  DbUtils::forEachReachableMemoryAccess(
      dbRef.getResult(),
      [&](const DbUtils::MemoryAccessInfo &access) {
        if (!DbUtils::isSameMemoryObject(access.memref, dbRef.getResult()))
          return WalkResult::interrupt();
        if (access.isWrite())
          stores.push_back(access.op);
        else
          loads.push_back(access.op);
        return WalkResult::advance();
      },
      &edt.getRegion());

  if (stores.empty())
    return false;

  for (Operation *load : loads) {
    bool covered = llvm::any_of(
        stores, [&](Operation *store) { return domInfo.dominates(store, load); });
    if (!covered)
      return false;
  }

  return true;
}

static std::optional<ScratchCandidate>
matchScratchCandidate(DbAllocOp alloc, DominanceInfo &domInfo) {
  if (!alloc || alloc.getAllocType() != DbAllocType::stack)
    return std::nullopt;
  if (!DbUtils::isCoarseGrained(alloc) || !isSingleSizeOne(alloc.getSizes()))
    return std::nullopt;

  auto ptrType = alloc.getPtr().getType().dyn_cast<MemRefType>();
  if (!ptrType || !ptrType.getElementType().isa<MemRefType>())
    return std::nullopt;

  ScratchCandidate candidate;
  candidate.alloc = alloc;
  llvm::SmallPtrSet<Operation *, 8> seenAcquireOps;

  auto classifyPtrUser = [&](Operation *user) -> bool {
    if (auto acquire = dyn_cast<DbAcquireOp>(user)) {
      if (acquire.getSourcePtr() != alloc.getPtr())
        return false;
      if (Value srcGuid = acquire.getSourceGuid();
          srcGuid && srcGuid != alloc.getGuid())
        return false;
      if (seenAcquireOps.insert(acquire.getOperation()).second)
        candidate.uses.push_back({acquire, nullptr, nullptr, nullptr});
      return true;
    }
    if (auto ref = dyn_cast<DbRefOp>(user)) {
      if (candidate.initRef || ref->getParentOfType<EdtOp>())
        return false;
      candidate.initRef = ref;
      return true;
    }
    if (auto free = dyn_cast<DbFreeOp>(user)) {
      if (!llvm::is_contained(candidate.freeOps, free))
        candidate.freeOps.push_back(free);
      return true;
    }
    return false;
  };

  auto classifyGuidUser = [&](Operation *user) -> bool {
    if (auto acquire = dyn_cast<DbAcquireOp>(user)) {
      if (acquire.getSourcePtr() != alloc.getPtr())
        return false;
      if (Value srcGuid = acquire.getSourceGuid();
          !srcGuid || srcGuid != alloc.getGuid())
        return false;
      if (seenAcquireOps.insert(acquire.getOperation()).second)
        candidate.uses.push_back({acquire, nullptr, nullptr, nullptr});
      return true;
    }
    if (auto free = dyn_cast<DbFreeOp>(user)) {
      if (!llvm::is_contained(candidate.freeOps, free))
        candidate.freeOps.push_back(free);
      return true;
    }
    return false;
  };

  for (Operation *user : alloc.getPtr().getUsers())
    if (!classifyPtrUser(user))
      return std::nullopt;
  for (Operation *user : alloc.getGuid().getUsers())
    if (!classifyGuidUser(user))
      return std::nullopt;

  if (candidate.uses.empty() ||
      !matchInitStore(candidate.initRef, candidate.initStore))
    return std::nullopt;

  for (ScratchUse &use : candidate.uses) {
    DbAcquireOp acquire = use.acquire;
    if (!acquire || acquire.getMode() == ArtsMode::in)
      return std::nullopt;
    if (!isSingleSizeOne(acquire.getSizes()) ||
        acquire.getOffsets().size() != 1 ||
        !ValueUtils::isZeroConstant(acquire.getOffsets().front()))
      return std::nullopt;

    auto mode = getPartitionMode(acquire.getOperation());
    if (mode && *mode != PartitionMode::coarse)
      return std::nullopt;

    auto [edt, blockArg] = EdtUtils::getEdtBlockArgumentForAcquire(acquire);
    if (!edt || !blockArg || acquire.getPtr().use_empty() ||
        !acquire.getPtr().hasOneUse())
      return std::nullopt;

    DbRefOp dbRef = nullptr;
    if (!matchScratchDbRef(blockArg, dbRef))
      return std::nullopt;
    if (!edt.getBody().isAncestor(dbRef->getParentRegion()))
      return std::nullopt;
    auto refType = dbRef.getResult().getType().dyn_cast<MemRefType>();
    if (!refType)
      return std::nullopt;
    if (getDynamicAllocaSizes(alloc, refType).size() !=
        static_cast<size_t>(refType.getNumDynamicDims()))
      return std::nullopt;
    if (!loadsAreInitializedByTask(dbRef, edt, domInfo))
      return std::nullopt;

    use.edt = edt;
    use.blockArg = blockArg;
    use.dbRef = dbRef;
  }

  return candidate;
}

static void eraseIfPresent(Operation *op) {
  if (op)
    op->erase();
}

struct DbOptsPass : public arts::DbOptsBase<DbOptsPass> {
  void runOnOperation() override {
    ModuleOp module = getOperation();
    SmallVector<ScratchCandidate, 8> candidates;

    module.walk([&](func::FuncOp func) {
      DominanceInfo domInfo(func);
      SmallVector<DbAllocOp, 8> allocs;
      func.walk([&](DbAllocOp alloc) { allocs.push_back(alloc); });
      for (DbAllocOp alloc : allocs)
        if (auto candidate = matchScratchCandidate(alloc, domInfo))
          candidates.push_back(std::move(*candidate));
    });

    if (candidates.empty())
      return;

    unsigned rewrites = 0;
    for (ScratchCandidate &candidate : candidates) {
      DenseMap<Operation *, SmallVector<unsigned, 4>> argsToRemove;
      DenseMap<Operation *, SmallVector<DbAcquireOp, 4>> acquiresToErase;

      for (ScratchUse &use : candidate.uses) {
        auto refType = cast<MemRefType>(use.dbRef.getResult().getType());
        SmallVector<Value> dynamicSizes =
            getDynamicAllocaSizes(candidate.alloc, refType);
        Block &entryBlock = use.edt.getBody().front();
        OpBuilder builder(&entryBlock, entryBlock.begin());
        auto local = builder.create<memref::AllocaOp>(use.dbRef.getLoc(), refType,
                                                      dynamicSizes);
        use.dbRef.getResult().replaceAllUsesWith(local.getMemref());
        use.dbRef.erase();

        SmallVector<Operation *, 4> releases;
        for (Operation *user : use.blockArg.getUsers())
          if (isa<DbReleaseOp>(user))
            releases.push_back(user);
        for (Operation *release : releases)
          release->erase();

        argsToRemove[use.edt.getOperation()].push_back(
            use.blockArg.getArgNumber());
        acquiresToErase[use.edt.getOperation()].push_back(use.acquire);
        rewrites++;
      }

      for (auto &entry : argsToRemove) {
        auto edt = cast<EdtOp>(entry.first);
        Block &body = edt.getBody().front();
        ValueRange deps = edt.getDependencies();
        SmallVector<unsigned, 4> indices = entry.second;
        llvm::sort(indices, std::greater<>());
        indices.erase(std::unique(indices.begin(), indices.end()),
                      indices.end());

        SmallVector<Value> newDeps;
        for (unsigned i = 0; i < deps.size(); ++i)
          if (!llvm::is_contained(indices, i))
            newDeps.push_back(deps[i]);

        for (unsigned idx : indices)
          body.eraseArgument(idx);
        edt.setDependencies(newDeps);
      }

      for (auto &entry : acquiresToErase) {
        for (DbAcquireOp acquire : entry.second)
          if (acquire && acquire.getGuid().use_empty() &&
              acquire.getPtr().use_empty())
            acquire.erase();
      }

      eraseIfPresent(candidate.initStore);
      eraseIfPresent(candidate.initRef.getOperation());
      for (DbFreeOp freeOp : candidate.freeOps)
        eraseIfPresent(freeOp.getOperation());
      if (candidate.alloc && candidate.alloc.getGuid().use_empty() &&
          candidate.alloc.getPtr().use_empty())
        candidate.alloc.erase();
    }

    ARTS_INFO("DbOptsPass: removed " << rewrites << " scratch DB uses");
  }
};

} // namespace

std::unique_ptr<Pass> mlir::arts::createDbOptsPass() {
  return std::make_unique<DbOptsPass>();
}
