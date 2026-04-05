///==========================================================================///
/// File: EdtUtils.cpp
///
/// Implementation of utility functions for working with ARTS EDTs.
///==========================================================================///

#include "arts/utils/EdtUtils.h"
#include "arts/analysis/value/ValueAnalysis.h"
#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Interfaces/SideEffectInterfaces.h"
#include "mlir/Transforms/RegionUtils.h"
#include "llvm/ADT/DenseSet.h"
#include <algorithm>

using namespace mlir;
using namespace mlir::arts;

namespace mlir {
namespace arts {

ForOp getSingleTopLevelFor(EdtOp edt) {
  if (!edt)
    return nullptr;

  ForOp result = nullptr;
  for (Operation &op : edt.getBody().front().without_terminator()) {
    auto forOp = dyn_cast<ForOp>(&op);
    if (!forOp)
      return nullptr;
    if (result)
      return nullptr;
    result = forOp;
  }
  return result;
}

std::pair<EdtOp, BlockArgument>
getEdtBlockArgumentForAcquire(DbAcquireOp acquireOp) {
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

EpochOp wrapBodyInEpoch(Block &body, Location loc) {
  SmallVector<Operation *, 8> opsToMove;
  for (Operation &op : body.without_terminator())
    opsToMove.push_back(&op);

  if (opsToMove.empty())
    return nullptr;

  OpBuilder builder(body.getTerminator());
  auto epochOp = builder.create<EpochOp>(loc);
  auto &epochBlock = epochOp.getRegion().emplaceBlock();

  for (Operation *op : opsToMove)
    op->moveBefore(&epochBlock, epochBlock.end());

  builder.setInsertionPointToEnd(&epochBlock);
  builder.create<YieldOp>(loc);

  return epochOp;
}

std::optional<unsigned> mapMemrefToEdtArg(EdtOp edt, Value memrefValue) {
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

void classifyEdtArgAccesses(EdtOp edt, SmallVectorImpl<bool> &reads,
                            SmallVectorImpl<bool> &writes) {
  reads.assign(edt.getDependencies().size(), false);
  writes.assign(edt.getDependencies().size(), false);

  auto markRead = [&](Value memrefValue) {
    auto argIdx = mapMemrefToEdtArg(edt, memrefValue);
    if (argIdx && *argIdx < reads.size())
      reads[*argIdx] = true;
  };
  auto markWrite = [&](Value memrefValue) {
    auto argIdx = mapMemrefToEdtArg(edt, memrefValue);
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

bool canCloneAllocaInitStore(memref::StoreOp store, Value memref) {
  if (!store || store.getMemRef() != memref)
    return false;

  llvm::DenseSet<Operation *> visited;
  if (!isCloneSafeStoreOperand(store.getValue(), memref, visited))
    return false;

  return llvm::all_of(store.getIndices(), [&](Value index) {
    return isCloneSafeStoreOperand(index, memref, visited);
  });
}

void classifyEdtUserValues(ArrayRef<Value> userValues,
                           llvm::SetVector<Value> &parameters,
                           llvm::SetVector<Value> &constants,
                           llvm::SetVector<Value> &dbHandles) {
  for (Value val : userValues) {
    if (auto *defOp = val.getDefiningOp()) {
      if (isa<DbAllocOp, DbAcquireOp>(defOp)) {
        dbHandles.insert(val);
        continue;
      }
      if (isa<arith::ConstantOp>(defOp)) {
        constants.insert(val);
        continue;
      }
    }

    if (val.getType().isIntOrIndexOrFloat())
      parameters.insert(val);
  }
}

void analyzeEdtCapturedValues(EdtOp edt, llvm::SetVector<Value> &capturedValues,
                              llvm::SetVector<Value> &parameters,
                              llvm::SetVector<Value> &constants,
                              llvm::SetVector<Value> &dbHandles) {
  if (!edt)
    return;

  getUsedValuesDefinedAbove(edt.getRegion(), capturedValues);
  classifyEdtUserValues(capturedValues.getArrayRef(), parameters, constants,
                        dbHandles);
}

} // namespace arts
} // namespace mlir
