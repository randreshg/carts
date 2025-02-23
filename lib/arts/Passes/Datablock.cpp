///==========================================================================
/// File: DataBlock.cpp
///==========================================================================

/// Dialects
#include "arts/Utils/ArtsUtils.h"
#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/AffineMap.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/MLIRContext.h"
#include "mlir/Support/LLVM.h"
#include "polygeist/Ops.h"
/// Arts
#include "ArtsPassDetails.h"
#include "arts/Passes/ArtsPasses.h"
#include "arts/Analysis/DataBlockAnalysis.h"
#include "arts/ArtsDialect.h"
/// Other
#include "mlir/IR/Dominance.h"
#include "mlir/IR/OpDefinition.h"
#include "mlir/IR/Operation.h"
#include "mlir/Pass/Pass.h"
/// Debug
#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include <cstdint>

#define DEBUG_TYPE "datablock"
#define line "-----------------------------------------\n"
#define dbgs() (llvm::dbgs())
#define DBGS() (dbgs() << "[" DEBUG_TYPE "] ")

using namespace mlir;
using namespace mlir::func;
using namespace mlir::arith;
using namespace mlir::polygeist;
using namespace mlir::arts;

/// Returns true if any memory effect of dbOp may alias the given value.
static bool ptrMayAlias(DataBlockOp dbOp, Value val) {
  /// Get aliasing effects.
  if (auto memEff = dyn_cast<MemoryEffectOpInterface>(dbOp.getOperation())) {
    SmallVector<MemoryEffects::EffectInstance, 2> effects;
    memEff.getEffects(effects);
    return llvm::any_of(effects, [&](const MemoryEffects::EffectInstance &eA) {
      return mayAlias(eA, val);
    });
  }
  return false;
}

/// Returns the affine map associated with the operation, if any.
static std::optional<AffineMap> getAffineMap(Operation *op) {
  /// Check for affine load, store, or datablock.
  if (auto loadOp = dyn_cast<affine::AffineLoadOp>(op))
    return loadOp.getAffineMap();
  if (auto storeOp = dyn_cast<affine::AffineStoreOp>(op))
    return storeOp.getAffineMap();
  if (auto dbOp = dyn_cast<arts::DataBlockOp>(op))
    return dbOp.getAffineMap();
  return std::nullopt;
}

/// Sets the affine map attribute for the operation.
static void setAffineMap(Operation *op, AffineMap map) {
  auto mapAttr = AffineMapAttr::get(map);
  if (auto loadOp = dyn_cast<affine::AffineLoadOp>(op))
    loadOp->setAttr("map", mapAttr);
  else if (auto storeOp = dyn_cast<affine::AffineStoreOp>(op))
    storeOp->setAttr("map", mapAttr);
  else if (auto dbOp = dyn_cast<arts::DataBlockOp>(op))
    dbOp->setAttr("affineMap", mapAttr);
}

/// Returns true if the given operation uses the datablock.
template <typename OpTy>
static bool isDataBlockUsed(arts::DataBlockOp &dbOp, OpTy op) {
  /// Check valid operation.
  if (!op)
    return false;

  /// Check if the memref aliases the datablock.
  if (!ptrMayAlias(dbOp, op.getMemref()))
    return false;

  /// Get operation indices.
  const auto opIndices = op.getIndices();
  const auto dbIndices = dbOp.getIndices();

  /// The operation cannot have fewer indices than the datablock.
  const uint32_t dbIndicesSize = dbIndices.size();
  if (opIndices.size() < dbIndicesSize)
    return false;

  /// Get affine maps for both operations.
  std::optional<AffineMap> opAffineMap = getAffineMap(op.getOperation());
  std::optional<AffineMap> dbAffineMap = getAffineMap(dbOp.getOperation());

  /// If the datablock is affine, the op must be affine as well and their affine
  /// maps must match.
  if (dbAffineMap && opAffineMap) {
    /// Compare the results of the affine maps up to the number of results of
    /// the datablock.
    const uint32_t dbNumResults = 0;
    for (uint32_t i = 0; i < dbNumResults; ++i) {
      if (dbAffineMap->getResult(i) != opAffineMap->getResult(i))
        return false;
    }
  }
  /// If only one of the maps exists, they do not match.
  else if (dbAffineMap || opAffineMap) {
    return false;
  }

  /// Compare pinned indices.
  for (uint32_t i = 0; i < dbIndicesSize; ++i) {
    if (opIndices[i] != dbIndices[i])
      return false;
  }

  LLVM_DEBUG(dbgs() << "    - " << op << "\n");
  return true;
}

//===----------------------------------------------------------------------===//
// Pass Implementation
//===----------------------------------------------------------------------===//
namespace {
struct DatablockPass : public arts::DatablockBase<DatablockPass> {
  void runOnOperation() override;
};
} // end anonymous namespace

void DatablockPass::runOnOperation() {
  ModuleOp module = getOperation();
  LLVM_DEBUG(dbgs() << "\n" << line << "DatablockPass STARTED\n" << line);
  OpBuilder builder(module);

  /// Retrieve the shared analysis result.
  DatablockAnalysis &dbAnalysis = getAnalysis<DatablockAnalysis>();

  /// Iterate over every function in the module.
  module->walk([&](func::FuncOp func) {
    /// Retrieve (or compute) the dependency graph for this function.
    auto &graph = dbAnalysis.getOrCreateGraph(func);
    if (graph.edges.empty())
      return;
    dbAnalysis.printGraph(func);
  });

  // module.walk([&](EdtOp edt) {
  //   auto &region = edt.getRegion();
  //   for (auto dep : edt.getDependencies()) {
  //     auto dbOp = cast<DataBlockOp>(dep.getDefiningOp());
  //     adjustDataBlock(dbOp, region);
  //   }
  // });

  LLVM_DEBUG(dbgs() << line << "DatablockPass FINISHED\n" << line);
}

///===----------------------------------------------------------------------===///
/// Pass creation
///===----------------------------------------------------------------------===///
namespace mlir {
namespace arts {
std::unique_ptr<Pass> createDatablockPass() {
  return std::make_unique<DatablockPass>();
}
} // namespace arts
} // namespace mlir
