///==========================================================================///
/// File: Db.cpp
/// Pass for DB optimizations and memory management.
///==========================================================================///

/// Dialects

#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/BuiltinAttributes.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/Value.h"
#include "mlir/Support/LLVM.h"
/// Arts
#include "ArtsPassDetails.h"
#include "arts/Analysis/ArtsAnalysisManager.h"
#include "arts/Analysis/Graphs/Db/DbGraph.h"
#include "arts/Analysis/Loop/LoopAnalysis.h"
#include "arts/Analysis/Metadata/ArtsMetadataManager.h"
#include "arts/ArtsDialect.h"
#include "arts/Passes/ArtsPasses.h"
/// Other
#include "mlir/IR/Dominance.h"
#include "mlir/IR/OpDefinition.h"
#include "mlir/IR/Operation.h"
#include "mlir/IR/Visitors.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Support/LogicalResult.h"
/// Debug
#include "arts/Analysis/Graphs/Db/DbNode.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallVector.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iterator>
#include <limits>
/// File operations
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/raw_ostream.h"

#include "arts/Transforms/DbRewriter.h"
#include "arts/Utils/ArtsDebug.h"
#include "arts/Utils/ArtsUtils.h"
#include "arts/Utils/Metadata/ArtsMetadata.h"
#include "arts/Utils/Metadata/LoopMetadata.h"
#include "arts/Utils/Metadata/MemrefMetadata.h"
ARTS_DEBUG_SETUP(db);

using namespace mlir;
using namespace mlir::func;
using namespace mlir::arith;
using namespace mlir::polygeist;
using namespace mlir::arts;

///===----------------------------------------------------------------------===///
// Pass Implementation
// transform/optimize ARTS DB IR using DbGraph/analyses.
///===----------------------------------------------------------------------===///
namespace {
struct DbPass : public arts::DbBase<DbPass> {
  DbPass(ArtsAnalysisManager *AM, bool enablePartitioning)
      : AM(AM), enablePartitioning(enablePartitioning) {
    assert(AM && "ArtsAnalysisManager must be provided externally");
  }

  void runOnOperation() override;

  /// Optimizations
  bool adjustDbModes();
  bool refineParallelAcquires();

  /// Graph rebuild
  void invalidateAndRebuildGraph();
  bool isEligibleForSlicing(DbAcquireNode *node);
  bool edtHasParallelLoopMetadata(EdtOp edt);
  bool isMemrefParallelFriendly(DbAllocNode *allocNode);
  bool shouldSliceAlloc(DbAllocNode *allocNode);
  LogicalResult computeChunkInfo(DbAcquireNode *node, Value &chunkOffset,
                                 Value &chunkSize);
  FailureOr<DbAllocOp> promoteAllocForChunking(DbAllocOp allocOp);
  void updateAcquireForChunk(DbAcquireOp acqOp, Value chunkOffset,
                             Value chunkSize);
  LogicalResult computeChunkInfoFromWhile(scf::WhileOp whileOp,
                                          DbAcquireOp acqOp, Value &chunkOffset,
                                          Value &chunkSize);
  void collectWhileBounds(Value cond, Value iterArg,
                          SmallVector<Value> &bounds);
  Value stripIndexCasts(Value value);
  Value ensureIndexType(Value value, OpBuilder &builder, Location loc);

private:
  ModuleOp module;
  ArtsAnalysisManager *AM = nullptr;
  bool enablePartitioning = true;

  /// Helper functions
  ArtsMode inferModeFromMetadata(Operation *allocOp);
  std::optional<int64_t> suggestChunkSize(Operation *loopOp,
                                          Operation *allocOp);
};
} // namespace

void DbPass::runOnOperation() {
  bool changed = false;
  module = getOperation();

  ARTS_INFO_HEADER(DbPass);
  ARTS_DEBUG_REGION(module.dump(););

  assert(AM && "ArtsAnalysisManager must be provided externally");

  /// Import sequential metadata if available
  ArtsMetadataManager &metadataManager = AM->getMetadataManager();
  std::string metadataFile = ".carts-metadata.json";
  if (llvm::sys::fs::exists(metadataFile)) {
    ARTS_DEBUG("Importing metadata from .carts-metadata.json");
    metadataManager.importFromJsonFile(module, metadataFile);
  }

  /// Graph construction and analysis
  invalidateAndRebuildGraph();

  changed |= adjustDbModes();
  changed |= refineParallelAcquires();

  /// Phase 2: Focus on parallel EDT worker acquires
  /// After ArtsForLowering has created worker EDTs with chunk-based acquires,
  /// this phase refines the partitioning strategy
  ARTS_INFO(" - Running Phase 2 optimizations (parallel EDT refinement)");

  if (changed) {
    /// If the module has changed, adjust the db modes again
    // changed |= adjustDbModes();
    ARTS_INFO(" Module has changed, invalidating analyses");
    AM->invalidate();
  }

  ARTS_INFO_FOOTER(DbPass);
  ARTS_DEBUG_REGION(module.dump(););
}

///===----------------------------------------------------------------------===///
/// Adjust DB modes based on accesses.
/// Based on the collected loads and stores, adjust the DB mode to in, out, or
/// inout.
///===----------------------------------------------------------------------===///
bool DbPass::adjustDbModes() {
  ARTS_DEBUG_HEADER(AdjustDBModes);
  bool changed = false;

  module.walk([&](func::FuncOp func) {
    DbGraph &graph = AM->getDbGraph(func);

    /// First, adjust per-acquire modes
    graph.forEachAcquireNode([&](DbAcquireNode *acqNode) {
      bool hasLoads = !acqNode->getLoads().empty();
      bool hasStores = !acqNode->getStores().empty();

      DbAcquireOp acqOp = acqNode->getDbAcquireOp();
      ArtsMode newMode = ArtsMode::in;
      if (hasLoads && hasStores)
        newMode = ArtsMode::inout;
      else if (hasStores)
        newMode = ArtsMode::out;
      else
        newMode = ArtsMode::in;

      /// Each DbAcquireNode's mode is derived from its own accesses only.
      /// Nested acquires will be processed separately by forEachAcquireNode.
      if (newMode == acqOp.getMode())
        return;

      ARTS_DEBUG("AcquireOp: " << acqOp);
      ARTS_DEBUG(" from " << acqOp.getMode() << " to " << newMode);
      acqOp.setModeAttr(ArtsModeAttr::get(acqOp.getContext(), newMode));
      changed = true;
    });

    /// Then, adjust alloc dbMode - collect modes from all acquires in hierarchy
    /// (direct children and nested acquires) and compute maximum required mode
    graph.forEachAllocNode([&](DbAllocNode *allocNode) {
      ArtsMode maxMode = ArtsMode::in;

      /// Recursive helper to collect modes from all acquire levels
      std::function<void(DbAcquireNode *)> collectModes =
          [&](DbAcquireNode *acqNode) {
            ArtsMode mode = acqNode->getDbAcquireOp().getMode();
            maxMode = combineAccessModes(maxMode, mode);

            /// Recursively process child acquires
            acqNode->forEachChildNode([&](NodeBase *child) {
              if (auto *nestedAcq = dyn_cast<DbAcquireNode>(child))
                collectModes(nestedAcq);
            });
          };

      allocNode->forEachChildNode([&](NodeBase *child) {
        if (auto *acqNode = dyn_cast<DbAcquireNode>(child)) {
          collectModes(acqNode);
        }
      });

      DbAllocOp allocOp = allocNode->getDbAllocOp();

      /// Use metadata from allocNode's info (MemrefMetadata)
      if (allocNode->readWriteRatio) {
        double ratio = *allocNode->readWriteRatio;
        ArtsMode metadataMode = ArtsMode::inout;
        if (ratio > 0.9)
          metadataMode = ArtsMode::in;
        else if (ratio < 0.1)
          metadataMode = ArtsMode::out;
        maxMode = combineAccessModes(maxMode, metadataMode);
        ARTS_DEBUG("  Using metadata read/write ratio: " << ratio << " -> "
                                                         << metadataMode);
      }

      ArtsMode currentDbMode = allocOp.getMode();
      if (currentDbMode == maxMode)
        return;
      ARTS_DEBUG("AllocOp: " << allocOp << " from " << currentDbMode << " to "
                             << maxMode);
      allocOp.setModeAttr(ArtsModeAttr::get(allocOp.getContext(), maxMode));

      changed = true;
    });
  });

  ARTS_DEBUG_FOOTER(AdjustDBModes);
  return changed;
}

bool DbPass::refineParallelAcquires() {
  if (!enablePartitioning)
    return false;
  ARTS_DEBUG_HEADER(RefineParallelAcquires);
  bool changed = false;

  /// Collect all (alloc, acquire, offset, size) tuples
  /// This is a READ-ONLY walk - no modifications to avoid iterator invalidation
  llvm::DenseMap<Operation *,
                 SmallVector<std::tuple<DbAcquireOp, Value, Value>, 4>>
      allocsToPromote;

  module.walk([&](func::FuncOp func) {
    DbGraph &graph = AM->getDbGraph(func);

    SmallVector<DbAcquireNode *, 16> candidates;
    graph.forEachAcquireNode([&](DbAcquireNode *acqNode) {
      if (!isEligibleForSlicing(acqNode))
        return;

      DbAllocNode *allocNode = acqNode->getRootAlloc();
      if (!allocNode || !shouldSliceAlloc(allocNode))
        return;

      candidates.push_back(acqNode);
    });

    for (DbAcquireNode *node : candidates) {
      Value chunkOffset;
      Value chunkSize;
      if (failed(computeChunkInfo(node, chunkOffset, chunkSize)))
        continue;

      DbAllocNode *allocNode = node->getRootAlloc();
      if (!allocNode)
        continue;

      DbAllocOp allocOp = allocNode->getDbAllocOp();
      DbAcquireOp acqOp = node->getDbAcquireOp();

      /// Collect this (acquire, offset, size) tuple for this alloc
      allocsToPromote[allocOp.getOperation()].push_back(
          std::make_tuple(acqOp, chunkOffset, chunkSize));
    }
  });

  /// Now promote allocations and update acquires (OUTSIDE the walk)
  /// Copy to vector to avoid iterator invalidation when keys (Operation*) get
  /// erased
  SmallVector<std::pair<DbAllocOp,
                        SmallVector<std::tuple<DbAcquireOp, Value, Value>, 4>>>
      allocsToProcess;
  for (auto &[allocOpPtr, acquireList] : allocsToPromote) {
    allocsToProcess.push_back(
        {cast<DbAllocOp>(allocOpPtr), std::move(acquireList)});
  }
  allocsToPromote.clear(); /// Clear map to avoid using dangling pointers

  for (auto &[allocOp, acquireList] : allocsToProcess) {
    ARTS_DEBUG("Promoting alloc with " << acquireList.size() << " acquires");

    auto promotedOr = promoteAllocForChunking(allocOp);
    if (failed(promotedOr))
      continue;

    DbAllocOp promoted = promotedOr.value();
    if (promoted.getOperation() == allocOp.getOperation()) {
      ARTS_DEBUG("  Alloc already promoted, updating acquires");
    } else {
      /// allocOp has been erased, can't access it
      ARTS_DEBUG("  Promoted alloc to sizes=" << promoted.getSizes().size());
      changed = true;
    }

    /// Update all acquires for this alloc
    for (auto &[acqOp, chunkOffset, chunkSize] : acquireList) {
      if (!acqOp)
        continue;

      updateAcquireForChunk(acqOp, chunkOffset, chunkSize);

      /// Verify the transformation
      if (acqOp.getOffsets().empty() || acqOp.getSizes().empty()) {
        ARTS_DEBUG("  WARNING: Acquire offsets or sizes empty after update");
      } else {
        ARTS_DEBUG("  Verified: Acquire has offsets="
                   << acqOp.getOffsets().size()
                   << " sizes=" << acqOp.getSizes().size());
      }

      if (acqOp.getSourcePtr() != promoted.getPtr()) {
        ARTS_DEBUG(
            "  WARNING: Acquire source ptr doesn't match promoted alloc");
      }
    }
  }

  /// Rebuild graph if any promotions occurred
  if (changed) {
    ARTS_DEBUG("Rebuilding graph after promotions");
    invalidateAndRebuildGraph();
  }

  ARTS_DEBUG_FOOTER(RefineParallelAcquires);
  return changed;
}

bool DbPass::isEligibleForSlicing(DbAcquireNode *node) {
  DbAcquireOp acq = node->getDbAcquireOp();
  auto skip = [&](StringRef reason) {
    ARTS_DEBUG("Skipping acquire " << acq << ": " << reason);
    return false;
  };
  if (!acq)
    return false;

  EdtOp edt = node->getEdtUser();
  if (!edt || edt.getType() != EdtType::task)
    return skip("not a task EDT");

  if (node->getLoads().empty() && node->getStores().empty())
    return skip("no memory accesses");

  DbAllocNode *allocNode = node->getRootAlloc();
  if (!allocNode)
    return skip("missing allocation node");

  if (allocNode->hasUniformAccess && !*allocNode->hasUniformAccess)
    return skip("non-uniform access");

  if (!isMemrefParallelFriendly(allocNode))
    return skip("memref metadata is not marked as parallel-friendly");

  if (!edtHasParallelLoopMetadata(edt))
    return skip("worker EDT does not contain parallel loop metadata");

  return true;
}

bool DbPass::isMemrefParallelFriendly(DbAllocNode *allocNode) {
  if (!allocNode)
    return false;

  if (allocNode->isStringDatablock())
    return false;

  if (!allocNode->accessedInParallelLoop ||
      !allocNode->accessedInParallelLoop.value_or(false))
    return false;

  return true;
}

bool DbPass::shouldSliceAlloc(DbAllocNode *allocNode) {
  if (!allocNode)
    return false;

  bool canSlice = false;
  if (allocNode->accessedInParallelLoop && *allocNode->accessedInParallelLoop) {
    if (!allocNode->hasLoopCarriedDeps || !*allocNode->hasLoopCarriedDeps)
      canSlice = true;
  }

  if (allocNode->hasUniformAccess && *allocNode->hasUniformAccess) {
    if (allocNode->dominantAccessPattern) {
      auto pattern = *allocNode->dominantAccessPattern;
      if (pattern == AccessPatternType::Sequential ||
          pattern == AccessPatternType::Strided)
        canSlice = true;
    }
  }

  return canSlice;
}

LogicalResult DbPass::computeChunkInfo(DbAcquireNode *node, Value &chunkOffset,
                                       Value &chunkSize) {
  if (!node)
    return failure();

  EdtOp edt = node->getEdtUser();
  if (!edt)
    return failure();

  scf::ForOp chunkLoop;
  scf::WhileOp chunkWhile;
  edt.walk([&](scf::ForOp forOp) {
    chunkLoop = forOp;
    return WalkResult::interrupt();
  });
  edt.walk([&](scf::WhileOp whileOp) {
    chunkWhile = whileOp;
    return WalkResult::interrupt();
  });

  if (chunkWhile) {
    DbAcquireOp acqOp = node->getDbAcquireOp();
    if (!acqOp)
      return failure();
    if (succeeded(computeChunkInfoFromWhile(chunkWhile, acqOp, chunkOffset,
                                            chunkSize)))
      return success();
  }

  if (!chunkLoop)
    return failure();

  chunkSize = chunkLoop.getUpperBound();

  Value ptrArg = node->getUseInEdt();
  if (!ptrArg)
    return failure();

  Value offsetCandidate;
  for (Operation *user : ptrArg.getUsers()) {
    auto refOp = dyn_cast<DbRefOp>(user);
    if (!refOp || !chunkLoop->isAncestor(refOp))
      continue;

    Value refVal = refOp.getResult();
    for (Operation *memUser : refVal.getUsers()) {
      auto storeOp = dyn_cast<memref::StoreOp>(memUser);
      if (!storeOp || !chunkLoop->isAncestor(storeOp))
        continue;

      if (storeOp.getIndices().empty())
        continue;
      Value idx = storeOp.getIndices().front();
      auto addOp = idx.getDefiningOp<arith::AddIOp>();
      if (!addOp)
        continue;

      Value iv = chunkLoop.getInductionVar();
      if (addOp.getOperand(0) == iv)
        offsetCandidate = addOp.getOperand(1);
      else if (addOp.getOperand(1) == iv)
        offsetCandidate = addOp.getOperand(0);

      if (offsetCandidate)
        break;
    }
    if (offsetCandidate)
      break;
  }

  if (!offsetCandidate)
    return failure();

  chunkOffset = offsetCandidate;
  return success();
}

Value DbPass::stripIndexCasts(Value value) {
  if (!value)
    return value;
  if (auto cast = value.getDefiningOp<arith::IndexCastOp>())
    return stripIndexCasts(cast.getIn());
  if (auto trunc = value.getDefiningOp<arith::TruncIOp>())
    return stripIndexCasts(trunc.getIn());
  if (auto ext = value.getDefiningOp<arith::ExtSIOp>())
    return stripIndexCasts(ext.getIn());
  return value;
}

Value DbPass::ensureIndexType(Value value, OpBuilder &builder, Location loc) {
  if (!value)
    return Value();
  if (value.getType().isa<IndexType>())
    return value;
  if (auto intTy = value.getType().dyn_cast<IntegerType>())
    return builder.create<arith::IndexCastOp>(loc, builder.getIndexType(),
                                              value);
  return Value();
}

void DbPass::collectWhileBounds(Value cond, Value iterArg,
                                SmallVector<Value> &bounds) {
  if (!cond)
    return;
  cond = stripIndexCasts(cond);
  if (auto andOp = cond.getDefiningOp<arith::AndIOp>()) {
    collectWhileBounds(andOp.getLhs(), iterArg, bounds);
    collectWhileBounds(andOp.getRhs(), iterArg, bounds);
    return;
  }
  if (auto cmp = cond.getDefiningOp<arith::CmpIOp>()) {
    auto lhs = stripIndexCasts(cmp.getLhs());
    auto rhs = stripIndexCasts(cmp.getRhs());
    auto pred = cmp.getPredicate();
    auto isLessPred =
        pred == arith::CmpIPredicate::slt || pred == arith::CmpIPredicate::ult;
    if (lhs == iterArg && isLessPred) {
      bounds.push_back(cmp.getRhs());
      return;
    }
    if (rhs == iterArg && (pred == arith::CmpIPredicate::sgt ||
                           pred == arith::CmpIPredicate::ugt)) {
      bounds.push_back(cmp.getLhs());
      return;
    }
  }
}

LogicalResult DbPass::computeChunkInfoFromWhile(scf::WhileOp whileOp,
                                                DbAcquireOp acqOp,
                                                Value &chunkOffset,
                                                Value &chunkSize) {
  if (!whileOp || !acqOp)
    return failure();
  if (whileOp.getInits().size() != 1)
    return failure();

  Value initValue = whileOp.getInits().front();
  Block &before = whileOp.getBefore().front();
  auto condition = dyn_cast<scf::ConditionOp>(before.getTerminator());
  if (!condition || before.getNumArguments() != 1)
    return failure();

  SmallVector<Value> bounds;
  collectWhileBounds(condition.getCondition(), before.getArgument(0), bounds);
  if (bounds.empty())
    return failure();

  OpBuilder builder(acqOp);
  Location loc = acqOp.getLoc();
  Value startIdx = ensureIndexType(initValue, builder, loc);
  if (!startIdx)
    return failure();

  SmallVector<Value> chunkSizes;
  for (Value bound : bounds) {
    Value boundIdx = ensureIndexType(bound, builder, loc);
    if (!boundIdx)
      continue;
    chunkSizes.push_back(
        builder.create<arith::SubIOp>(loc, boundIdx, startIdx));
  }

  if (chunkSizes.empty())
    return failure();

  Value minSize = chunkSizes.front();
  for (Value candidate : llvm::drop_begin(chunkSizes)) {
    minSize = builder.create<arith::MinSIOp>(loc, minSize, candidate);
  }
  Value zero = builder.create<arith::ConstantIndexOp>(loc, 0);
  minSize = builder.create<arith::MaxSIOp>(loc, minSize, zero);

  chunkOffset = startIdx;
  chunkSize = minSize;
  return success();
}

FailureOr<DbAllocOp> DbPass::promoteAllocForChunking(DbAllocOp allocOp) {
  if (!allocOp || !allocOp.getOperation())
    return failure();

  ValueRange elementSizes = allocOp.getElementSizes();
  ValueRange sizes = allocOp.getSizes();
  if (elementSizes.empty())
    return failure();

  bool elementsAreUnit = llvm::all_of(elementSizes.drop_front(), [](Value v) {
    int64_t cst;
    return arts::getConstantIndex(v, cst) && cst == 1;
  });
  bool alreadyPromoted = !sizes.empty() && !elementsAreUnit;
  if (alreadyPromoted)
    return allocOp;

  OpBuilder builder(allocOp);
  Location loc = allocOp.getLoc();
  Value one = builder.create<arith::ConstantIndexOp>(loc, 1);

  SmallVector<Value> newSizes;
  for (Value size : sizes) {
    int64_t cst;
    if (newSizes.empty() && arts::getConstantIndex(size, cst) && cst == 1)
      continue;
    newSizes.push_back(size);
  }
  newSizes.push_back(elementSizes.front());

  SmallVector<Value> newElementSizes;
  newElementSizes.append(elementSizes.begin() + 1, elementSizes.end());
  if (newElementSizes.empty())
    newElementSizes.push_back(one);

  Value address = allocOp.getAddress();
  auto newAlloc = builder.create<DbAllocOp>(
      loc, allocOp.getMode(), allocOp.getRoute(), allocOp.getAllocType(),
      allocOp.getDbMode(), allocOp.getElementType(), address, newSizes,
      newElementSizes);
  for (auto attr : allocOp->getAttrs()) {
    if (attr.getName().getValue() == "operandSegmentSizes")
      continue;
    newAlloc->setAttr(attr.getName(), attr.getValue());
  }

  DbRewriter::Config rewriterConfig;
  rewriterConfig.preserveSemantics = false;
  DbRewriter rewriter(module.getContext(), rewriterConfig);
  if (failed(rewriter.rewriteAllUses(allocOp, newAlloc))) {
    newAlloc.erase();
    return failure();
  }

  return newAlloc;
}

void DbPass::updateAcquireForChunk(DbAcquireOp acqOp, Value chunkOffset,
                                   Value chunkSize) {
  if (!acqOp)
    return;

  SmallVector<Value> offsets{chunkOffset};
  SmallVector<Value> sizes{chunkSize};
  acqOp.getOffsetsMutable().assign(offsets);
  acqOp.getSizesMutable().assign(sizes);
}

bool DbPass::edtHasParallelLoopMetadata(EdtOp edt) {
  if (!edt)
    return false;

  bool foundParallelLoop = false;
  auto &dbAnalysis = AM->getDbAnalysis();
  auto *loopAnalysis = dbAnalysis.getLoopAnalysis();
  if (!loopAnalysis)
    return false;
  ArtsMetadataManager &metadataManager = AM->getMetadataManager();

  edt.walk([&](Operation *op) {
    if (auto forOp = dyn_cast<scf::ForOp>(op)) {
      metadataManager.ensureLoopMetadata(forOp);

      /// Validate that metadata was recovered
      if (auto loopAttr = forOp->getAttr(AttrNames::LoopMetadata::Name)) {
        ARTS_DEBUG("  Found loop metadata on scf.for in EDT");
      } else {
        ARTS_DEBUG("  No loop metadata found on scf.for in EDT");
      }

      if (LoopNode *loopNode = loopAnalysis->getOrCreateLoopNode(forOp)) {
        bool noDeps = !loopNode->hasInterIterationDeps.has_value() ||
                      !loopNode->hasInterIterationDeps.value();
        if (loopNode->potentiallyParallel && noDeps) {
          ARTS_DEBUG(
              "  Loop is parallel-friendly: potentiallyParallel=true, noDeps="
              << noDeps);
          foundParallelLoop = true;
          return WalkResult::interrupt();
        } else {
          ARTS_DEBUG(
              "  Loop is NOT parallel-friendly: potentiallyParallel="
              << loopNode->potentiallyParallel << ", hasInterIterationDeps="
              << (loopNode->hasInterIterationDeps.has_value()
                      ? (loopNode->hasInterIterationDeps.value() ? "true"
                                                                 : "false")
                      : "unknown"));
        }
      }
    }
    return WalkResult::advance();
  });

  return foundParallelLoop;
}

///===----------------------------------------------------------------------===///
/// Invalidate and rebuild the graph
///===----------------------------------------------------------------------===///
void DbPass::invalidateAndRebuildGraph() {
  module.walk([&](func::FuncOp func) {
    AM->invalidateFunction(func);
    (void)AM->getDbGraph(func);
  });
}

////===----------------------------------------------------------------------===////
/// Pass creation
////===----------------------------------------------------------------------===////
namespace mlir {
namespace arts {
std::unique_ptr<Pass> createDbPass(ArtsAnalysisManager *AM,
                                   bool enablePartitioning) {
  return std::make_unique<DbPass>(AM, enablePartitioning);
}
} // namespace arts
} // namespace mlir
