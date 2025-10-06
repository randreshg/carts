///==========================================================================
/// File: Db.cpp
/// Pass for DB optimizations and memory management.
///==========================================================================

/// Dialects

#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/Value.h"
#include "mlir/Support/LLVM.h"
/// Arts
#include "ArtsPassDetails.h"
#include "arts/Analysis/ArtsAnalysisManager.h"
#include "arts/Analysis/Graphs/Db/DbGraph.h"
#include "arts/ArtsDialect.h"
#include "arts/Passes/ArtsPasses.h"
/// Other
#include "mlir/IR/Dominance.h"
#include "mlir/IR/OpDefinition.h"
#include "mlir/IR/Operation.h"
#include "mlir/IR/Visitors.h"
#include "mlir/Pass/Pass.h"
/// Debug
#include "arts/Analysis/Graphs/Db/DbNode.h"
#include "llvm/ADT/SmallVector.h"
#include <cstdint>
/// File operations
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/raw_ostream.h"

#include "arts/Utils/ArtsDebug.h"
#include "arts/Utils/ArtsUtils.h"
ARTS_DEBUG_SETUP(db);

using namespace mlir;
using namespace mlir::func;
using namespace mlir::arith;
using namespace mlir::polygeist;
using namespace mlir::arts;

//===----------------------------------------------------------------------===//
// Pass Implementation
// transform/optimize ARTS DB IR using DbGraph/analyses.
//===----------------------------------------------------------------------===//
namespace {
struct DbPass : public arts::DbBase<DbPass> {
  DbPass(ArtsAnalysisManager *AM, bool exportJson) : AM(AM) {
    assert(AM && "ArtsAnalysisManager must be provided externally");
    this->exportJson = exportJson;
  }

  void runOnOperation() override;

  /// Adjust DB modes based on actual read/write accesses.
  bool adjustDbModes();

  /// DBs with single size, and "in" mode, can be converted into parameters.
  bool convertToParameters();

  /// Remove unused DB allocations.
  bool deadDbElimination();

  /// Export DB analysis to JSON files in output/ directory.
  void exportToJson();

  /// Reduce DB dimensionality at allocation level
  bool reduceAllocDimensionality();

  /// Tighten acquire slices inside EDTs.
  bool tightenAcquireSlices();

  /// Pin and split acquires based on invariant index patterns.
  bool pinAndSplitAcquires();

private:
  ModuleOp module;
  ArtsAnalysisManager *AM = nullptr;
};
} // namespace

void DbPass::runOnOperation() {
  bool changed = false;
  module = getOperation();

  ARTS_INFO_HEADER(DbPass);
  ARTS_DEBUG_REGION(module.dump(););

  assert(AM && "ArtsAnalysisManager must be provided externally");

  /// Graph construction and analysis
  module.walk([&](func::FuncOp func) {
    DbGraph &graph = AM->getDbGraph(func);
    graph.build();

    /// Print analysis results for verification
    ARTS_DEBUG_REGION({
      llvm::outs() << "\n";
      graph.print(llvm::outs());
      llvm::outs() << "\n";
    });
  });
  /// flush buffer
  llvm::outs().flush();

  /// Adjust DB modes based on dependencies and accesses
  changed |= adjustDbModes();

  /// Pin and split acquires based on invariant indices
  bool pinChanged = pinAndSplitAcquires();
  changed |= pinChanged;

  /// If pinning changed the IR, rebuild graph for dimensionality reduction
  if (pinChanged) {
    ARTS_DEBUG("Rebuilding graph after pinAndSplitAcquires");
    module.walk([&](func::FuncOp func) {
      AM->invalidateFunction(func);
      DbGraph &graph = AM->getDbGraph(func);
      graph.build();
    });
  }

  /// Reduce allocation dimensionality (assumes pinAndSplitAcquires ran first)
  changed |= reduceAllocDimensionality();

  if (changed)
    module.walk([&](func::FuncOp func) { AM->invalidateFunction(func); });
  else
    ARTS_INFO("No changes made to the module");

  ARTS_INFO_FOOTER(DbPass);
  ARTS_DEBUG_REGION(module.dump(););
}

//===----------------------------------------------------------------------===//
/// Adjust DB modes based on accesses.
/// Based on the collected loads and stores, adjust the DB mode to in, out, or
/// inout.
//===----------------------------------------------------------------------===//
bool DbPass::adjustDbModes() {
  ARTS_DEBUG_HEADER(AdjustDBModes);
  bool changed = false;

  module.walk([&](func::FuncOp func) {
    DbGraph &graph = AM->getDbGraph(func);

    /// Helper: lattice join for ArtsMode. Ensures monotonic escalation:
    /// in U in = in, out U out = out, any U inout = inout, and
    /// in U out = inout
    auto joinMode = [](ArtsMode a, ArtsMode b) -> ArtsMode {
      if (a == b)
        return a;
      if (a == ArtsMode::inout || b == ArtsMode::inout)
        return ArtsMode::inout;
      /// Remaining distinct cases are (in, out) or (out, in)
      return ArtsMode::inout;
    };

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

      // Each DbAcquireNode's mode is derived from its own accesses only.
      // Nested acquires will be processed separately by forEachAcquireNode.

      if (newMode == acqOp.getMode())
        return;

      ARTS_DEBUG("AcquireOp: " << acqOp << " from " << acqOp.getMode() << " to "
                               << newMode);
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
            maxMode = joinMode(maxMode, mode);

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

//===----------------------------------------------------------------------===//
/// Pin and split acquires based on invariant access patterns
///
/// This optimization analyzes datablock acquires from innermost to outermost,
/// checking if memory accesses (loads/stores) use invariant indices within the
/// EDT region.
///
/// Example transformation:
///   Before:
///     %guid, %ptr = arts.db_acquire (%src_guid, %src_ptr)
///                   indices[%i] offsets[%c0] sizes[%N]
///     arts.edt (%ptr) {
///     ^bb0(%arg: memref<?xmemref<?xf64>>):
///       %a = memref.load %arg[%c5]  // invariant index
///       %b = memref.load %arg[%c7]  // different invariant index
///     }
///
///   After (split into two acquires):
///     %guid1, %ptr1 = arts.db_acquire (%src_guid, %src_ptr)
///                     indices[%i, %c5] offsets[%c0] sizes[%c1]
///     %guid2, %ptr2 = arts.db_acquire (%src_guid, %src_ptr)
///                     indices[%i, %c7] offsets[%c0] sizes[%c1]
///     arts.edt (%ptr1, %ptr2) {
///     ^bb0(%arg1: memref<?xf64>, %arg2: memref<?xf64>):
///       %a = memref.load %arg1[]  // now scalar access
///       %b = memref.load %arg2[]  // now scalar access
///     }
///
/// Key principle: We modify the acquire's indices (dimension selectors) and
/// set sizes to 1 for pinned dimensions, but offsets remain unchanged.
//===----------------------------------------------------------------------===//
bool DbPass::pinAndSplitAcquires() {
  ARTS_DEBUG_HEADER(PinAndSplitAcquires);
  bool changed = false;

  module.walk([&](func::FuncOp func) {
    DbGraph &graph = AM->getDbGraph(func);
    ARTS_DEBUG("Analyzing function: " << func.getName());

    /// Step 1: Collect all acquires with nesting depth for bottom-up processing
    struct AcquireWithDepth {
      DbAcquireNode *node;
      size_t depth;
    };
    SmallVector<AcquireWithDepth> allAcquires;

    std::function<void(DbAcquireNode *, size_t)> collectAcquires =
        [&](DbAcquireNode *acqNode, size_t depth) {
          allAcquires.push_back({acqNode, depth});
          acqNode->forEachChildNode([&](NodeBase *child) {
            if (auto *nestedAcq = dyn_cast<DbAcquireNode>(child))
              collectAcquires(nestedAcq, depth + 1);
          });
        };

    graph.forEachAllocNode([&](DbAllocNode *allocNode) {
      allocNode->forEachChildNode([&](NodeBase *child) {
        if (auto *acqNode = dyn_cast<DbAcquireNode>(child))
          collectAcquires(acqNode, 0);
      });
    });

    /// Sort by depth (descending) to process innermost acquires first
    llvm::sort(allAcquires,
               [](const AcquireWithDepth &a, const AcquireWithDepth &b) {
                 return a.depth > b.depth;
               });

    ARTS_DEBUG("Found " << allAcquires.size() << " acquire nodes");
    if (allAcquires.empty())
      return;

    /// Step 2: Process each acquire from innermost to outermost
    for (const AcquireWithDepth &info : allAcquires) {
      DbAcquireNode *acqNode = info.node;
      DbAcquireOp acqOp = acqNode->getDbAcquireOp();
      size_t rank = acqNode->getInfo().getRank();

      ARTS_DEBUG("Processing acquire: " << acqOp << " (rank=" << rank << ")");

      /// Get memory accesses (loads/stores) within the EDT
      SmallVector<Operation *> accesses;
      acqNode->getMemoryAccesses(accesses);
      if (accesses.empty()) {
        ARTS_DEBUG("  No memory accesses, skipping");
        continue;
      }

      Region &edtRegion = acqNode->getEdtUser().getBody();
      Value edtArg = acqNode->getUseInEdt();

      /// Helper: Extract indices from memory operation if it accesses edtArg
      auto getAccessIndices = [&](Operation *op) -> std::optional<ValueRange> {
        Value mem;
        ValueRange idxs;
        if (auto ld = dyn_cast<memref::LoadOp>(op)) {
          mem = ld.getMemref();
          idxs = ld.getIndices();
        } else if (auto st = dyn_cast<memref::StoreOp>(op)) {
          mem = st.getMemref();
          idxs = st.getIndices();
        } else {
          return std::nullopt;
        }
        if (!edtRegion.isAncestor(op->getParentRegion()) || mem != edtArg)
          return std::nullopt;
        return idxs;
      };

      /// Step 3: Analyze which dimensions use invariant indices
      /// For each dimension, track whether ALL accesses use invariant indices
      SmallVector<bool> dimHasInvariant(rank, true);

      for (Operation *acc : accesses) {
        auto indicesOpt = getAccessIndices(acc);
        /// Skip accesses that don't match (e.g., nested memref accesses)
        if (!indicesOpt)
          continue;

        /// Check for rank mismatch
        if (indicesOpt->size() != rank) {
          ARTS_DEBUG("  Access with mismatched rank, marking all dims variant");
          std::fill(dimHasInvariant.begin(), dimHasInvariant.end(), false);
          break;
        }

        ValueRange indices = *indicesOpt;
        for (size_t d = 0; d < rank; ++d) {
          Value idx = indices[d];
          bool isInv = arts::isInvariantInEdt(edtRegion, idx);
          ARTS_DEBUG("    Index [" << d << "]: " << idx
                                   << " -> invariant=" << isInv);
          if (!isInv)
            dimHasInvariant[d] = false;
        }
      }

      /// If no dimensions have invariant accesses, skip
      if (llvm::none_of(dimHasInvariant, [](bool b) { return b; })) {
        ARTS_DEBUG("  No invariant dimensions, skipping");
        continue;
      }

      /// Step 4: Group accesses by their invariant index patterns
      /// Key: invariant indices for this access group
      /// Value: list of operations with that pattern
      struct AccessGroup {
        SmallVector<Value> invariantIndices; ///< Indices for invariant dims
        SmallVector<Operation *> operations; ///< Ops with this pattern
      };
      SmallVector<AccessGroup> groups;

      for (Operation *acc : accesses) {
        auto indicesOpt = getAccessIndices(acc);
        if (!indicesOpt || indicesOpt->size() != rank)
          continue;

        ValueRange indices = *indicesOpt;
        SmallVector<Value> pattern;
        for (size_t d = 0; d < rank; ++d) {
          if (dimHasInvariant[d])
            pattern.push_back(indices[d]);
        }

        /// Find existing group or create new one
        auto it = llvm::find_if(groups, [&](const AccessGroup &g) {
          return arts::equalRange(g.invariantIndices, pattern);
        });
        if (it != groups.end()) {
          it->operations.push_back(acc);
        } else {
          groups.push_back({pattern, {acc}});
        }
      }

      size_t numGroups = groups.size();
      if (numGroups == 0) {
        ARTS_DEBUG("  No valid access groups, skipping");
        continue;
      }

      ARTS_DEBUG("  Found " << numGroups << " unique access patterns");

      /// Step 5: Create new acquires - one per access group
      /// Each new acquire has:
      /// - Extended indices (original + invariant values)
      /// - Sizes set to 1 for invariant dimensions
      /// - Offsets unchanged
      OpBuilder b(acqOp);
      Location loc = acqOp.getLoc();
      Value zero = b.create<arith::ConstantIndexOp>(loc, 0);
      Value one = b.create<arith::ConstantIndexOp>(loc, 1);

      SmallVector<DbAcquireOp> newAcquires;
      newAcquires.reserve(numGroups);

      for (const AccessGroup &group : groups) {
        /// Build new indices: original + invariant values
        SmallVector<Value> newIndices(acqOp.getIndices().begin(),
                                      acqOp.getIndices().end());
        newIndices.append(group.invariantIndices.begin(),
                          group.invariantIndices.end());

        /// Build new sizes: set to 1 for invariant dimensions
        SmallVector<Value> newSizes;
        newSizes.reserve(rank);
        auto oldSizes = acqOp.getSizes();
        for (size_t d = 0; d < rank; ++d) {
          if (dimHasInvariant[d])
            newSizes.push_back(one);
          else
            newSizes.push_back(d < oldSizes.size() ? oldSizes[d] : one);
        }

        /// Offsets remain unchanged
        SmallVector<Value> newOffsets;
        newOffsets.reserve(rank);
        auto oldOffsets = acqOp.getOffsets();
        for (size_t d = 0; d < rank; ++d)
          newOffsets.push_back(d < oldOffsets.size() ? oldOffsets[d] : zero);

        DbAcquireOp newAcq = b.create<DbAcquireOp>(
            loc, acqOp.getMode(), acqOp.getSourceGuid(), acqOp.getSourcePtr(),
            newIndices, newOffsets, newSizes);
        ARTS_DEBUG("  Created new acquire: " << newAcq);
        newAcquires.push_back(newAcq);
      }

      /// Step 6: Update EDT to accept multiple dependencies
      EdtOp edtOp = acqNode->getEdtUser();
      Operation *edtOperation = edtOp.getOperation();
      Value oldPtr = acqOp.getPtr();

      /// Find which operand corresponds to this acquire
      size_t operandIdx = 0;
      bool found = false;
      for (size_t i = 0; i < edtOperation->getNumOperands(); ++i) {
        if (edtOperation->getOperand(i) == oldPtr) {
          operandIdx = i;
          found = true;
          break;
        }
      }
      assert(found && "Acquire pointer not found in EDT operands");

      /// Replace operand with first new acquire, insert others after
      SmallVector<Value> newPtrs;
      for (DbAcquireOp newAcq : newAcquires)
        newPtrs.push_back(newAcq.getPtr());
      edtOperation->setOperand(operandIdx, newPtrs[0]);
      if (numGroups > 1)
        edtOperation->insertOperands(operandIdx + 1,
                                     ArrayRef<Value>(newPtrs).drop_front());

      /// Step 7: Update EDT region - add block arguments for new acquires
      Block &entryBlock = edtRegion.front();
      BlockArgument oldBlockArg = cast<BlockArgument>(edtArg);
      size_t argIdx = oldBlockArg.getArgNumber();
      Type argType = edtArg.getType();

      /// Insert all new block arguments (this pushes the old one to argIdx +
      /// numGroups)
      SmallVector<BlockArgument> newBlockArgs;
      newBlockArgs.reserve(numGroups);
      for (size_t k = 0; k < numGroups; ++k) {
        BlockArgument newArg =
            entryBlock.insertArgument(argIdx + k, argType, loc);
        newBlockArgs.push_back(newArg);
      }

      /// Step 8: Update memory accesses to use appropriate block arg and
      /// indices
      for (size_t k = 0; k < numGroups; ++k) {
        BlockArgument newArg = newBlockArgs[k];
        for (Operation *acc : groups[k].operations) {
          auto indicesOpt = getAccessIndices(acc);
          if (!indicesOpt)
            continue;

          /// Build new indices: zero for invariant dims, keep for variant dims
          SmallVector<Value> newAccessIndices;
          newAccessIndices.reserve(rank);
          ValueRange oldIndices = *indicesOpt;
          for (size_t d = 0; d < rank; ++d) {
            newAccessIndices.push_back(dimHasInvariant[d] ? zero
                                                          : oldIndices[d]);
          }

          /// Update the memory operation
          if (auto ld = dyn_cast<memref::LoadOp>(acc)) {
            ld.setMemRef(newArg);
            ld.getIndicesMutable().assign(newAccessIndices);
          } else if (auto st = dyn_cast<memref::StoreOp>(acc)) {
            st.setMemRef(newArg);
            st.getIndicesMutable().assign(newAccessIndices);
          }
        }
      }

      /// Step 9: Handle releases - duplicate for each new acquire
      if (DbReleaseOp oldRelease = acqNode->getDbReleaseOp()) {
        OpBuilder rb(oldRelease);
        for (size_t k = 0; k < numGroups; ++k) {
          if (k == 0) {
            oldRelease.getSourceMutable().assign(newBlockArgs[0]);
          } else {
            auto newRelease = cast<DbReleaseOp>(rb.clone(*oldRelease));
            newRelease.getSourceMutable().assign(newBlockArgs[k]);
          }
        }
      }

      /// Step 10: Clean up - remove old block argument and acquire operation
      entryBlock.eraseArgument(argIdx + numGroups);

      /// Replace guid uses (use first new acquire's guid)
      acqOp.getGuid().replaceAllUsesWith(newAcquires[0].getGuid());
      acqOp.erase();

      ARTS_DEBUG("  Successfully split acquire into " << numGroups
                                                      << " acquires");
      changed = true;
    }
  });

  ARTS_DEBUG_FOOTER(PinAndSplitAcquires);
  return changed;
}

//===----------------------------------------------------------------------===//
/// Reduce allocation dimensionality
///
/// After pinAndSplitAcquires runs, acquires will have extended indices arrays
/// where extra indices indicate pinned dimensions. This pass checks if all
/// acquires for an allocation have at least one pinned dimension. If so, the
/// allocation can be reduced by moving those dimensions to the payload.
///
/// Assumes pinAndSplitAcquires ran first to ensure acquires have extended
/// indices with pinned dimensions indicated by sizes=1.
///
/// Example:
///   Before:
///     %alloc = arts.db_alloc sizes[%N] elementType(memref<?xf64>)
///     %acq1 = arts.db_acquire indices[%i, %c5] sizes[%c1]  // pinned dim
///     %acq2 = arts.db_acquire indices[%j, %c7] sizes[%c1]  // pinned dim
///
///   After:
///     %alloc = arts.db_alloc sizes[] elementType(memref<?xf64>)
///              payloadSizes[%N]
///     %acq1 = arts.db_acquire indices[%i] sizes[]
///     %acq2 = arts.db_acquire indices[%j] sizes[]
//===----------------------------------------------------------------------===//
bool DbPass::reduceAllocDimensionality() {
  ARTS_DEBUG_HEADER(ReduceAllocDimensionality);
  bool changed = false;

  module.walk([&](func::FuncOp func) {
    DbGraph &graph = AM->getDbGraph(func);

    graph.forEachAllocNode([&](DbAllocNode *allocNode) {
      DbAllocOp oldAlloc = allocNode->getDbAllocOp();
      assert(oldAlloc && "Alloc node must have a DbAllocOp");

      /// Get allocation's base dimensionality
      size_t allocDims = oldAlloc.getSizes().size();
      if (allocDims == 0) {
        ARTS_DEBUG("Alloc has no dimensions, skipping");
        return;
      }

      /// Collect all acquire nodes (including nested ones)
      SmallVector<DbAcquireNode *, 8> acquires;
      std::function<void(DbAcquireNode *)> collectAcquires =
          [&](DbAcquireNode *acqNode) {
            acquires.push_back(acqNode);
            acqNode->forEachChildNode([&](NodeBase *child) {
              if (auto *nestedAcq = dyn_cast<DbAcquireNode>(child))
                collectAcquires(nestedAcq);
            });
          };

      allocNode->forEachChildNode([&](NodeBase *child) {
        if (auto *acq = dyn_cast<DbAcquireNode>(child))
          collectAcquires(acq);
      });

      if (acquires.empty())
        return;

      /// Check if all acquires have the same number of pinned dimensions
      /// Pinned dimensions are indicated by: indices.size() > allocDims
      std::optional<size_t> commonPinnedDims;
      for (DbAcquireNode *acqNode : acquires) {
        DbAcquireOp acqOp = acqNode->getDbAcquireOp();
        size_t numIndices = acqOp.getIndices().size();

        if (numIndices < allocDims) {
          ARTS_DEBUG("  Acquire has fewer indices than alloc dims, skipping");
          return;
        }

        size_t pinnedDims = numIndices - allocDims;

        if (!commonPinnedDims.has_value()) {
          commonPinnedDims = pinnedDims;
        } else if (*commonPinnedDims != pinnedDims) {
          ARTS_DEBUG("  Acquires have different numbers of pinned dimensions");
          return;
        }
      }

      /// If no dimensions are pinned, nothing to reduce
      size_t dropDims = commonPinnedDims.value_or(0);
      if (dropDims == 0) {
        ARTS_DEBUG("  No pinned dimensions, skipping");
        return;
      }

      ARTS_DEBUG("Reducing alloc " << oldAlloc << " by " << dropDims
                                   << " dimensions");

      /// Build new allocation with reduced dimensionality
      SmallVector<Value> oldSizes(oldAlloc.getSizes().begin(),
                                  oldAlloc.getSizes().end());
      if (dropDims > oldSizes.size())
        return;

      SmallVector<Value> oldPayload(oldAlloc.getPayloadSizes().begin(),
                                    oldAlloc.getPayloadSizes().end());

      /// New sizes: drop the first dropDims dimensions
      SmallVector<Value> newSizes(oldSizes.begin() + dropDims, oldSizes.end());

      /// New payload: prepend dropped dimensions to existing payload
      SmallVector<Value> newPayload;
      newPayload.reserve(dropDims + oldPayload.size());
      newPayload.append(oldSizes.begin(), oldSizes.begin() + dropDims);
      newPayload.append(oldPayload.begin(), oldPayload.end());

      if (newSizes.empty()) {
        ARTS_DEBUG("  All dimensions would be dropped, skipping");
        return;
      }

      OpBuilder builder(oldAlloc);
      Location loc = oldAlloc.getLoc();
      Value newAddress = oldAlloc.getAddress();

      DbAllocOp newAlloc;
      if (newAddress)
        newAlloc = builder.create<DbAllocOp>(
            loc, oldAlloc.getMode(), oldAlloc.getRoute(),
            oldAlloc.getAllocType(), oldAlloc.getDbMode(), newAddress, newSizes,
            newPayload);
      else
        newAlloc = builder.create<DbAllocOp>(
            loc, oldAlloc.getMode(), oldAlloc.getRoute(),
            oldAlloc.getAllocType(), oldAlloc.getDbMode(),
            oldAlloc.getElementType(), newSizes, newPayload);

      /// Update all acquires - drop pinned indices
      for (DbAcquireNode *acqNode : acquires) {
        DbAcquireOp oldAcq = acqNode->getDbAcquireOp();
        OpBuilder acqBuilder(oldAcq);
        Location aloc = oldAcq.getLoc();

        /// Drop the trailing pinned indices (keep only first allocDims indices)
        SmallVector<Value> allIndices(oldAcq.getIndices().begin(),
                                      oldAcq.getIndices().end());
        SmallVector<Value> newIdx(allIndices.begin(),
                                  allIndices.begin() + allocDims);
        SmallVector<Value> offs(oldAcq.getOffsets().begin(),
                                oldAcq.getOffsets().end());
        SmallVector<Value> szs(oldAcq.getSizes().begin(),
                               oldAcq.getSizes().end());

        DbAcquireOp newAcq = acqBuilder.create<DbAcquireOp>(
            aloc, oldAcq.getMode(), newAlloc.getGuid(), newAlloc.getPtr(),
            newIdx, offs, szs);

        oldAcq.getGuid().replaceAllUsesWith(newAcq.getGuid());
        oldAcq.getPtr().replaceAllUsesWith(newAcq.getPtr());
        oldAcq.erase();
      }

      oldAlloc.getGuid().replaceAllUsesWith(newAlloc.getGuid());
      oldAlloc.getPtr().replaceAllUsesWith(newAlloc.getPtr());
      oldAlloc.erase();

      ARTS_DEBUG("Reduced alloc dimensionality by " << dropDims
                                                    << " dimensions");
      changed = true;
    });
  });

  ARTS_DEBUG_FOOTER(ReduceAllocDimensionality);
  return changed;
}

//===----------------------------------------------------------------------===//
/// Tighten acquire slices
///
/// Optimization A (offset tightening): For acquires, if all accesses within
/// the EDT target a single invariant index tuple, rewrite the acquire to
/// `sizes=[1,...]` with `offsets=[base+idx,...]` and retarget load/store
/// indices by subtracting the chosen offset (becomes [0,...]).
///
/// Optimization B (split by N elements): For acquires with exactly N distinct
/// invariant index tuples accessed in the EDT (and no dynamic indices), split
/// the single acquire into N size-1 acquires (one per tuple), update the EDT
/// dependency list and insert new block arguments for the additional acquires,
/// then retarget loads/stores to the corresponding block argument with index
/// [0,...]. The original releases are replaced by N releases.
//===----------------------------------------------------------------------===//
bool DbPass::tightenAcquireSlices() {
  ARTS_DEBUG_HEADER(TightenAcquireSlices);
  bool changed = false;

  module.walk([&](func::FuncOp func) {
    DbGraph &graph = AM->getDbGraph(func);

    graph.forEachAcquireNode([&](DbAcquireNode *acqNode) {
      DbAcquireOp acqOp = acqNode->getDbAcquireOp();
      size_t rank = acqNode->getInfo().getRank();
      if (rank == 0)
        return;

      SmallVector<Operation *, 6> accesses;
      acqNode->getMemoryAccesses(accesses);
      if (accesses.empty())
        return;

      DenseMap<Operation *, SmallVector<Value>> accToTuple;
      SmallVector<SmallVector<Value>> uniqueTuples;
      bool anyNonInvariant = false;

      Region &edtRegion = acqNode->getEdtUser().getBody();
      Value edtArg = acqNode->getUseInEdt();

      for (Operation *acc : accesses) {
        Value mem;
        ValueRange idxs;
        if (auto ld = dyn_cast<memref::LoadOp>(acc)) {
          mem = ld.getMemref();
          idxs = ld.getIndices();
        } else if (auto st = dyn_cast<memref::StoreOp>(acc)) {
          mem = st.getMemref();
          idxs = st.getIndices();
        } else {
          continue;
        }
        if (!edtRegion.isAncestor(acc->getParentRegion()) || mem != edtArg)
          continue;

        if (idxs.size() != rank) {
          anyNonInvariant = true;
          break;
        }

        SmallVector<Value> tuple(idxs.begin(), idxs.end());
        bool allInvariant = true;
        for (Value iv : tuple) {
          if (!arts::isInvariantInEdt(edtRegion, iv)) {
            allInvariant = false;
            break;
          }
        }
        if (!allInvariant) {
          anyNonInvariant = true;
          break;
        }

        accToTuple[acc] = tuple;
        bool found = false;
        for (const auto &u : uniqueTuples)
          if (u == tuple) {
            found = true;
            break;
          }
        if (!found)
          uniqueTuples.push_back(tuple);
      }

      if (anyNonInvariant) {
        ARTS_DEBUG("Skipping acquire " << acqOp
                                       << " - has non-invariant indices");
        return;
      }

      // Case A: Single invariant tuple used – tighten acquire.
      if (uniqueTuples.size() == 1) {
        ARTS_DEBUG("Tightening acquire " << acqOp
                                         << " to single element slice");
        const SmallVector<Value> &theTuple = uniqueTuples.front();
        OpBuilder b(acqOp);
        Location loc = acqOp.getLoc();

        SmallVector<Value> newOffs;
        auto oldOffs = acqOp.getOffsets();
        if (oldOffs.empty()) {
          newOffs.append(theTuple.begin(), theTuple.end());
        } else {
          if (oldOffs.size() != rank)
            return;
          for (size_t d = 0; d < rank; ++d) {
            Value base = oldOffs[d];
            Value addend = theTuple[d];
            {
              int64_t cst = 0;
              if (arts::getConstantIndex(addend, cst) && cst == 0) {
                newOffs.push_back(base);
                continue;
              }
            }
            {
              int64_t cst = 0;
              if (arts::getConstantIndex(base, cst) && cst == 0) {
                newOffs.push_back(addend);
                continue;
              }
            }
            newOffs.push_back(b.create<arith::AddIOp>(loc, base, addend));
          }
        }

        Value one = b.create<arith::ConstantIndexOp>(loc, 1);
        SmallVector<Value> newSizes(rank, one);

        DbAcquireOp newAcq = b.create<DbAcquireOp>(
            loc, acqOp.getMode(), acqOp.getSourceGuid(), acqOp.getSourcePtr(),
            acqOp.getIndices(), newOffs, newSizes);

        // Update EDT operand (replace pointer use in the EDT)
        Value oldPtr = acqOp.getPtr();
        Operation *edtOp = acqNode->getEdtUser().getOperation();
        for (auto &use : oldPtr.getUses()) {
          if (use.getOwner() == edtOp) {
            edtOp->setOperand(use.getOperandNumber(), newAcq.getPtr());
            break;
          }
        }

        // Retarget accesses to zero indices
        Value zero = b.create<arith::ConstantIndexOp>(loc, 0);
        SmallVector<Value> zeroIdx(rank, zero);
        for (auto &kv : accToTuple) {
          Operation *acc = kv.first;
          if (auto ld = dyn_cast<memref::LoadOp>(acc)) {
            OpBuilder lb(ld);
            ld.getMemrefMutable().assign(edtArg);
            ld.getIndicesMutable().assign(zeroIdx);
          } else if (auto st = dyn_cast<memref::StoreOp>(acc)) {
            OpBuilder sb(st);
            st.getMemrefMutable().assign(edtArg);
            st.getIndicesMutable().assign(zeroIdx);
          }
        }

        acqOp.getGuid().replaceAllUsesWith(newAcq.getGuid());
        acqOp.getPtr().replaceAllUsesWith(newAcq.getPtr());
        acqOp.erase();

        ARTS_DEBUG("Successfully tightened acquire " << newAcq
                                                     << " to single element");
        changed = true;
        return;
      }

      // Case B removed: splitting acquires is handled in
      // reduceAllocDimensionality.
    });
  });

  ARTS_DEBUG_FOOTER(TightenAcquireSlices);
  return changed;
}

//===----------------------------------------------------------------------===//
/// Convert trivial DBs to parameters
///
/// When a DB is effectively read-only and 1D, turn it into a parameter
/// to avoid runtime traffic.
/// Example:
///   %db = arts.db_alloc %ptr sizes(%cN)
///   %a  = arts.db_acquire %db ...
///   load %a[...]
///   --> convert to a function/block argument replacing %db/%a
//===----------------------------------------------------------------------===//
bool DbPass::convertToParameters() {
  bool changed = false;
  // module.walk([&](func::FuncOp func) {
  //   DbGraph &graph = AM->getDbGraph(func);
  //   graph.forEachAllocNode([&](DbAllocNode *allocNode) {
  //     bool hasSingleSize = (allocNode->getSizes().size() == 1);
  //     bool isOnlyReader = (allocNode->getAcquireNodesSize() > 0 &&
  //                          allocNode->getReleaseNodesSize() == 0);

  //     if (!hasSingleSize || !isOnlyReader)
  //       return;

  //     ARTS_DEBUG("- Converting DB to parameter: "
  //                       << allocNode->getDbAllocOp() << "\n");
  //     allocNode->forEachChildNode([&](NodeBase *child) {
  //       if (auto acquireNode = dyn_cast<DbAcquireNode>(child)) {
  //         DbAcquireOp acquireOp = acquireNode->getDbAcquireOp();
  //         acquireOp.getPtr().replaceAllUsesWith(allocNode->getPtr());
  //         acquireOp.erase();
  //       }
  //     });
  //     DbAllocOp allocOp = allocNode->getDbAllocOp();
  //     if (allocOp.getGuid().use_empty() && allocOp.getPtr().use_empty()) {
  //       allocOp.erase();
  //     }
  //     changed = true;
  //   });
  // });
  return changed;
}

//===----------------------------------------------------------------------===//
/// Dead DB elimination
///
/// Remove DbAllocOps with no associated acquires/releases and
/// no remaining uses. Uses DbGraph to count children; then checks use_empty.
/// Example (before → after):
///   %db = arts.db_alloc %ptr sizes(%cN,%cM)
/// no uses, no acquires/releases
/// becomes: (alloc erased)
//===----------------------------------------------------------------------===//
bool DbPass::deadDbElimination() {
  bool changed = false;

  module.walk([&](func::FuncOp func) {
    auto &graph = AM->getDbGraph(func);
    SmallVector<DbAllocNode *> deadAllocs;
    graph.forEachAllocNode([&](DbAllocNode *allocNode) {
      /// Check if allocation has no acquires (counting nested acquires too)
      size_t totalAcquires = 0;

      /// Count all acquire nodes in the hierarchy
      std::function<void(DbAcquireNode *)> countAcquires =
          [&](DbAcquireNode *acqNode) {
            totalAcquires++;

            /// Recursively count child acquires
            acqNode->forEachChildNode([&](NodeBase *child) {
              if (auto *nestedAcq = dyn_cast<DbAcquireNode>(child))
                countAcquires(nestedAcq);
            });
          };

      allocNode->forEachChildNode([&](NodeBase *child) {
        if (auto *acqNode = dyn_cast<DbAcquireNode>(child))
          countAcquires(acqNode);
      });

      if (totalAcquires == 0) {
        deadAllocs.push_back(allocNode);
      }
    });
    for (auto *allocNode : deadAllocs) {
      auto dbOp = allocNode->getDbAllocOp();
      if (dbOp && dbOp->use_empty()) {
        dbOp.erase();
        changed = true;
      }
    }
  });
  return changed;
}

void DbPass::exportToJson() {
  /// Create output directory if it doesn't exist
  std::string outputDir = "output";
  std::error_code ec = llvm::sys::fs::create_directories(outputDir);
  if (ec) {
    ARTS_ERROR("Failed to create output directory: " << ec.message());
    return;
  }

  module.walk([&](func::FuncOp func) {
    DbGraph &graph = AM->getDbGraph(func);
    if (graph.isEmpty())
      return;

    /// Create filename based on function name
    std::string filename = outputDir + "/" + func.getName().str() + "_db.json";

    /// Open file for writing
    std::error_code ec;
    llvm::raw_fd_ostream file(filename, ec);
    if (ec) {
      ARTS_ERROR("Failed to open file " << filename << ": " << ec.message());
      return;
    }

    /// Export the graph to JSON
    graph.exportToJson(file, true);
    file.close();

    ARTS_INFO("Exported DB analysis for function '" << func.getName() << "' to "
                                                    << filename);
  });
}

///===----------------------------------------------------------------------===///
/// Pass creation
///===----------------------------------------------------------------------===///
namespace mlir {
namespace arts {
std::unique_ptr<Pass> createDbPass(ArtsAnalysisManager *AM, bool exportJson) {
  return std::make_unique<DbPass>(AM, exportJson);
}
} // namespace arts
} // namespace mlir
