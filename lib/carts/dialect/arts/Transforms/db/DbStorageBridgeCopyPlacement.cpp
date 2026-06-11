///==========================================================================///
/// File: DbStorageBridgeCopyPlacement.cpp
///
/// Place realized storage-bridge copy-outs at coarse host observations.
///
/// Before:
///   scf.for { arts.edt ... attributes {storageBridgeCopy} }
///   arts.barrier
///   ... later block writers ...
///   func.call @timer_stop(...)
///   %v = memref.load %coarse[%i]
///
/// After:
///   func.call @timer_stop(...)
///   scf.for { arts.edt ... attributes {storageBridgeCopy} }  // last writer
///   arts.barrier
///   %v = memref.load %coarse[%i]
///==========================================================================///

#define GEN_PASS_DEF_DBSTORAGEBRIDGECOPYPLACEMENT
#include "carts/dialect/arts/IR/ArtsDialect.h"
#include "carts/dialect/arts/Utils/DbUtils.h"
#include "carts/dialect/arts/Utils/OperationAttributes.h"
#include "carts/passes/Passes.h"
#include "carts/passes/Passes.h.inc"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/Pass/Pass.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/Hashing.h"
#include "llvm/ADT/Statistic.h"

using namespace mlir;
using namespace mlir::carts;
using namespace mlir::carts::arts;

static llvm::Statistic numBridgeCopyOutsMoved{
    "db_storage_bridge_copy_placement", "NumBridgeCopyOutsMoved",
    "Number of block-to-host storage bridge copy-outs moved to host reads"};
static llvm::Statistic numStaleBridgeCopyOutsRemoved{
    "db_storage_bridge_copy_placement", "NumStaleBridgeCopyOutsRemoved",
    "Number of stale block-to-host storage bridge copy-outs removed"};
static llvm::Statistic numLoopBridgeCopyOutsSunk{
    "db_storage_bridge_copy_placement", "NumLoopBridgeCopyOutsSunk",
    "Number of loop-contained block-to-host storage bridge copy-outs sunk"};

namespace {

struct BridgeCopyOut {
  scf::ForOp loop;
  BarrierOp barrier;
  DbAllocOp hostAlloc;
  DbAllocOp blockAlloc;
};

struct BridgeCopyKey {
  Operation *host = nullptr;
  Operation *block = nullptr;

  bool operator==(const BridgeCopyKey &other) const {
    return host == other.host && block == other.block;
  }
};

struct BridgeCopyKeyInfo {
  static inline BridgeCopyKey getEmptyKey() {
    return {DenseMapInfo<Operation *>::getEmptyKey(),
            DenseMapInfo<Operation *>::getEmptyKey()};
  }
  static inline BridgeCopyKey getTombstoneKey() {
    return {DenseMapInfo<Operation *>::getTombstoneKey(),
            DenseMapInfo<Operation *>::getTombstoneKey()};
  }
  static unsigned getHashValue(const BridgeCopyKey &key) {
    return llvm::hash_combine(key.host, key.block);
  }
  static bool isEqual(const BridgeCopyKey &lhs, const BridgeCopyKey &rhs) {
    return lhs == rhs;
  }
};

static bool isSingleNode(ModuleOp module) {
  std::optional<int64_t> totalNodes = arts::getRuntimeTotalNodes(module);
  return totalNodes && *totalNodes == 1;
}

static bool isPartitionedAs(DbAcquireOp acquire, PartitionMode mode) {
  if (!acquire)
    return false;
  std::optional<PartitionMode> partitionMode = acquire.getPartitionMode();
  return partitionMode && *partitionMode == mode;
}

static bool isHostWholeToComputeBlockBridgeDb(DbAllocOp alloc) {
  if (!alloc)
    return false;
  StorageBridgeAttr bridge = alloc.getStorageBridgeAttr();
  return bridge &&
         bridge.getValue() == StorageBridge::host_whole_to_compute_block;
}

static bool classifyBlockToHostBridgeCopy(EdtOp edt, DbAllocOp &hostAlloc,
                                          DbAllocOp &blockAlloc) {
  if (!edt || !edt.getStorageBridgeCopyAttr())
    return false;

  ValueRange deps = edt.getDependencies();
  if (deps.size() < 2)
    return false;

  auto hostAcquire = deps.front().getDefiningOp<DbAcquireOp>();
  if (!hostAcquire || !DbUtils::isWriterMode(hostAcquire.getMode()) ||
      !isPartitionedAs(hostAcquire, PartitionMode::coarse))
    return false;

  auto candidateHost =
      dyn_cast_or_null<DbAllocOp>(DbUtils::getUnderlyingDbAlloc(deps.front()));
  if (!DbUtils::isCoarseUserDataDb(candidateHost))
    return false;

  DbAllocOp candidateBlock;
  for (Value dep : deps.drop_front()) {
    auto blockAcquire = dep.getDefiningOp<DbAcquireOp>();
    if (!blockAcquire || blockAcquire.getMode() != ArtsMode::in ||
        !isPartitionedAs(blockAcquire, PartitionMode::block))
      return false;
    auto depBlock =
        dyn_cast_or_null<DbAllocOp>(DbUtils::getUnderlyingDbAlloc(dep));
    if (!isHostWholeToComputeBlockBridgeDb(depBlock))
      return false;
    if (candidateBlock && candidateBlock != depBlock)
      return false;
    candidateBlock = depBlock;
  }

  if (!candidateBlock)
    return false;
  hostAlloc = candidateHost;
  blockAlloc = candidateBlock;
  return true;
}

static std::optional<BridgeCopyOut> getBridgeCopyOut(scf::ForOp loop) {
  if (!loop || loop->getParentOfType<EdtOp>())
    return std::nullopt;

  SmallVector<EdtOp, 2> copyTasks;
  loop.walk([&](EdtOp edt) {
    if (edt.getStorageBridgeCopyAttr())
      copyTasks.push_back(edt);
  });
  if (copyTasks.size() != 1)
    return std::nullopt;

  DbAllocOp hostAlloc;
  DbAllocOp blockAlloc;
  if (!classifyBlockToHostBridgeCopy(copyTasks.front(), hostAlloc, blockAlloc))
    return std::nullopt;

  auto barrier = dyn_cast_or_null<BarrierOp>(loop->getNextNode());
  if (!barrier)
    return std::nullopt;

  return BridgeCopyOut{loop, barrier, hostAlloc, blockAlloc};
}

static bool accessTouchesAlloc(const DbUtils::MemoryAccessInfo &access,
                               DbAllocOp alloc) {
  if (!alloc || !access.memref)
    return false;
  auto root =
      dyn_cast_or_null<DbAllocOp>(DbUtils::getUnderlyingDbAlloc(access.memref));
  return root == alloc;
}

static bool operationReadsHostAlloc(Operation *op, DbAllocOp hostAlloc) {
  bool readsHost = false;
  op->walk([&](Operation *nested) {
    if (readsHost)
      return WalkResult::interrupt();
    if (nested->getParentOfType<EdtOp>())
      return WalkResult::advance();
    std::optional<DbUtils::MemoryAccessInfo> access =
        DbUtils::getMemoryAccessInfo(nested);
    if (access && access->isRead() && accessTouchesAlloc(*access, hostAlloc)) {
      readsHost = true;
      return WalkResult::interrupt();
    }
    return WalkResult::advance();
  });
  return readsHost;
}

static bool operationDirectlyReadsHostAlloc(Operation *op,
                                            DbAllocOp hostAlloc) {
  if (!op || op->getParentOfType<EdtOp>())
    return false;
  std::optional<DbUtils::MemoryAccessInfo> access =
      DbUtils::getMemoryAccessInfo(op);
  return access && access->isRead() && accessTouchesAlloc(*access, hostAlloc);
}

static Operation *findHostReadInsertionPoint(Operation *observation,
                                             DbAllocOp hostAlloc) {
  if (operationDirectlyReadsHostAlloc(observation, hostAlloc))
    return observation;

  if (!isa<scf::IfOp>(observation))
    return observation;

  for (Region &region : observation->getRegions()) {
    for (Block &block : region) {
      for (Operation &nested : block) {
        if (operationReadsHostAlloc(&nested, hostAlloc))
          return &nested;
      }
    }
  }
  return observation;
}

static bool operationWritesHostAlloc(Operation *op, DbAllocOp hostAlloc) {
  bool writesHost = false;
  op->walk([&](Operation *nested) {
    if (writesHost)
      return WalkResult::interrupt();
    if (nested->getParentOfType<EdtOp>())
      return WalkResult::advance();
    if (isa<memref::DeallocOp>(nested)) {
      for (Value operand : nested->getOperands()) {
        auto root =
            dyn_cast_or_null<DbAllocOp>(DbUtils::getUnderlyingDbAlloc(operand));
        if (root == hostAlloc) {
          writesHost = true;
          return WalkResult::interrupt();
        }
      }
      return WalkResult::advance();
    }
    std::optional<DbUtils::MemoryAccessInfo> access =
        DbUtils::getMemoryAccessInfo(nested);
    if (access && access->isWrite() && accessTouchesAlloc(*access, hostAlloc)) {
      writesHost = true;
      return WalkResult::interrupt();
    }
    return WalkResult::advance();
  });
  return writesHost;
}

static bool operationWritesBlockAlloc(Operation *op, DbAllocOp blockAlloc) {
  bool writesBlock = false;
  op->walk([&](Operation *nested) {
    if (writesBlock)
      return WalkResult::interrupt();
    if (auto acquire = dyn_cast<DbAcquireOp>(nested)) {
      if (!DbUtils::isWriterMode(acquire.getMode()))
        return WalkResult::advance();
      auto root = dyn_cast_or_null<DbAllocOp>(
          DbUtils::getUnderlyingDbAlloc(acquire.getPtr()));
      if (root == blockAlloc) {
        writesBlock = true;
        return WalkResult::interrupt();
      }
    }
    return WalkResult::advance();
  });
  return writesBlock;
}

static bool sameBlockCopy(const BridgeCopyOut &lhs, const BridgeCopyOut &rhs) {
  return lhs.loop->getBlock() == rhs.loop->getBlock() &&
         lhs.barrier->getBlock() == rhs.barrier->getBlock();
}

static void eraseCopyOut(BridgeCopyOut copyOut,
                         DenseSet<Operation *> &erasedOps) {
  erasedOps.insert(copyOut.loop.getOperation());
  erasedOps.insert(copyOut.barrier.getOperation());
  copyOut.barrier.erase();
  copyOut.loop.erase();
}

static bool isValueDefinedInsideButOutsideCopy(Value value,
                                               scf::ForOp outerLoop,
                                               scf::ForOp copyLoop) {
  if (Operation *def = value.getDefiningOp())
    return outerLoop->isAncestor(def) && !copyLoop->isAncestor(def);

  auto blockArg = dyn_cast<BlockArgument>(value);
  if (!blockArg)
    return false;
  Operation *owner = blockArg.getOwner()->getParentOp();
  return owner && outerLoop->isAncestor(owner) && !copyLoop->isAncestor(owner);
}

static bool copyOutCanMoveAfterLoop(BridgeCopyOut copyOut,
                                    scf::ForOp outerLoop) {
  bool canMove = true;
  copyOut.loop->walk([&](Operation *nested) {
    for (Value operand : nested->getOperands()) {
      if (isValueDefinedInsideButOutsideCopy(operand, outerLoop,
                                             copyOut.loop)) {
        canMove = false;
        return WalkResult::interrupt();
      }
    }
    return WalkResult::advance();
  });
  if (!canMove)
    return false;
  for (Value operand : copyOut.barrier->getOperands())
    if (isValueDefinedInsideButOutsideCopy(operand, outerLoop, copyOut.loop))
      return false;
  return true;
}

static bool loopHasHostAccessOutsideCopy(scf::ForOp loop, DbAllocOp hostAlloc,
                                         BridgeCopyOut copyOut) {
  bool hasAccess = false;
  loop->walk([&](Operation *nested) {
    if (nested == copyOut.loop.getOperation() ||
        copyOut.loop->isAncestor(nested) ||
        nested == copyOut.barrier.getOperation())
      return WalkResult::skip();
    if (nested->getParentOfType<EdtOp>())
      return WalkResult::advance();

    if (isa<memref::DeallocOp>(nested)) {
      for (Value operand : nested->getOperands()) {
        auto root =
            dyn_cast_or_null<DbAllocOp>(DbUtils::getUnderlyingDbAlloc(operand));
        if (root == hostAlloc) {
          hasAccess = true;
          return WalkResult::interrupt();
        }
      }
      return WalkResult::advance();
    }

    std::optional<DbUtils::MemoryAccessInfo> access =
        DbUtils::getMemoryAccessInfo(nested);
    if (access && accessTouchesAlloc(*access, hostAlloc)) {
      hasAccess = true;
      return WalkResult::interrupt();
    }

    return WalkResult::advance();
  });
  return hasAccess;
}

static unsigned sinkLoopContainedCopyOuts(scf::ForOp loop, unsigned &removed) {
  if (!loop || loop.getRegion().empty())
    return 0;
  SmallVector<BridgeCopyOut, 4> candidates;
  for (Operation &op : loop.getBody()->without_terminator()) {
    auto nestedLoop = dyn_cast<scf::ForOp>(op);
    if (!nestedLoop)
      continue;
    if (std::optional<BridgeCopyOut> copyOut = getBridgeCopyOut(nestedLoop))
      candidates.push_back(*copyOut);
  }

  DenseMap<BridgeCopyKey, BridgeCopyOut, BridgeCopyKeyInfo> lastCopy;
  DenseSet<Operation *> erasedOps;
  for (BridgeCopyOut copyOut : candidates) {
    if (!copyOutCanMoveAfterLoop(copyOut, loop) ||
        loopHasHostAccessOutsideCopy(loop, copyOut.hostAlloc, copyOut))
      continue;

    BridgeCopyKey key{copyOut.hostAlloc.getOperation(),
                      copyOut.blockAlloc.getOperation()};
    auto it = lastCopy.find(key);
    if (it != lastCopy.end() && sameBlockCopy(it->second, copyOut)) {
      eraseCopyOut(it->second, erasedOps);
      lastCopy.erase(it);
      ++removed;
    }
    lastCopy.insert({key, copyOut});
  }

  unsigned sunk = 0;
  Operation *anchor = loop.getOperation();
  for (const auto &entry : lastCopy) {
    BridgeCopyOut copyOut = entry.second;
    if (erasedOps.contains(copyOut.loop.getOperation()))
      continue;
    copyOut.loop->moveAfter(anchor);
    copyOut.barrier->moveAfter(copyOut.loop);
    anchor = copyOut.barrier.getOperation();
    ++sunk;
  }
  return sunk;
}

static unsigned coalesceStorageBridgeCopyOutsInBlock(Block &block,
                                                     unsigned &removed) {
  SmallVector<Operation *> ops;
  for (Operation &op : block)
    ops.push_back(&op);

  DenseMap<BridgeCopyKey, BridgeCopyOut, BridgeCopyKeyInfo> pending;
  DenseSet<Operation *> erasedOps;
  unsigned moved = 0;

  auto erasePendingForBlockWrite = [&](DbAllocOp blockAlloc) {
    SmallVector<BridgeCopyKey, 4> stale;
    for (const auto &entry : pending)
      if (entry.second.blockAlloc == blockAlloc)
        stale.push_back(entry.first);
    for (BridgeCopyKey key : stale) {
      eraseCopyOut(pending.lookup(key), erasedOps);
      pending.erase(key);
      ++removed;
    }
  };

  auto flushPendingForHostRead = [&](Operation *observation,
                                     DbAllocOp hostAlloc) {
    SmallVector<BridgeCopyKey, 4> ready;
    for (const auto &entry : pending)
      if (entry.second.hostAlloc == hostAlloc)
        ready.push_back(entry.first);
    for (BridgeCopyKey key : ready) {
      BridgeCopyOut copyOut = pending.lookup(key);
      Operation *insertionPoint =
          findHostReadInsertionPoint(observation, hostAlloc);
      copyOut.loop->moveBefore(insertionPoint);
      copyOut.barrier->moveAfter(copyOut.loop);
      pending.erase(key);
      ++moved;
    }
  };

  auto clearPendingForHostWrite = [&](DbAllocOp hostAlloc) {
    SmallVector<BridgeCopyKey, 4> blocked;
    for (const auto &entry : pending)
      if (entry.second.hostAlloc == hostAlloc)
        blocked.push_back(entry.first);
    for (BridgeCopyKey key : blocked)
      pending.erase(key);
  };

  for (Operation *op : ops) {
    if (!op || erasedOps.contains(op) || op->getBlock() != &block)
      continue;

    if (auto loop = dyn_cast<scf::ForOp>(op)) {
      if (std::optional<BridgeCopyOut> copyOut = getBridgeCopyOut(loop)) {
        BridgeCopyKey key{copyOut->hostAlloc.getOperation(),
                          copyOut->blockAlloc.getOperation()};
        auto it = pending.find(key);
        if (it != pending.end() && sameBlockCopy(it->second, *copyOut)) {
          eraseCopyOut(it->second, erasedOps);
          pending.erase(it);
          ++removed;
        }
        pending.insert({key, *copyOut});
        continue;
      }
    }

    SmallVector<DbAllocOp, 4> hostReads;
    for (const auto &entry : pending)
      if (operationReadsHostAlloc(op, entry.second.hostAlloc) &&
          !llvm::is_contained(hostReads, entry.second.hostAlloc))
        hostReads.push_back(entry.second.hostAlloc);
    for (DbAllocOp hostAlloc : hostReads)
      flushPendingForHostRead(op, hostAlloc);

    SmallVector<DbAllocOp, 4> hostWrites;
    for (const auto &entry : pending)
      if (operationWritesHostAlloc(op, entry.second.hostAlloc) &&
          !llvm::is_contained(hostWrites, entry.second.hostAlloc))
        hostWrites.push_back(entry.second.hostAlloc);
    for (DbAllocOp hostAlloc : hostWrites)
      clearPendingForHostWrite(hostAlloc);

    SmallVector<DbAllocOp, 4> blockWrites;
    for (const auto &entry : pending)
      if (operationWritesBlockAlloc(op, entry.second.blockAlloc) &&
          !llvm::is_contained(blockWrites, entry.second.blockAlloc))
        blockWrites.push_back(entry.second.blockAlloc);
    for (DbAllocOp blockAlloc : blockWrites)
      erasePendingForBlockWrite(blockAlloc);
  }

  return moved;
}

static unsigned placeStorageBridgeCopyOuts(ModuleOp module) {
  unsigned moved = 0;
  unsigned removed = 0;
  unsigned sunk = 0;
  SmallVector<scf::ForOp, 16> loops;
  module.walk([&](scf::ForOp loop) {
    if (!getBridgeCopyOut(loop))
      loops.push_back(loop);
  });
  for (scf::ForOp loop : llvm::reverse(loops))
    sunk += sinkLoopContainedCopyOuts(loop, removed);

  module.walk([&](Operation *op) {
    for (Region &region : op->getRegions())
      for (Block &block : region)
        moved += coalesceStorageBridgeCopyOutsInBlock(block, removed);
  });
  numStaleBridgeCopyOutsRemoved += removed;
  numLoopBridgeCopyOutsSunk += sunk;
  return moved;
}

struct DbStorageBridgeCopyPlacementPass
    : public impl::DbStorageBridgeCopyPlacementBase<
          DbStorageBridgeCopyPlacementPass> {
  DbStorageBridgeCopyPlacementPass() = default;

  void runOnOperation() override {
    ModuleOp module = getOperation();
    if (!isSingleNode(module))
      return;
    numBridgeCopyOutsMoved += placeStorageBridgeCopyOuts(module);
  }
};

} // namespace

std::unique_ptr<Pass>
mlir::carts::arts::createDbStorageBridgeCopyPlacementPass() {
  return std::make_unique<DbStorageBridgeCopyPlacementPass>();
}
