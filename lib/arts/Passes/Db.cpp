//===----------------------------------------------------------------------===//
// Db/DbPass.cpp - Pass for DB optimizations
//===----------------------------------------------------------------------===//

/// Dialects
#include "arts/Codegen/ArtsIR.h"
#include "arts/Utils/ArtsUtils.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/AffineMap.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"
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
#define LINE "-----------------------------------------\n"
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

  /// DBs with single size, and "in" mode, can be converted into parameters.
  bool convertToParameters();

  /// Analyze DB usage and resize it to the minimum required based on min/max indices from acquires/releases.
  bool shrinkDb();

  /// Remove unused DB allocations.
  bool deadDbElimination();

  /// Reuse buffers for non-overlapping DB lifetimes.
  bool bufferReuse();

  /// Hoist allocations out of loops if invariant.
  bool hoistAllocs();

  /// Fuse consecutive acquires/releases on the same alloc if no intervening ops.
  bool fuseAccesses();

  /// Inline small DB allocs into acquires if single-use.
  bool inlineAllocs();

private:
  ModuleOp module;
};
} // end anonymous namespace

void DbPass::runOnOperation() {
  bool changed = false;
  module = getOperation();
  changed |= canonicalizeDimOps();

  LLVM_DEBUG({
    dbgs() << "\n" << LINE << "DbPass STARTED\n" << LINE;
    module.dump();
  });

  auto &dbAnalysis = getAnalysis<DbAnalysis>();
  module.walk([&](func::FuncOp func) {
    auto graph = dbAnalysis.getOrCreateGraph(func);
    if (graph->getNumNodes() == 0) return;

    std::string filename = "DbGraph_" + func.getName().str() + ".dot";
    std::error_code EC;
    llvm::raw_fd_ostream dotFile(filename, EC);
    if (!EC) {
      graph->exportToDot(dotFile);
      LLVM_DEBUG(DBGS() << "Exported dot graph to: " << filename << "\n");
    } else {
      LLVM_DEBUG(DBGS() << "Failed to create dot file: " << filename << "\n");
    }
  });

  changed |= deadDbElimination();
  changed |= convertToParameters();
  changed |= shrinkDb();
  changed |= bufferReuse();
  changed |= hoistAllocs();
  changed |= fuseAccesses();
  changed |= inlineAllocs();

  if (!changed) {
    LLVM_DEBUG(DBGS() << "No changes made to the module\n");
    markAnalysesPreserved<DbAnalysis>();
  }

  LLVM_DEBUG({
    dbgs() << LINE << "DbPass FINISHED\n" << LINE;
    module.dump();
  });
}

bool DbPass::canonicalizeDimOps() {
  bool changed = false;
  module.walk([&](Operation *op) {
    if (auto dimOp = dyn_cast<memref::DimOp>(op)) {
      Value source = dimOp.getSource();
      if (auto dbOp = source.getDefiningOp<DbAllocOp>()) {
        if (auto constIndex = dimOp.getConstantIndex()) {
          Value size = dbOp.getSizes()[*constIndex];
          dimOp.replaceAllUsesWith(size);
          dimOp.erase();
          changed = true;
        }
      } else if (auto acquireOp = source.getDefiningOp<DbAcquireOp>()) {
        DbAllocOp parent = dbAnalysis.getParentAlloc(acquireOp);
        if (parent && auto constIndex = dimOp.getConstantIndex()) {
          Value size = parent.getSizes()[*constIndex];
          dimOp.replaceAllUsesWith(size);
          dimOp.erase();
          changed = true;
        }
      } else if (auto releaseOp = source.getDefiningOp<DbReleaseOp>()) {
        DbAllocOp parent = dbAnalysis.getParentAlloc(releaseOp);
        if (parent && auto constIndex = dimOp.getConstantIndex()) {
          Value size = parent.getSizes()[*constIndex];
          dimOp.replaceAllUsesWith(size);
          dimOp.erase();
          changed = true;
        }
      }
    }
  });
  return changed;
}

bool DbPass::convertToParameters() {
  bool changed = false;
  module.walk([&](func::FuncOp func) {
    auto graph = dbAnalysis.getOrCreateGraph(func);
    graph->forEachAllocNode([&](DbAllocNode *allocNode) {
      bool hasSingleSize = (allocNode->getSizes().size() == 1);
      bool isOnlyReader = (allocNode->getAcquireNodesSize() > 0 && allocNode->getReleaseNodesSize() == 0);

      if (!hasSingleSize || !isOnlyReader) return;

      LLVM_DEBUG(dbgs() << "- Converting DB to parameter: " << allocNode->getDbAllocOp() << "\n");
      allocNode->forEachChildNode([&](NodeBase *child) {
        if (auto acquireNode = dyn_cast<DbAcquireNode>(child)) {
          Operation *acquireOp = acquireNode->getOp();
          acquireOp->getResult(0).replaceAllUsesWith(allocNode->getPtr());
          acquireOp->erase();
        }
      });
      if (allocNode->getDbAllocOp().use_empty()) {
        allocNode->getDbAllocOp().erase();
      }
      changed = true;
    });
  });
  return changed;
}

bool DbPass::shrinkDb() {
  bool changed = false;
  module.walk([&](func::FuncOp func) {
    auto graph = dbAnalysis.getOrCreateGraph(func);
    graph->forEachAllocNode([&](DbAllocNode *allocNode) {
      DbAllocOp dbOp = allocNode->getDbAllocOp();
      Location loc = dbOp.getLoc();
      auto rank = dbOp.getSizes().size();

      SmallVector<unsigned> dimsToShrink;
      SmallVector<Value> offsets(rank);
      SmallVector<Value> newSizes(rank);
      bool needsShrinking = false;

      OpBuilder builder(dbOp);
      builder.setInsertionPoint(dbOp);

      LLVM_DEBUG(dbgs() << "Analyzing DB: " << dbOp << "\n");
      for (unsigned i = 0; i < rank; ++i) {
        auto dimAnalysis = dbAnalysis.getDimensionAnalysis(*allocNode);
        int64_t minDim = dimAnalysis[i].overallPattern.conservativeMin.constant;
        int64_t maxDim = dimAnalysis[i].overallPattern.conservativeMax.constant;
        int64_t originalSize = dbOp.getSizes()[i].getDefiningOp<ConstantIndexOp>() ? dbOp.getSizes()[i].getDefiningOp<ConstantIndexOp>().value() : -1;

        bool shrinkMin = (minDim > 0);
        bool shrinkMax = (maxDim < originalSize && originalSize != -1);
        bool shrinkThisDim = shrinkMin || shrinkMax;

        if (shrinkThisDim) {
          needsShrinking = true;
          dimsToShrink.push_back(i);
          offsets[i] = builder.create<arith::ConstantIndexOp>(loc, minDim);
          newSizes[i] = builder.create<arith::SubIOp>(loc, builder.create<arith::ConstantIndexOp>(loc, maxDim), offsets[i]);
        } else {
          offsets[i] = builder.create<arith::ConstantIndexOp>(loc, 0);
          newSizes[i] = dbOp.getSizes()[i];
        }
      }

      if (!needsShrinking) return;

      LLVM_DEBUG(dbgs() << "- Shrinking DB\n");
      changed = true;

      auto newDbOp = builder.create<DbAllocOp>(loc, dbOp.getMode(), dbOp.getPtr(), newSizes);
      for (auto attr : dbOp->getAttrs()) {
        if (attr.getName() != "operandSegmentSizes") {
          newDbOp->setAttr(attr.getName(), attr.getValue());
        }
      }

      allocNode->forEachChildNode([&](NodeBase *child) {
        Operation *accessOp = child->getOp();
        SmallVector<Value> oldIndices;
        if (auto acquireOp = dyn_cast<DbAcquireOp>(accessOp)) {
          oldIndices = acquireOp.getIndices();
        } else if (auto releaseOp = dyn_cast<DbReleaseOp>(accessOp)) {
          oldIndices = releaseOp.getIndices();
        }
        SmallVector<Value> newIndices = oldIndices;
        for (unsigned j = 0; j < rank; ++j) {
          if (offsets[j]) {
            newIndices[j] = builder.create<arith::SubIOp>(loc, oldIndices[j], offsets[j]);
          }
        }
        if (auto acquireOp = dyn_cast<DbAcquireOp>(accessOp)) {
          auto newAcquire = builder.create<DbAcquireOp>(loc, newDbOp.getResult(0), newIndices);
          acquireOp.replaceAllUsesWith(newAcquire.getResult());
          acquireOp.erase();
        } else if (auto releaseOp = dyn_cast<DbReleaseOp>(accessOp)) {
          auto newRelease = builder.create<DbReleaseOp>(loc, newDbOp.getResult(0), newIndices);
          releaseOp.replaceAllUsesWith(newRelease.getResult());
          releaseOp.erase();
        }
      });

      dbOp.replaceAllUsesWith(newDbOp.getResults());
      dbOp.erase();
    });
  });
  return changed;
}

bool DbPass::deadDbElimination() {
  bool changed = false;
  module.walk([&](func::FuncOp func) {
    auto graph = dbAnalysis.getOrCreateGraph(func);
    SmallVector<DbAllocNode *> deadAllocs;
    graph->forEachAllocNode([&](DbAllocNode *allocNode) {
      if (allocNode->getAcquireNodesSize() == 0 && allocNode->getReleaseNodesSize() == 0) {
        deadAllocs.push_back(allocNode);
      }
    });
    for (auto *allocNode : deadAllocs) {
      allocNode->getDbAllocOp().erase();
      changed = true;
    }
  });
  return changed;
}

bool DbPass::bufferReuse() {
  bool changed = false;
  module.walk([&](func::FuncOp func) {
    auto graph = dbAnalysis.getOrCreateGraph(func);
    SmallVector<std::pair<DbAllocNode *, DbAllocNode *>> reusePairs;
    graph->forEachAllocNode([&](DbAllocNode *alloc1) {
      graph->forEachAllocNode([&](DbAllocNode *alloc2) {
        if (alloc1 != alloc2 && !graph->isAllocReachable(alloc1->getDbAllocOp(), alloc2->getDbAllocOp()) &&
            !graph->isAllocReachable(alloc2->getDbAllocOp(), alloc1->getDbAllocOp()) &&
            dbAnalysis.getAliasAnalysis()->mayAlias(alloc1->getDbAllocOp(), alloc2->getDbAllocOp())) {
          reusePairs.push_back({alloc1, alloc2});
        }
      });
    });
    for (auto &[alloc1, alloc2] : reusePairs) {
      alloc2->forEachChildNode([&](NodeBase *child) {
        if (auto acquire = dyn_cast<DbAcquireNode>(child)) {
          acquire->getOp()->setOperand(0, alloc1->getDbAllocOp().getResult(0));
        } else if (auto release = dyn_cast<DbReleaseNode>(child)) {
          release->getOp()->setOperand(0, alloc1->getDbAllocOp().getResult(0));
        }
      });
      alloc2->getDbAllocOp().erase();
      changed = true;
    }
  });
  return changed;
}

bool DbPass::hoistAllocs() {
  bool changed = false;
  module.walk([&](func::FuncOp func) {
    auto graph = dbAnalysis.getOrCreateGraph(func);
    auto *loopAnalysis = dbAnalysis.getLoopAnalysis();
    graph->forEachAllocNode([&](DbAllocNode *allocNode) {
      Operation *allocOp = allocNode->getOp();
      if (auto enclosingLoop = loopAnalysis->getEnclosingLoop(allocOp)) {
        if (loopAnalysis->isLoopInvariant(allocOp, enclosingLoop)) {
          allocOp->moveBefore(enclosingLoop);
          changed = true;
        }
      }
    });
  });
  return changed;
}

bool DbPass::fuseAccesses() {
  bool changed = false;
  module.walk([&](func::FuncOp func) {
    auto graph = dbAnalysis.getOrCreateGraph(func);
    graph->forEachAllocNode([&](DbAllocNode *allocNode) {
      SmallVector<DbAcquireNode *> acquires;
      SmallVector<DbReleaseNode *> releases;
      allocNode->forEachChildNode([&](NodeBase *child) {
        if (auto acquire = dyn_cast<DbAcquireNode>(child)) acquires.push_back(acquire);
        if (auto release = dyn_cast<DbReleaseNode>(child)) releases.push_back(release);
      });

      // Fuse consecutive acquires if no intervening ops
      for (size_t i = 0; i < acquires.size() - 1; ++i) {
        Operation *acq1 = acquires[i]->getOp();
        Operation *acq2 = acquires[i + 1]->getOp();
        if (acq1->getNextNode() == acq2 && dbAnalysis.getAliasAnalysis()->mayAlias(*acquires[i], *acquires[i + 1])) {
          acq2->replaceAllUsesWith(acq1->getResults());
          acq2->erase();
          changed = true;
        }
      }

      // Similar for releases
      for (size_t i = 0; i < releases.size() - 1; ++i) {
        Operation *rel1 = releases[i]->getOp();
        Operation *rel2 = releases[i + 1]->getOp();
        if (rel1->getNextNode() == rel2 && dbAnalysis.getAliasAnalysis()->mayAlias(*releases[i], *releases[i + 1])) {
          rel2->erase();  // Fuse by removing redundant release
          changed = true;
        }
      }
    });
  });
  return changed;
}

bool DbPass::inlineAllocs() {
  bool changed = false;
  module.walk([&](func::FuncOp func) {
    auto graph = dbAnalysis.getOrCreateGraph(func);
    graph->forEachAllocNode([&](DbAllocNode *allocNode) {
      if (allocNode->getAcquireNodesSize() + allocNode->getReleaseNodesSize() == 1) {
        NodeBase *child = nullptr;
        allocNode->forEachChildNode([&](NodeBase *c) { child = c; });
        if (child) {
          Operation *childOp = child->getOp();
          childOp->setOperand(0, allocNode->getPtr());  // Inline ptr directly
          allocNode->getDbAllocOp().erase();
          changed = true;
        }
      }
    });
  });
  return changed;
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