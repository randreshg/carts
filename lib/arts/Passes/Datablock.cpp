///==========================================================================
/// File: DataBlock.cpp
///==========================================================================

/// Dialects
#include "arts/Codegen/ArtsIR.h"
#include "arts/Utils/ArtsUtils.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/AffineMap.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/MLIRContext.h"
#include "mlir/IR/PatternMatch.h"
#include "mlir/IR/Value.h"
#include "mlir/Support/LLVM.h"
/// Arts
#include "ArtsPassDetails.h"
#include "arts/Analysis/DataBlockAnalysis.h"
#include "arts/ArtsDialect.h"
#include "arts/Passes/ArtsPasses.h"
#include "arts/Utils/ArtsUtils.h"
/// Other
#include "mlir/IR/Dominance.h"
#include "mlir/IR/OpDefinition.h"
#include "mlir/IR/Operation.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Transforms/CSE.h"
#include "mlir/Transforms/DialectConversion.h"
#include "polygeist/Ops.h"
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

//===----------------------------------------------------------------------===//
// Pass Implementation
//===----------------------------------------------------------------------===//
namespace {
struct DatablockPass : public arts::DatablockBase<DatablockPass> {
  void runOnOperation() override;

  /// Canonicalize memref.dim ops of datablocks.
  bool canonicalizeDimOps();

  /// Datablocks with single size, and result type different to a memref, can
  /// be converted into parameter.
  bool convertToParameters();

  /// Analyze datablock usage and resize it to the minimum
  /// required based on the min/max access indices found by the analysis.
  /// It also adjusts the indices in load/store operations to account for
  /// the new base offset if the minimum access index was greater than 0.
  bool shrinkDatablock();

private:
  ModuleOp module;
  DatablockAnalysis *dbAnalysis;
};
} // end anonymous namespace

void DatablockPass::runOnOperation() {
  bool changed = false;
  module = getOperation();
  /// Canonicalize dim ops of datablocks.
  canonicalizeDimOps();

  /// Print the module before the pass.
  LLVM_DEBUG({
    dbgs() << "\n" << line << "DatablockPass STARTED\n" << line;
    module.dump();
  });

  /// Retrieve the shared analysis result.
  dbAnalysis = &getAnalysis<DatablockAnalysis>();

  changed |= convertToParameters();
  changed |= shrinkDatablock();

  /// Preserve analysis results if no changes were made.
  if (!changed) {
    LLVM_DEBUG(DBGS() << "No changes made to the module\n");
    markAnalysesPreserved<DatablockAnalysis>();
  }

  LLVM_DEBUG({
    dbgs() << line << "DatablockPass FINISHED\n" << line;
    module.dump();
  });
}

bool DatablockPass::convertToParameters() {
  auto convertDbToParam = [&](DatablockGraph *graph) -> bool {
    bool changed = false;
    auto &dbNodes = graph->getNodes();
    DenseMap<EdtOp, SetVector<DbControlOp>> dbNodesToRemove;
    SetVector<Operation *> opsToRemove;
    OpBuilder builder(module);

    LLVM_DEBUG(dbgs() << "Converting datablocks to parameters - Analyzing "
                      << dbNodes.size() << " datablocks\n");
    for (auto *dbNode : dbNodes) {
      if (!dbNode->op)
        continue;

      if (!dbNode->hasSingleSize || isa<MemRefType>(dbNode->elementType) ||
          !dbNode->isOnlyReader() || !graph->isOnlyDependentOnEntry(*dbNode))
        continue;

      LLVM_DEBUG(dbgs() << "- Converting datablock to parameter: "
                        << *dbNode->op << "\n");
      dbNodesToRemove[dbNode->edtUser].insert(dbNode->op);
      changed = true;
      /// Create a new load op for the datablock.
      builder.setInsertionPoint(dbNode->op);
      auto newLoadOp = builder.create<memref::LoadOp>(
          dbNode->op.getLoc(), dbNode->ptr, dbNode->indices);
      LLVM_DEBUG(dbgs() << "  - New load op: " << *newLoadOp << "\n");

      /// Replace all load operations with the new load op.
      for (auto &use :
           llvm::make_early_inc_range(dbNode->op.getResult().getUses())) {
        if (isa<arts::EdtOp>(use.getOwner()))
          continue;
        auto loadOp = cast<memref::LoadOp>(use.getOwner());
        loadOp.replaceAllUsesWith(newLoadOp.getResult());
        loadOp.erase();
      }
    }

    /// If no datablocks were converted, return.
    if (!changed) {
      LLVM_DEBUG(dbgs() << " - No datablocks to convert\n");
      return false;
    }
    LLVM_DEBUG(dbgs() << " - Datablock conversion - Found "
                      << dbNodesToRemove.size() << " datablocks to convert\n");

    /// Update EDT dependencies and remove replaced datablock ops.
    for (auto &entry : dbNodesToRemove) {
      auto edtOp = entry.first;
      const auto &dbSet = entry.second;
      SmallVector<Value> newDeps;
      newDeps.reserve(dbSet.size());
      auto edtDeps = edtOp.getDependencies();
      LLVM_DEBUG(dbgs() << "Analyzing edt deps - Size: " << edtDeps.size()
                        << "\n");
      for (auto dep : edtDeps) {
        auto db = cast<DbControlOp>(dep.getDefiningOp());
        if (dbSet.contains(db)) {
          opsToRemove.insert(db);
          continue;
        }
        newDeps.push_back(dep);
      }

      builder.setInsertionPoint(edtOp);
      auto newEdtOp =
          createEdtOp(builder, edtOp.getLoc(), getEdtType(edtOp), newDeps);
      newEdtOp.getRegion().takeBody(edtOp.getRegion());
      opsToRemove.insert(edtOp);
    }

    LLVM_DEBUG(dbgs() << "Removing datablocks " << opsToRemove.size() << "\n");
    removeOps(module, builder, opsToRemove);

    return changed;
  };

  bool changed = false;
  module->walk([&](func::FuncOp func) {
    auto *graph = dbAnalysis->getOrCreateGraph(func);
    if (!graph || !graph->hasNodes())
      return;
    if (convertDbToParam(graph)) {
      changed = true;
      dbAnalysis->invalidateGraph(func);
    }
  });
  return changed;
}

bool DatablockPass::shrinkDatablock() {
  /// Helper lambda to get an MLIR Value from ValueOrInt.
  auto getValue = [&](ValueOrInt val, Operation *contextOp,
                      OpBuilder &builder) -> Value {
    if (val.isValue) {
      /// Cast the Value to an Index type if needed.
      Value v = val.v_val;
      if (v.getType().isIndex()) {
        return v;
      } else {
        OpBuilder::InsertionGuard guard(builder);
        LLVM_DEBUG(dbgs() << "Converting to index: " << v << "\n");
        auto op = v.getDefiningOp();
        assert(op && "Value must have a defining operation");
        builder.setInsertionPoint(op);
        return builder.create<arith::IndexCastOp>(contextOp->getLoc(),
                                                  builder.getIndexType(), v);
      }
    }
    OpBuilder::InsertionGuard guard(builder);
    builder.setInsertionPoint(contextOp);
    /// Create a constant index op at the location of the context operation.
    auto iVal =
        builder.create<arith::ConstantIndexOp>(contextOp->getLoc(), val.i_val);
    return iVal.getResult();
  };

  /// Process a single datablock graph (typically associated with a function).
  auto shrinkDbInGraph = [&](DatablockGraph *graph) -> bool {
    bool graphChanged = false;
    auto &dbNodes = graph->getNodes();
    OpBuilder builder(module);
    /// Keep track of operations to remove after modifications.
    SetVector<Operation *> opsToRemove;
    LLVM_DEBUG(dbgs() << "Analyzing " << dbNodes.size() << " datablocks\n");
    /// Iterate through all datablock nodes identified by the analysis.
    for (auto *dbNode : dbNodes) {
      if (!dbNode->op)
        continue;

      DbControlOp dbOp = dbNode->op;
      Location loc = dbOp.getLoc();
      auto rank = dbNode->dimMax.size();
      assert(dbNode->sizes.size() == rank &&
             "Datablock sizes and dimMax must have the same rank");

      /// Determine which dimensions need shrinking and calculate offsets/new
      /// sizes.
      SmallVector<unsigned> dimsToShrink;
      SmallVector<Value> offsets(rank);
      SmallVector<Value> newSizes(rank);
      bool needsShrinking = false;

      /// Set the insertion point before the datablock op for creating
      /// constants.
      builder.setInsertionPoint(dbOp);

      LLVM_DEBUG(dbgs() << "Analyzing datablock: " << *dbOp << "\n");
      for (unsigned i = 0; i < rank; ++i) {
        ValueOrInt minDim = dbNode->dimMin[i];
        ValueOrInt maxDim = dbNode->dimMax[i];
        ValueOrInt originalSize(dbNode->sizes[i]);

        /// Get Value representations for min/max dimensions.
        Value minDimValue = getValue(minDim, dbOp, builder);
        Value maxDimValue = getValue(maxDim, dbOp, builder);

        /// Check if shrinking is needed for this dimension.
        /// Shrinking is needed if min bound > 0 or max bound < original size.
        bool shrinkMin = (minDim > 0);
        /// Check if maxDim is semantically equal to originalSize using valueCmp.
        bool shrinkMax = false;
        if(!originalSize.isValue && !maxDim.isValue) {
          shrinkMax = (maxDim.i_val < originalSize.i_val);
        } else if(originalSize.isValue && maxDim.isValue) {
          shrinkMax = (maxDim.v_val != originalSize.v_val);
        }
        bool shrinkThisDim = shrinkMin || shrinkMax;

        if (shrinkThisDim) {
          needsShrinking = true;
          dimsToShrink.push_back(i);
          /// Calculate the new size: maxDim - minDim
          offsets[i] = minDimValue;
          newSizes[i] =
              builder.create<arith::SubIOp>(loc, maxDimValue, minDimValue);
        } else {
          /// If not shrinking, the offset is 0 and size remains the same.
          offsets[i] = minDimValue;
          newSizes[i] = dbNode->sizes[i];
        }
      }

      /// If no dimensions require shrinking, continue to the next datablock.
      if (!needsShrinking)
        continue;

      LLVM_DEBUG(dbgs() << "- Shrinking datablock \n");
      graphChanged = true;

      /// Create the new DbControlOp with adjusted sizes and offsets.
      auto newDbOp = builder.create<arts::DbControlOp>(
          loc, dbOp.getType(), dbOp.getMode(), dbOp.getPtr(),
          dbOp.getElementType(), dbOp.getElementTypeSize(), dbOp.getIndices(),
          offsets, newSizes);
      /// Copy all attributes except "operandSegmentSizes".
      for (auto attr : dbOp->getAttrs()) {
        if (attr.getName() != "operandSegmentSizes")
          newDbOp->setAttr(attr.getName(), attr.getValue());
      }

      /// Update users (loads/stores) to use the new datablock and adjusted
      /// indices.
      SmallVector<Operation *> usersToUpdate(dbNode->loadsAndStores.begin(),
                                             dbNode->loadsAndStores.end());
      for (Operation *user : usersToUpdate) {
        builder.setInsertionPoint(user);
        Location userLoc = user->getLoc();

        /// Adjust indices for load operations.
        if (auto loadOp = dyn_cast<memref::LoadOp>(user)) {
          SmallVector<Value> oldIndices = loadOp.getIndices();
          SmallVector<Value> newIndices = oldIndices;
          /// Subtract offset for dimensions that were shrunk from non-zero min.
          for (unsigned i = 0; i < rank; ++i) {
            if (offsets[i]) {
              newIndices[i] = builder.create<arith::SubIOp>(
                  userLoc, oldIndices[i], offsets[i]);
            }
          }
          /// Create the new load using the new datablock and adjusted indices.
          auto newLoad = builder.create<memref::LoadOp>(
              userLoc, newDbOp.getResult(), newIndices);
          loadOp.replaceAllUsesWith(newLoad.getResult());
          opsToRemove.insert(loadOp);
        }
        /// Adjust indices for store operations.
        else if (auto storeOp = dyn_cast<memref::StoreOp>(user)) {
          SmallVector<Value> oldIndices = storeOp.getIndices();
          SmallVector<Value> newIndices = oldIndices;
          /// Subtract offset for dimensions that were shrunk from non-zero min.
          for (unsigned i = 0; i < rank; ++i) {
            if (offsets[i]) {
              newIndices[i] = builder.create<arith::SubIOp>(
                  userLoc, oldIndices[i], offsets[i]);
            }
          }
          /// Create the new store using the new datablock and adjusted indices.
          builder.create<memref::StoreOp>(userLoc, storeOp.getValue(),
                                          newDbOp.getResult(), newIndices);
          opsToRemove.insert(storeOp);
        }
      }

      /// Replace all remaining uses of the old datablock with the new one.
      dbOp.getResult().replaceAllUsesWith(newDbOp.getResult());

      /// Mark the original DbControlOp for removal.
      opsToRemove.insert(dbOp);
    }

    removeOps(module, builder, opsToRemove);
    return graphChanged;
  };

  bool changedOverall = false;
  /// Iterate through all functions in the module.
  module->walk([&](func::FuncOp func) {
    /// Get the datablock analysis results for the current function.
    auto *graph = dbAnalysis->getOrCreateGraph(func);
    if (!graph || !graph->hasNodes())
      return;

    /// Attempt to shrink datablocks within this function's graph.
    LLVM_DEBUG({
      DBGS() << "Shrinking datablocks in function: " << func.getName() << "\n";
      graph->print();
    });
    if (shrinkDbInGraph(graph)) {
      changedOverall = true;
      dbAnalysis->invalidateGraph(func);
    }
  });
  return changedOverall;
}

bool DatablockPass::canonicalizeDimOps() {
  LLVM_DEBUG(dbgs() << line << "Canonicalizing dim ops\n");
  module->walk([&](arts::DbControlOp dbOp) {
    /// Analyze uses of the datablock.
    for (auto *op : dbOp->getUsers()) {
      auto dimOp = dyn_cast<memref::DimOp>(op);
      if (!dimOp)
        continue;

      LLVM_DEBUG(dbgs() << "- Canonicalizing dim op: " << *dimOp << "\n");
      auto dim = dimOp.getConstantIndex();
      assert(dim && "Dim op must have a constant index");

      /// Replace the dim op result with the size of the datablock.
      dimOp.replaceAllUsesWith(dbOp.getSizes()[*dim]);
      dimOp.erase();
    }
  });
  LLVM_DEBUG(dbgs() << line);
  return false;
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
