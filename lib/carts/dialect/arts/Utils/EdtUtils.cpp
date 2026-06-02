///==========================================================================///
/// File: EdtUtils.cpp
///
/// Implementation of utility functions for working with ARTS EDTs.
///==========================================================================///

#include "carts/dialect/arts/Utils/EdtUtils.h"
#include "carts/dialect/arts/Utils/ArtsAttrNames.h"
#include "carts/dialect/arts/Utils/DbUtils.h"
#include "carts/dialect/arts/Utils/LoweringContractUtils.h"
#include "carts/utils/Utils.h"
#include "carts/utils/ValueAnalysis.h"
#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Interfaces/SideEffectInterfaces.h"
#include "mlir/Transforms/RegionUtils.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/STLExtras.h"
#include <algorithm>
#include <functional>

using namespace mlir;
using namespace mlir::carts;
using namespace mlir::carts::arts;

namespace mlir {
namespace carts::arts {

std::pair<EdtOp, BlockArgument>
EdtUtils::getBlockArgumentForAcquire(DbAcquireOp acquireOp) {
  /// Find the EDT that uses this acquire's pointer result
  EdtOp edtUser = nullptr;
  Value operandValue = nullptr;

  for (auto &use : acquireOp->getUses()) {
    Operation *userOp = use.getOwner();
    if (auto edtOp = dyn_cast<EdtOp>(userOp)) {
      edtUser = edtOp;
      operandValue = edtOp->getOperand(use.getOperandNumber());
      break;
    }
  }

  if (!edtUser || !operandValue)
    return {nullptr, nullptr};

  /// The index within dependencies equals the block argument index.
  ValueRange deps = edtUser.getDependencies();
  auto depIt = std::find(deps.begin(), deps.end(), operandValue);
  if (depIt == deps.end())
    return {nullptr, nullptr};

  unsigned blockArgIdx = std::distance(deps.begin(), depIt);

  /// Get the block argument.
  Block &body = edtUser.getRegion().front();
  if (blockArgIdx >= body.getNumArguments())
    return {nullptr, nullptr};

  BlockArgument blockArg = body.getArgument(blockArgIdx);
  return {edtUser, blockArg};
}

std::optional<unsigned> EdtUtils::mapMemrefToArg(EdtOp edt, Value memrefValue) {
  if (!memrefValue)
    return std::nullopt;
  Value current = ValueAnalysis::stripMemrefViewOps(memrefValue);
  if (auto dbRef = current.getDefiningOp<DbRefOp>())
    current = ValueAnalysis::stripMemrefViewOps(dbRef.getSource());

  auto blockArg = dyn_cast<BlockArgument>(current);
  if (!blockArg)
    return std::nullopt;
  Block *edtBody = &edt.getBody().front();
  if (blockArg.getOwner() != edtBody)
    return std::nullopt;
  return blockArg.getArgNumber();
}

void EdtUtils::classifyArgAccesses(EdtOp edt, SmallVectorImpl<bool> &reads,
                                   SmallVectorImpl<bool> &writes) {
  reads.assign(edt.getDependencies().size(), false);
  writes.assign(edt.getDependencies().size(), false);

  auto markRead = [&](Value memrefValue) {
    auto argIdx = EdtUtils::mapMemrefToArg(edt, memrefValue);
    if (argIdx && *argIdx < reads.size())
      reads[*argIdx] = true;
  };
  auto markWrite = [&](Value memrefValue) {
    auto argIdx = EdtUtils::mapMemrefToArg(edt, memrefValue);
    if (argIdx && *argIdx < writes.size())
      writes[*argIdx] = true;
  };

  edt.walk([&](Operation *nested) {
    if (auto load = dyn_cast<memref::LoadOp>(nested))
      markRead(load.getMemRef());
    else if (auto store = dyn_cast<memref::StoreOp>(nested))
      markWrite(store.getMemRef());
    else if (auto affineLoad = dyn_cast<affine::AffineLoadOp>(nested))
      markRead(affineLoad.getMemRef());
    else if (auto affineStore = dyn_cast<affine::AffineStoreOp>(nested))
      markWrite(affineStore.getMemRef());
  });
}

namespace {

static bool acquireHasCoarseEntry(DbAcquireOp acquire) {
  if (!acquire)
    return false;
  if (!acquire.hasMultiplePartitionEntries())
    return acquire.getPartitionModeOr() == PartitionMode::coarse;
  for (size_t i = 0, e = acquire.getNumPartitionEntries(); i < e; ++i)
    if (acquire.getPartitionEntryMode(i) == PartitionMode::coarse)
      return true;
  return false;
}

static bool acquireHasSubpartitionEntry(DbAcquireOp acquire) {
  if (!acquire)
    return false;
  if (!acquire.hasMultiplePartitionEntries())
    return acquire.getPartitionModeOr() != PartitionMode::coarse;
  for (size_t i = 0, e = acquire.getNumPartitionEntries(); i < e; ++i)
    if (acquire.getPartitionEntryMode(i) != PartitionMode::coarse)
      return true;
  return false;
}

static bool acquireCarriesPlannedSubpartitionEvidence(DbAcquireOp acquire) {
  if (!acquire)
    return false;

  if (acquire.getPreserveDepEdge() || acquire.getReplicatedRead() ||
      acquire.getDepPatternAttr() || acquire.getDistributionKindAttr() ||
      acquire.getDistributionPatternAttr() ||
      acquire.getDistributionVersionAttr())
    return true;

  if (acquire.getStencilCenterOffsetAttr() ||
      acquire.getStencilMinOffsetsAttr() ||
      acquire.getStencilMaxOffsetsAttr() ||
      acquire.getStencilSpatialDimsAttr() ||
      acquire.getStencilOwnerDimsAttr() || acquire.getStencilBlockShapeAttr() ||
      acquire.getStencilWriteFootprintAttr() ||
      acquire.getStencilSupportedBlockHaloAttr())
    return true;

  Operation *root = DbUtils::getUnderlyingDbAlloc(acquire.getSourcePtr());
  auto alloc = dyn_cast_or_null<DbAllocOp>(root);
  return alloc &&
         (alloc.getDistributedAttr() || alloc.getOwnerMapKindAttr() ||
          alloc.getOwnerMapVersionAttr() || alloc.getOwnerMapDimsAttr() ||
          alloc.getOwnerBlockShapeAttr() || alloc.getOwnerNodeShapeAttr() ||
          alloc.getPlanOwnerDimsAttr() ||
          alloc.getPlanPhysicalBlockShapeAttr() ||
          alloc.getPlanLogicalWorkerSliceAttr() ||
          alloc.getPlanHaloShapeAttr() ||
          alloc.getPlanIterationTopologyAttr() ||
          alloc.getPlanRepetitionStructureAttr() ||
          alloc.getPlanAsyncStrategyAttr() ||
          alloc->hasAttr("perBlockReplicated") ||
          alloc->hasAttr("perBlockSingleWriterStencil") ||
          alloc.getStencilOwnerDimsAttr() || alloc.getStencilBlockShapeAttr() ||
          alloc.getStencilSupportedBlockHaloAttr());
}

static void forceCoarseAcquire(DbAcquireOp acquire) {
  acquire.clearPartitionHints();
  acquire.setPartitionModeAttr(
      PartitionModeAttr::get(acquire.getContext(), PartitionMode::coarse));
  if (auto contractOp = getLoweringContractOp(acquire.getPtr()))
    contractOp->removeAttr(AttrNames::Contract::NarrowableDep);
}

struct RootDepInfo {
  unsigned index = 0;
  DbAcquireOp acquire;
};

struct RootDepGroup {
  SmallVector<RootDepInfo, 4> deps;
  bool hasCoarse = false;
  bool hasSubpartition = false;
  bool hasProtectedSubpartition = false;
  ArtsMode combinedMode = ArtsMode::uninitialized;
};

} // namespace

unsigned EdtUtils::canonicalizeMixedRootDependencies(ModuleOp module) {
  if (!module)
    return 0;

  unsigned changedDeps = 0;

  module.walk([&](EdtOp edt) {
    Block &body = edt.getBody().front();
    ValueRange deps = edt.getDependencies();
    if (deps.size() > body.getNumArguments())
      return;

    DenseMap<Operation *, RootDepGroup> byRoot;
    for (auto [idx, dep] : llvm::enumerate(deps)) {
      auto acquire = dep.getDefiningOp<DbAcquireOp>();
      if (!acquire)
        continue;
      Operation *root = DbUtils::getUnderlyingDbAlloc(acquire.getSourcePtr());
      if (!root)
        continue;
      RootDepGroup &group = byRoot[root];
      group.deps.push_back({static_cast<unsigned>(idx), acquire});
      group.hasCoarse |= acquireHasCoarseEntry(acquire);
      group.hasSubpartition |= acquireHasSubpartitionEntry(acquire);
      group.hasProtectedSubpartition |=
          acquireHasSubpartitionEntry(acquire) &&
          acquireCarriesPlannedSubpartitionEvidence(acquire);
      group.combinedMode =
          combineAccessModes(group.combinedMode, acquire.getMode());
    }

    DenseSet<unsigned> removeIndices;
    for (auto &entry : byRoot) {
      RootDepGroup &group = entry.second;
      if (!group.hasCoarse || !group.hasSubpartition || group.deps.size() < 2)
        continue;
      if (group.hasProtectedSubpartition)
        continue;

      unsigned repPos = 0;
      bool foundWriter = false;
      for (auto [pos, depInfo] : llvm::enumerate(group.deps)) {
        if (!acquireHasCoarseEntry(depInfo.acquire))
          continue;
        if (DbUtils::isWriterMode(depInfo.acquire.getMode())) {
          repPos = static_cast<unsigned>(pos);
          foundWriter = true;
          break;
        }
        if (!foundWriter)
          repPos = static_cast<unsigned>(pos);
      }
      if (!foundWriter) {
        for (auto [pos, depInfo] : llvm::enumerate(group.deps)) {
          if (DbUtils::isWriterMode(depInfo.acquire.getMode())) {
            repPos = static_cast<unsigned>(pos);
            break;
          }
        }
      }

      RootDepInfo representative = group.deps[repPos];
      BlockArgument representativeArg = body.getArgument(representative.index);
      representative.acquire.setModeAttr(ArtsModeAttr::get(
          representative.acquire.getContext(), group.combinedMode));
      forceCoarseAcquire(representative.acquire);

      for (RootDepInfo depInfo : group.deps) {
        forceCoarseAcquire(depInfo.acquire);
        if (depInfo.index == representative.index)
          continue;
        if (depInfo.acquire.getPreserveDepEdge())
          continue;
        BlockArgument arg = body.getArgument(depInfo.index);
        if (arg.getType() != representativeArg.getType())
          continue;
        arg.replaceAllUsesWith(representativeArg);
        removeIndices.insert(depInfo.index);
      }
    }

    if (removeIndices.empty())
      return;

    SmallVector<Value> newDeps;
    newDeps.reserve(deps.size() - removeIndices.size());
    for (auto [idx, dep] : llvm::enumerate(deps)) {
      if (!removeIndices.contains(static_cast<unsigned>(idx)))
        newDeps.push_back(dep);
    }

    SmallVector<unsigned, 4> sortedRemove(removeIndices.begin(),
                                          removeIndices.end());
    llvm::sort(sortedRemove, std::greater<>());
    for (unsigned idx : sortedRemove)
      body.eraseArgument(idx);
    edt.setDependencies(newDeps);

    changedDeps += sortedRemove.size();
  });

  return changedDeps;
}

namespace {
static bool isCloneSafeStoreOperand(Value value, Value memref,
                                    llvm::DenseSet<Operation *> &visited) {
  if (!value || value == memref)
    return true;

  if (isa<BlockArgument>(value))
    return true;

  Operation *defOp = value.getDefiningOp();
  if (!defOp)
    return true;

  if (defOp->hasTrait<OpTrait::ConstantLike>())
    return true;

  if (defOp->getNumRegions() != 0)
    return false;

  bool hasSideEffects = false;
  if (auto memEffects = dyn_cast<MemoryEffectOpInterface>(defOp)) {
    hasSideEffects = memEffects.hasEffect<MemoryEffects::Write>() ||
                     memEffects.hasEffect<MemoryEffects::Allocate>() ||
                     memEffects.hasEffect<MemoryEffects::Free>();
  } else {
    hasSideEffects = !isMemoryEffectFree(defOp);
  }
  if (hasSideEffects)
    return false;

  if (!visited.insert(defOp).second)
    return true;

  return llvm::all_of(defOp->getOperands(), [&](Value operand) {
    return isCloneSafeStoreOperand(operand, memref, visited);
  });
}

} // namespace

bool EdtUtils::canCloneAllocaInitStore(memref::StoreOp store, Value memref) {
  if (!store || store.getMemRef() != memref)
    return false;

  llvm::DenseSet<Operation *> visited;
  if (!isCloneSafeStoreOperand(store.getValue(), memref, visited))
    return false;

  return llvm::all_of(store.getIndices(), [&](Value index) {
    return isCloneSafeStoreOperand(index, memref, visited);
  });
}

Value EdtUtils::traceCapturedDbHandle(Value value) {
  DenseSet<Value> visited;
  for (unsigned depth = 0; value && depth < 16; ++depth) {
    if (!visited.insert(value).second)
      break;

    Operation *defOp = value.getDefiningOp();
    if (!defOp)
      return Value();

    if (isa<DbAllocOp, DbAcquireOp, memref::AllocOp>(defOp))
      return value;

    if (auto dbRef = dyn_cast<DbRefOp>(defOp)) {
      value = dbRef.getSource();
      continue;
    }
    if (auto cast = dyn_cast<memref::CastOp>(defOp)) {
      value = cast.getSource();
      continue;
    }
    if (auto subview = dyn_cast<memref::SubViewOp>(defOp)) {
      value = subview.getSource();
      continue;
    }
    if (auto unrealized = dyn_cast<UnrealizedConversionCastOp>(defOp)) {
      if (unrealized.getInputs().size() == 1) {
        value = unrealized.getInputs().front();
        continue;
      }
    }

    break;
  }
  return Value();
}

void EdtUtils::classifyUserValues(ArrayRef<Value> userValues,
                                  llvm::SetVector<Value> &parameters,
                                  llvm::SetVector<Value> &constants,
                                  llvm::SetVector<Value> &dbHandles) {
  for (Value val : userValues) {
    if (Value dbHandle = EdtUtils::traceCapturedDbHandle(val)) {
      dbHandles.insert(dbHandle);
      continue;
    }

    if (auto *defOp = val.getDefiningOp()) {
      if (isa<arith::ConstantOp>(defOp)) {
        constants.insert(val);
        continue;
      }
    }

    /// Direct scalar captures are not implicit parameters. Every scalar that
    /// crosses an EDT boundary must be listed on `arts.edt params(...)` and
    /// used through the corresponding block argument.
    if (val.getType().isIntOrIndexOrFloat())
      continue;

    // Stack allocas for loop-local scratch remain clonable. Heap memref.alloc
    // handles are classified by traceCapturedDbHandle above.
  }
}

void EdtUtils::analyzeCapturedValues(EdtOp edt,
                                     llvm::SetVector<Value> &capturedValues,
                                     llvm::SetVector<Value> &parameters,
                                     llvm::SetVector<Value> &constants,
                                     llvm::SetVector<Value> &dbHandles) {
  if (!edt)
    return;

  getUsedValuesDefinedAbove(edt.getRegion(), capturedValues);
  /// RegionUtils also reports values defined in the EDT body when they are
  /// referenced from nested regions inside the EDT. Those are not true
  /// captures for outlining: they must remain local to the outlined function.
  auto isDefinedInsideEdt = [&](Value value) {
    if (Operation *defOp = value.getDefiningOp())
      return edt.getOperation()->isAncestor(defOp);
    if (auto blockArg = dyn_cast<BlockArgument>(value)) {
      if (Operation *parentOp = blockArg.getOwner()->getParentOp())
        return edt.getOperation()->isAncestor(parentOp);
    }
    return false;
  };
  llvm::SetVector<Value> externalCaptures;
  for (Value value : capturedValues)
    if (!isDefinedInsideEdt(value))
      externalCaptures.insert(value);
  capturedValues = std::move(externalCaptures);
  EdtUtils::classifyUserValues(capturedValues.getArrayRef(), parameters,
                               constants, dbHandles);
}

} // namespace carts::arts
} // namespace mlir
