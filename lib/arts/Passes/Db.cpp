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
#include "arts/Analysis/Db/DbAnalysis.h"
#include "arts/Analysis/Db/Graph/DbGraph.h"
#include "arts/Analysis/Db/Graph/DbNode.h"

#define DEBUG_TYPE "db"
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
struct DbPass : public arts::DbBase<DbPass> {
  void runOnOperation() override;

  /// Canonicalize memref.dim ops of datablocks.
  bool canonicalizeDimOps();

  /// Dbs with single size, and result type different to a memref, can
  /// be converted into parameter.
  bool convertToParameters();

  /// Analyze db usage and resize it to the minimum
  /// required based on the min/max access indices found by the analysis.
  /// It also adjusts the indices in load/store operations to account for
  /// the new base offset if the minimum access index was greater than 0.
  bool shrinkDb();

private:
  ModuleOp module;
};
} // end anonymous namespace

void DbPass::runOnOperation() {
  bool changed = false;
  module = getOperation();
  /// Canonicalize dim ops of datablocks.
  canonicalizeDimOps();

  /// Print the module before the pass.
  LLVM_DEBUG({
    dbgs() << "\n" << line << "DbPass STARTED\n" << line;
    module.dump();
  });

  /// Retrieve the shared analysis result.
  auto &dbAnalysis = getAnalysis<DbAnalysis>();
  module.walk([&](func::FuncOp func) {
    dbAnalysis.getOrCreateGraph(func);
  });
  // mlir::arts::DbGraph graph(module, nullptr);
  // graph.build();

  // changed |= convertToParameters();
  // changed |= shrinkDb();

  /// Preserve analysis results if no changes were made.
  if (!changed) {
    LLVM_DEBUG(DBGS() << "No changes made to the module\n");
    markAnalysesPreserved<DbAnalysis>();
  }

  LLVM_DEBUG({
    dbgs() << line << "DbPass FINISHED\n" << line;
    module.dump();
  });
}

bool DbPass::convertToParameters() {
  // auto convertDbToParam = [&](DbGraph *graph) -> bool {
  //   bool changed = false;
  //   graph->forEachAllocNode([&](mlir::arts::DbAllocNode *allocNode) {
  //     if (allocNode->getMode().empty())
  //       return;

  //     // Check for single size
  //     bool hasSingleSize = (allocNode->getSizes().size() == 1 /* TODO: check value == 1 if possible */);
  //     // Only reader
  //     bool isOnlyReader = (allocNode->getMode() == "in");
  //     // Only writer
  //     bool isOnlyWriter = (allocNode->getMode() == "out");
  //     // Inout
  //     bool isInOut = (allocNode->getMode() == "inout");

  //     // Remove elementType and isOnlyDependentOnEntry checks for now
  //     if (!hasSingleSize || !isOnlyReader)
  //       return;

  //     LLVM_DEBUG(dbgs() << "- Converting db to parameter: "
  //                       << allocNode->getDbCreateOp() << "\n");
  //     allocNode->forEachAccessNode([&](mlir::arts::DbAccessNode *accessNode) {
  //       auto *op = accessNode->subviewOp.getOperation();
  //       if (isa<arts::EdtOp>(op))
  //         return;
  //       auto loadOp = dyn_cast<memref::LoadOp>(op);
  //       if (loadOp) {
  //         loadOp.replaceAllUsesWith(allocNode->getPtr());
  //         loadOp.erase();
  //       }
  //     });
  //     changed = true;
  //   });

  //   /// If no datablocks were converted, return.
  //   if (!changed) {
  //     LLVM_DEBUG(dbgs() << " - No datablocks to convert\n");
  //     return false;
  //   }
  //   LLVM_DEBUG(dbgs() << " - Db conversion - Found "
  //                     << " datablocks to convert\n");

  //   return changed;
  // };

  // bool changed = false;
  // module->walk([&](func::FuncOp func) {
  //   mlir::arts::DbGraph graph(func, nullptr);
  //   graph.build();
  //   if (convertDbToParam(&graph)) {
  //     changed = true;
  //   }
  // });
  // return changed;
  return false;
}

bool DbPass::shrinkDb() {
  // /// Helper lambda to get an MLIR Value from ValueOrInt.
  // auto getValue = [&](ValueOrInt val, Operation *contextOp,
  //                     OpBuilder &builder) -> Value {
  //   if (val.isValue) {
  //     /// Cast the Value to an Index type if needed.
  //     Value v = val.v_val;
  //     if (v.getType().isIndex()) {
  //       return v;
  //     } else {
  //       OpBuilder::InsertionGuard guard(builder);
  //       LLVM_DEBUG(dbgs() << "Converting to index: " << v << "\n");
  //       auto op = v.getDefiningOp();
  //       assert(op && "Value must have a defining operation");
  //       builder.setInsertionPoint(op);
  //       return builder.create<arith::IndexCastOp>(contextOp->getLoc(),
  //                                                 builder.getIndexType(), v);
  //     }
  //   }
  //   OpBuilder::InsertionGuard guard(builder);
  //   builder.setInsertionPoint(contextOp);
  //   /// Create a constant index op at the location of the context operation.
  //   auto iVal =
  //       builder.create<arith::ConstantIndexOp>(contextOp->getLoc(), val.i_val);
  //   return iVal.getResult();
  // };

  // /// Process a single db graph (typically associated with a function).
  // auto shrinkDbInGraph = [&](DbGraph *graph) -> bool {
  //   bool graphChanged = false;
  //   graph->forEachAllocNode([&](mlir::arts::DbAllocNode *allocNode) {
  //     if (allocNode->getMode().empty())
  //       return;

  //     DbCreateOp dbOp = allocNode->getDbCreateOp();
  //     Location loc = dbOp.getLoc();
  //     auto rank = allocNode->getSizes().size();
  //     assert(allocNode->getSizes().size() == rank &&
  //            "Db sizes and dimMax must have the same rank");

  //     /// Determine which dimensions need shrinking and calculate offsets/new
  //     /// sizes.
  //     SmallVector<unsigned> dimsToShrink;
  //     SmallVector<Value> offsets(rank);
  //     SmallVector<Value> newSizes(rank);
  //     bool needsShrinking = false;

  //     /// Set the insertion point before the db op for creating
  //     /// constants.
  //     OpBuilder builder(module);
  //     builder.setInsertionPoint(dbOp);

  //     LLVM_DEBUG(dbgs() << "Analyzing db: " << *dbOp << "\n");
  //     for (unsigned i = 0; i < rank; ++i) {
  //       ValueOrInt minDim = allocNode->getIndices()[i];
  //       ValueOrInt maxDim = allocNode->getSizes()[i];
  //       ValueOrInt originalSize(allocNode->getSizes()[i]);

  //       /// Get Value representations for min/max dimensions.
  //       Value minDimValue = getValue(minDim, dbOp, builder);
  //       Value maxDimValue = getValue(maxDim, dbOp, builder);

  //       /// Check if shrinking is needed for this dimension.
  //       /// Shrinking is needed if min bound > 0 or max bound < original size.
  //       bool shrinkMin = (!minDim.isValue && minDim.i_val > 0);
  //       /// Check if maxDim is semantically equal to originalSize using valueCmp.
  //       bool shrinkMax = false;
  //       if(!originalSize.isValue && !maxDim.isValue) {
  //         shrinkMax = (maxDim.i_val < originalSize.i_val);
  //       } else if(originalSize.isValue && maxDim.isValue) {
  //         shrinkMax = (maxDim.v_val != originalSize.v_val);
  //       }
  //       bool shrinkThisDim = shrinkMin || shrinkMax;

  //       if (shrinkThisDim) {
  //         needsShrinking = true;
  //         dimsToShrink.push_back(i);
  //         /// Calculate the new size: maxDim - minDim
  //         offsets[i] = minDimValue;
  //         newSizes[i] =
  //             builder.create<arith::SubIOp>(loc, maxDimValue, minDimValue);
  //       } else {
  //         /// If not shrinking, the offset is 0 and size remains the same.
  //         offsets[i] = minDimValue;
  //         newSizes[i] = allocNode->getSizes()[i];
  //       }
  //     }

  //     /// If no dimensions require shrinking, continue to the next db.
  //     if (!needsShrinking)
  //       return;

  //     LLVM_DEBUG(dbgs() << "- Shrinking db \n");
  //     graphChanged = true;

  //     /// Create the new DbControlOp with adjusted sizes and offsets.
  //     auto newDbOp = builder.create<arts::DbCreateOp>(
  //         loc,
  //         builder.getStringAttr(allocNode->getMode()),
  //         allocNode->getPtr(),
  //         newSizes
  //     );
  //     /// Copy all attributes except "operandSegmentSizes".
  //     for (auto attr : dbOp->getAttrs()) {
  //       if (attr.getName() != "operandSegmentSizes")
  //         newDbOp->setAttr(attr.getName(), attr.getValue());
  //     }

  //     /// Update users (loads/stores) to use the new db and adjusted
  //     /// indices.
  //     allocNode->forEachAccessNode([&](mlir::arts::DbAccessNode *accessNode) {
  //       if (auto loadOp = dyn_cast<memref::LoadOp>(accessNode->subviewOp.getOperation())) {
  //         SmallVector<Value> oldIndices = loadOp.getIndices();
  //         SmallVector<Value> newIndices = oldIndices;
  //         /// Subtract offset for dimensions that were shrunk from non-zero min.
  //         for (unsigned j = 0; j < rank; ++j) {
  //           if (offsets[j]) {
  //             newIndices[j] = builder.create<arith::SubIOp>(
  //                 accessNode->subviewOp.getLoc(), oldIndices[j], offsets[j]);
  //           }
  //         }
  //         /// Create the new load using the new db and adjusted indices.
  //         auto newLoad = builder.create<memref::LoadOp>(
  //             accessNode->subviewOp.getLoc(), newDbOp.getResult(0), ValueRange(newIndices));
  //         loadOp.getResult().replaceAllUsesWith(newLoad.getResult());
  //       }
  //       else if (auto storeOp = dyn_cast<memref::StoreOp>(accessNode->subviewOp.getOperation())) {
  //         SmallVector<Value> oldIndices = storeOp.getIndices();
  //         SmallVector<Value> newIndices = oldIndices;
  //         /// Subtract offset for dimensions that were shrunk from non-zero min.
  //         for (unsigned j = 0; j < rank; ++j) {
  //           if (offsets[j]) {
  //             newIndices[j] = builder.create<arith::SubIOp>(
  //                 accessNode->subviewOp.getLoc(), oldIndices[j], offsets[j]);
  //           }
  //         }
  //         /// Create the new store using the new db and adjusted indices.
  //         builder.create<memref::StoreOp>(accessNode->subviewOp.getLoc(), storeOp.getValueToStore(),
  //                                         newDbOp.getResult(0), ValueRange(newIndices));
  //       }
  //     });

  //     /// Replace all remaining uses of the old db with the new one.
  //     dbOp.getResult(0).replaceAllUsesWith(newDbOp.getResult(0));
  //     dbOp.getResult(1).replaceAllUsesWith(newDbOp.getResult(1));
  //   });

  //   return graphChanged;
  // };

  // bool changedOverall = false;
  // /// Iterate through all functions in the module.
  // module->walk([&](func::FuncOp func) {
  //   /// Get the db analysis results for the current function.
  //   mlir::arts::DbGraph graph(func, nullptr);
  //   graph.build();

  //   /// Attempt to shrink datablocks within this function's graph.
  //   LLVM_DEBUG({
  //     DBGS() << "Shrinking datablocks in function: " << func.getName() << "\n";
  //     std::ostringstream os;
  //     graph.print(os);
  //     llvm::errs() << os.str();
  //   });
  //   if (shrinkDbInGraph(&graph)) {
  //     changedOverall = true;
  //   }
  // });
  // return changedOverall;
  return false;
}

bool DbPass::canonicalizeDimOps() {
  // LLVM_DEBUG(dbgs() << line << "Canonicalizing dim ops\n");
  // module->walk([&](arts::DbControlOp dbOp) {
  //   /// Analyze uses of the db.
  //   for (auto *op : dbOp->getUsers()) {
  //     auto dimOp = dyn_cast<memref::DimOp>(op);
  //     if (!dimOp)
  //       continue;

  //     LLVM_DEBUG(dbgs() << "- Canonicalizing dim op: " << *dimOp << "\n");
  //     auto dim = dimOp.getConstantIndex();
  //     assert(dim && "Dim op must have a constant index");

  //     /// Replace the dim op result with the size of the db.
  //     dimOp.replaceAllUsesWith(dbOp.getSizes()[*dim]);
  //     dimOp.erase();
  //   }
  // });
  // LLVM_DEBUG(dbgs() << line);
  return false;
}
///===----------------------------------------------------------------------===///
/// Pass creation
///===----------------------------------------------------------------------===///
namespace mlir {
namespace arts {
std::unique_ptr<Pass> createDbPass() {
  return std::make_unique<DbPass>();
}
} // namespace arts
} // namespace mlir
