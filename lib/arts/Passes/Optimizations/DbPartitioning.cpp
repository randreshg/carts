///==========================================================================///
/// DbPartitioning.cpp - Datablock partitioning decision pass
///
/// Analyzes allocations and selects partitioning strategies:
///   Phase 1: Per-acquire analysis (chunk offset/size, capabilities)
///   Phase 2: Heuristic voting -> RewriterMode
///   (Coarse/ElementWise/Block/Stencil) Phase 3: Size computation and
///   rewriter application
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
#include "../ArtsPassDetails.h"
#include "arts/Analysis/ArtsAnalysisManager.h"
#include "arts/Analysis/Db/DbAnalysis.h"
#include "arts/Analysis/Graphs/Db/DbGraph.h"
#include "arts/Analysis/Loop/LoopAnalysis.h"
#include "arts/Analysis/Loop/LoopNode.h"
#include "arts/ArtsDialect.h"
#include "arts/Passes/ArtsPasses.h"
/// Other
#include "mlir/IR/OpDefinition.h"
#include "mlir/IR/Operation.h"
#include "mlir/IR/Visitors.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Support/LogicalResult.h"
/// Debug
#include "arts/Analysis/Graphs/Db/DbNode.h"
#include "arts/Analysis/Graphs/Edt/EdtGraph.h"
#include "arts/Analysis/Graphs/Edt/EdtNode.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallVector.h"
#include <algorithm>
#include <cstdint>
#include <optional>

#include "arts/Transforms/Datablock/DbBlockPlanResolver.h"
#include "arts/Transforms/Datablock/DbPartitionDecisionArbiter.h"
#include "arts/Transforms/Datablock/DbPartitionPlanner.h"
#include "arts/Transforms/Datablock/DbRewriter.h"
#include "arts/Transforms/Datablock/StencilBoundsLowering.h"
#include "arts/Utils/ArtsDebug.h"
#include "arts/Utils/ArtsUtils.h"
#include "arts/Utils/DatablockUtils.h"
#include "arts/Utils/OperationAttributes.h"
ARTS_DEBUG_SETUP(db_partitioning);

using namespace mlir;
using namespace mlir::func;
using namespace mlir::arith;
using namespace mlir::polygeist;
using namespace mlir::arts;

namespace {

/// Validate that element-wise partition indices match actual EDT accesses.
/// Returns false if this is a block-wise pattern (indices are block corners,
/// but accesses span a range) - indicating we should fall back to block mode.
///
/// Detection criteria for block-wise pattern:
/// 1. Partition hints have indices[] (looks like element-wise)
/// 2. BUT the enclosing loop steps by > 1 (BLOCK_SIZE)
/// 3. OR partition indices don't appear in EDT body accesses
static bool
validateElementWiseIndices(ArrayRef<AcquirePartitionInfo> acquireInfos,
                           DbAllocNode *allocNode) {
  for (const auto &info : acquireInfos) {
    DbAcquireOp acquire = info.acquire;
    auto partitionIndices = acquire.getPartitionIndices();
    if (partitionIndices.empty())
      continue;

    /// Use DbAcquireNode's validation method for the actual check
    if (allocNode) {
      auto *acqNode = allocNode->findAcquireNode(acquire);
      if (acqNode && !acqNode->validateElementWisePartitioning())
        return false;
    }
  }
  return true; /// Valid element-wise
}

/// Extract all partition offsets for N-D support.
/// Unified function that handles both single and multi-dimensional cases.
/// Use front() on the result for 1-D compatibility.
static SmallVector<Value>
getPartitionOffsetsND(DbAcquireNode *acqNode,
                      const AcquirePartitionInfo *info) {
  if (info && !info->partitionOffsets.empty())
    return info->partitionOffsets;

  SmallVector<Value> result;
  DbAcquireOp acqOp = acqNode->getDbAcquireOp();
  auto mode = acqOp.getPartitionMode();
  bool preferPartitionIndices = !mode || *mode == PartitionMode::fine_grained;

  /// Fine-grained uses partition indices; block/stencil use offsets.
  auto indices = acqOp.getPartitionIndices();
  auto offsets = acqOp.getPartitionOffsets();
  if (preferPartitionIndices && !indices.empty()) {
    result.append(indices.begin(), indices.end());
    return result;
  }
  if (!offsets.empty()) {
    result.append(offsets.begin(), offsets.end());
    return result;
  }
  if (!indices.empty()) {
    result.append(indices.begin(), indices.end());
    return result;
  }

  /// Finally, try getPartitionInfo for single-dim case
  auto [offset, size] = acqNode->getPartitionInfo();
  if (offset)
    result.push_back(offset);

  return result;
}

/// Pick a representative partition offset (prefer non-constant).
/// Returns the chosen offset and its index via outIdx when provided.
static Value pickRepresentativePartitionOffset(ArrayRef<Value> offsets,
                                               unsigned *outIdx = nullptr) {
  if (outIdx)
    *outIdx = 0;
  for (unsigned i = 0; i < offsets.size(); ++i) {
    Value off = offsets[i];
    if (!off)
      continue;
    int64_t c = 0;
    if (!ValueUtils::getConstantIndex(ValueUtils::stripNumericCasts(off), c)) {
      if (outIdx)
        *outIdx = i;
      return off;
    }
  }
  for (unsigned i = 0; i < offsets.size(); ++i) {
    if (offsets[i]) {
      if (outIdx)
        *outIdx = i;
      return offsets[i];
    }
  }
  return Value();
}

/// Analyze a single acquire to determine its partition mode and chunk info.
static AcquirePartitionInfo computeAcquirePartitionInfo(DbAcquireOp acquire,
                                                        DbAcquireNode *acqNode,
                                                        OpBuilder &builder) {
  AcquirePartitionInfo info;
  info.acquire = acquire;
  DbAnalysis::AcquirePartitionSummary summary;
  if (acqNode && acqNode->getAnalysis())
    summary = acqNode->getAnalysis()->analyzeAcquirePartition(acquire, builder);
  else {
    summary.mode = DatablockUtils::getPartitionModeFromStructure(acquire);
    summary.isValid = false;
  }
  info.mode = summary.mode;
  info.partitionOffsets.assign(summary.partitionOffsets.begin(),
                               summary.partitionOffsets.end());
  info.partitionSizes.assign(summary.partitionSizes.begin(),
                             summary.partitionSizes.end());
  info.partitionDims.assign(summary.partitionDims.begin(),
                            summary.partitionDims.end());
  info.isValid = summary.isValid;
  info.hasIndirectAccess = summary.hasIndirectAccess;

  return info;
}

static void resetCoarseAcquireRanges(DbAllocOp allocOp, DbAllocNode *allocNode,
                                     OpBuilder &builder) {
  if (!allocNode || allocOp.getSizes().empty())
    return;

  Value zero = builder.create<arith::ConstantIndexOp>(allocOp.getLoc(), 0);
  SmallVector<Value> fullOffsets;
  SmallVector<Value> fullSizes;
  for (Value size : allocOp.getSizes()) {
    fullOffsets.push_back(zero);
    fullSizes.push_back(size);
  }

  allocNode->forEachChildNode([&](NodeBase *child) {
    auto *acqNode = dyn_cast<DbAcquireNode>(child);
    if (!acqNode)
      return;
    DbAcquireOp acqOp = acqNode->getDbAcquireOp();
    if (!acqOp)
      return;
    acqOp.getOffsetsMutable().assign(fullOffsets);
    acqOp.getSizesMutable().assign(fullSizes);
    /// Clear partition hints for coarse allocations to avoid redundant
    /// per-task hint plumbing and enable invariant acquire hoisting.
    acqOp.getPartitionIndicesMutable().clear();
    acqOp.getPartitionOffsetsMutable().clear();
    acqOp.getPartitionSizesMutable().clear();
    /// Keep acquire partition attribute consistent with coarse allocation.
    setPartitionMode(acqOp.getOperation(), PartitionMode::coarse);
  });
}

} // namespace

namespace {
struct DbPartitioningPass
    : public arts::DbPartitioningBase<DbPartitioningPass> {
  DbPartitioningPass(ArtsAnalysisManager *AM) : AM(AM) {
    assert(AM && "ArtsAnalysisManager must be provided externally");
  }

  void runOnOperation() override;

  ///===--------------------------------------------------------------------===///
  /// Main Partitioning Methods
  ///===--------------------------------------------------------------------===///

  /// Iterates over allocations and applies partitioning rewriters.
  bool partitionDb();

  ///===--------------------------------------------------------------------===///
  /// Stencil Bounds Analysis
  ///===--------------------------------------------------------------------===///

  /// Analyze stencil access patterns and generate bounds check flags.
  void analyzeStencilBounds();

  ///===--------------------------------------------------------------------===///
  /// Graph Management
  ///===--------------------------------------------------------------------===///

  /// Invalidate and rebuild the datablock graph after transformations.
  void invalidateAndRebuildGraph();

  /// Partition an allocation based on per-acquire analysis and heuristics
  FailureOr<DbAllocOp> partitionAlloc(DbAllocOp allocOp,
                                      DbAllocNode *allocNode);

  /// Expand multi-entry acquires into separate DbAcquireOps.nt.
  bool expandMultiEntryAcquires();

private:
  ModuleOp module;
  ArtsAnalysisManager *AM = nullptr;
};
} // namespace

void DbPartitioningPass::runOnOperation() {
  bool changed = false;
  module = getOperation();

  ARTS_INFO_HEADER(DbPartitioningPass);
  ARTS_DEBUG_REGION(module.dump(););

  assert(AM && "ArtsAnalysisManager must be provided externally");

  /// Phase 0: Expand multi-entry acquires into separate DbAcquireOps.
  /// This must happen BEFORE graph construction so each acquire can be
  /// analyzed independently.
  changed |= expandMultiEntryAcquires();

  /// Graph construction and analysis
  invalidateAndRebuildGraph();

  /// Partition DBs and analyze stencil patterns for bounds checking
  changed |= partitionDb();
  analyzeStencilBounds();

  if (changed) {
    ARTS_INFO(" Module has changed, invalidating analyses");
    AM->invalidate();
  }

  ARTS_INFO_FOOTER(DbPartitioningPass);
  ARTS_DEBUG_REGION(module.dump(););
}

namespace {
struct EdtAcquireUse {
  EdtOp edt = nullptr;
  BlockArgument blockArg;
};

static EdtAcquireUse findEdtUse(DbAcquireOp acquire) {
  EdtAcquireUse use;
  for (Operation *user : acquire.getPtr().getUsers()) {
    auto edtOp = dyn_cast<EdtOp>(user);
    if (!edtOp)
      continue;
    use.edt = edtOp;
    for (auto [idx, dep] : llvm::enumerate(edtOp.getDependencies())) {
      if (dep == acquire.getPtr()) {
        use.blockArg = edtOp.getBody().front().getArgument(idx);
        break;
      }
    }
    break;
  }
  return use;
}

static SmallVector<DbAcquireOp> createExpandedAcquires(DbAcquireOp original,
                                                       OpBuilder &builder) {
  SmallVector<DbAcquireOp> expanded;
  Location loc = original.getLoc();
  size_t numEntries = original.getNumPartitionEntries();

  for (size_t i = 0; i < numEntries; ++i) {
    PartitionMode entryMode = original.getPartitionEntryMode(i);
    SmallVector<Value> entryIndices = original.getPartitionIndicesForEntry(i);
    SmallVector<Value> entryOffsets = original.getPartitionOffsetsForEntry(i);
    SmallVector<Value> entrySizes = original.getPartitionSizesForEntry(i);

    ARTS_DEBUG("    Entry " << i << ": mode=" << static_cast<int>(entryMode)
                            << ", indices=" << entryIndices.size()
                            << ", offsets=" << entryOffsets.size());

    auto newAcquire = builder.create<DbAcquireOp>(
        loc, original.getMode(), original.getSourceGuid(),
        original.getSourcePtr(), entryMode,
        /*indices=*/SmallVector<Value>{},
        /*offsets=*/
        SmallVector<Value>(original.getOffsets().begin(),
                           original.getOffsets().end()),
        /*sizes=*/
        SmallVector<Value>(original.getSizes().begin(),
                           original.getSizes().end()),
        /*partition_indices=*/entryIndices,
        /*partition_offsets=*/entryOffsets,
        /*partition_sizes=*/entrySizes);

    /// Preserve per-entry segmentation for single-entry acquires so
    /// getPartitionInfos() can infer dimensionality.
    SmallVector<int32_t> indicesSegs = {
        static_cast<int32_t>(entryIndices.size())};
    SmallVector<int32_t> offsetsSegs = {
        static_cast<int32_t>(entryOffsets.size())};
    SmallVector<int32_t> sizesSegs = {static_cast<int32_t>(entrySizes.size())};
    SmallVector<int32_t> entryModes = {static_cast<int32_t>(entryMode)};
    newAcquire.setPartitionSegments(indicesSegs, offsetsSegs, sizesSegs,
                                    entryModes);

    expanded.push_back(newAcquire);
    ARTS_DEBUG("    Created expanded acquire: " << newAcquire);
  }

  return expanded;
}

static SmallVector<Value> rebuildEdtDeps(EdtOp edt, DbAcquireOp original,
                                         ArrayRef<DbAcquireOp> expanded) {
  SmallVector<Value> deps;
  for (Value dep : edt.getDependencies()) {
    if (dep == original.getPtr()) {
      for (DbAcquireOp acq : expanded)
        deps.push_back(acq.getPtr());
    } else {
      deps.push_back(dep);
    }
  }
  return deps;
}

static SmallVector<BlockArgument>
insertExpandedBlockArgs(EdtOp edt, BlockArgument originalArg, size_t numEntries,
                        Type argType, Location loc) {
  Block &edtBlock = edt.getBody().front();
  SmallVector<BlockArgument> args;
  args.push_back(originalArg);
  for (size_t i = 1; i < numEntries; ++i) {
    unsigned insertPos = originalArg.getArgNumber() + i;
    BlockArgument newArg = edtBlock.insertArgument(insertPos, argType, loc);
    args.push_back(newArg);
  }
  return args;
}

static SmallVector<DbRefOp> collectDbRefUsers(BlockArgument arg) {
  SmallVector<DbRefOp> refs;
  for (Operation *user : arg.getUsers())
    if (auto dbRef = dyn_cast<DbRefOp>(user))
      refs.push_back(dbRef);
  return refs;
}

static SmallVector<Value> collectAccessIndices(DbRefOp dbRef) {
  SmallVector<Value> indices;
  for (Operation *refUser : dbRef.getResult().getUsers()) {
    if (auto load = dyn_cast<memref::LoadOp>(refUser)) {
      indices.assign(load.getIndices().begin(), load.getIndices().end());
      break;
    }
    if (auto store = dyn_cast<memref::StoreOp>(refUser)) {
      indices.assign(store.getIndices().begin(), store.getIndices().end());
      break;
    }
  }
  return indices;
}

static SmallVector<Value> collectAccessIndicesFromUser(Operation *refUser) {
  SmallVector<Value> indices;
  if (auto load = dyn_cast<memref::LoadOp>(refUser)) {
    indices.assign(load.getIndices().begin(), load.getIndices().end());
    return indices;
  }
  if (auto store = dyn_cast<memref::StoreOp>(refUser)) {
    indices.assign(store.getIndices().begin(), store.getIndices().end());
    return indices;
  }
  return indices;
}

struct EntryMatch {
  size_t index = 0;
  bool found = false;
  int score = -1;
};

static SmallVector<Value> getEntryAnchors(DbAcquireOp original, size_t entry) {
  SmallVector<Value> anchors = original.getPartitionIndicesForEntry(entry);
  if (!anchors.empty())
    return anchors;
  return original.getPartitionOffsetsForEntry(entry);
}

static bool indexMatchesAnchor(Value idx, Value anchor) {
  if (!idx || !anchor)
    return false;
  if (idx == anchor || ValueUtils::dependsOn(idx, anchor))
    return true;
  if (auto cast = idx.getDefiningOp<arith::IndexCastOp>())
    idx = cast.getIn();
  if (auto blockArg = idx.dyn_cast<BlockArgument>()) {
    if (auto forOp = dyn_cast<scf::ForOp>(blockArg.getOwner()->getParentOp())) {
      if (blockArg == forOp.getInductionVar()) {
        Value lb = forOp.getLowerBound();
        if (lb == anchor || ValueUtils::dependsOn(lb, anchor))
          return true;
      }
    }
  }
  return false;
}

static int scoreEntry(ArrayRef<Value> indices, ArrayRef<Value> anchors) {
  int score = 0;
  size_t n = std::min(indices.size(), anchors.size());
  for (size_t d = 0; d < n; ++d) {
    Value idx = indices[d];
    Value anchor = anchors[d];
    if (!idx || !anchor)
      continue;
    if (idx == anchor) {
      score += 2;
      continue;
    }
    if (indexMatchesAnchor(idx, anchor))
      score += 1;
  }
  return score;
}

static EntryMatch matchDbRefEntry(DbRefOp dbRef, ArrayRef<Value> accessIndices,
                                  DbAcquireOp original, size_t numEntries) {
  EntryMatch match;

  auto tryMatch = [&](ArrayRef<Value> indices) {
    if (indices.empty())
      return;
    for (size_t i = 0; i < numEntries; ++i) {
      SmallVector<Value> anchors = getEntryAnchors(original, i);
      if (anchors.empty())
        continue;
      int score = scoreEntry(indices, anchors);
      if (score > match.score) {
        match.score = score;
        match.index = i;
      }
    }
  };

  SmallVector<Value> dbRefIndices(dbRef.getIndices().begin(),
                                  dbRef.getIndices().end());
  tryMatch(dbRefIndices);
  tryMatch(accessIndices);
  match.found = match.score > 0;
  return match;
}

} // namespace

/// Expand multi-entry acquires into separate DbAcquireOps.
/// When an OpenMP task has `depend(in: A[i][j], A[i-1][j])`, CreateDbs captures
/// both entries in a single DbAcquireOp with segment attributes. This function
/// expands each entry into its own acquire with a corresponding EDT block arg.
///
/// BEFORE:
///   %acq = arts.db_acquire [inout] (%guid, %ptr)
///     partition_indices = [%i, %j, %i_minus_1, %j]
///     partition_indices_segments = [2, 2]  /// Two entries, each with 2
///     indices partition_entry_modes = [fine_grained, fine_grained]
///   arts.edt (%acq) { ^bb0(%arg0): ... }
///
/// AFTER:
///   %acq0 = arts.db_acquire [inout] (%guid, %ptr)
///     partitioning(fine_grained, indices[%i, %j])
///   %acq1 = arts.db_acquire [inout] (%guid, %ptr)
///     partitioning(fine_grained, indices[%i_minus_1, %j])
///   arts.edt (%acq0, %acq1) { ^bb0(%arg0, %arg1): ... }
bool DbPartitioningPass::expandMultiEntryAcquires() {
  ARTS_DEBUG_HEADER(ExpandMultiEntryAcquires);
  bool changed = false;

  SmallVector<DbAcquireOp> acquiresToExpand;
  module.walk([&](DbAcquireOp acquire) {
    if (acquire.hasMultiplePartitionEntries())
      acquiresToExpand.push_back(acquire);
  });

  if (acquiresToExpand.empty()) {
    ARTS_DEBUG("  No multi-entry acquires to expand");
    return false;
  }

  ARTS_DEBUG("  Found " << acquiresToExpand.size()
                        << " multi-entry acquires to expand");

  for (DbAcquireOp original : acquiresToExpand) {
    size_t numEntries = original.getNumPartitionEntries();
    if (numEntries <= 1)
      continue;

    /// Check for stencil pattern - skip expansion only when we need ESD.
    /// If the stencil entries are explicit fine-grained deps (e.g., u[i-1],
    /// u[i], u[i+1]), prefer expansion to element-wise acquires.
    int64_t minOffset = 0, maxOffset = 0;
    if (DatablockUtils::hasMultiEntryStencilPattern(original, minOffset,
                                                    maxOffset)) {
      bool allFineGrainedEntries = true;
      for (size_t entryIdx = 0; entryIdx < numEntries; ++entryIdx) {
        if (original.getPartitionEntryMode(entryIdx) !=
            PartitionMode::fine_grained) {
          allFineGrainedEntries = false;
          break;
        }
      }

      if (!allFineGrainedEntries) {
        ARTS_DEBUG("  Stencil pattern detected for acquire with "
                   << numEntries << " entries: haloLeft=" << (-minOffset)
                   << ", haloRight=" << maxOffset
                   << " - skipping expansion for ESD mode");

        /// Set stencil partition mode to preserve multi-entry structure
        /// The partitioning pass will handle this with stencil/ESD mode
        original.setPartitionModeAttr(PartitionModeAttr::get(
            original.getContext(), PartitionMode::stencil));
        continue;
      }

      ARTS_DEBUG("  Stencil pattern detected for acquire with "
                 << numEntries << " entries"
                 << " - expanding for explicit fine-grained deps");
    }

    ARTS_DEBUG("  Expanding acquire with " << numEntries
                                           << " entries: " << original);

    OpBuilder builder(original);

    /// Find the EDT that uses this acquire
    EdtAcquireUse use = findEdtUse(original);
    if (!use.edt) {
      ARTS_DEBUG("    No EDT user found, skipping expansion");
      continue;
    }

    /// Create separate acquires for each partition entry
    SmallVector<DbAcquireOp> expandedAcquires =
        createExpandedAcquires(original, builder);

    /// Update EDT dependencies: replace original with all expanded acquires
    use.edt.setDependencies(
        rebuildEdtDeps(use.edt, original, expandedAcquires));

    /// Add new block arguments for expanded acquires (first replaces original)
    Type argType = use.blockArg.getType();
    SmallVector<BlockArgument> newBlockArgs = insertExpandedBlockArgs(
        use.edt, use.blockArg, numEntries, argType, original.getLoc());

    /// Remap DbRef operations to use their correct block arguments.
    for (DbRefOp dbRef : collectDbRefUsers(use.blockArg)) {
      SmallVector<Operation *> users(dbRef.getResult().getUsers().begin(),
                                     dbRef.getResult().getUsers().end());
      SmallVector<std::pair<Operation *, size_t>> matchedUsers;
      SmallVector<Operation *> unmatchedUsers;
      llvm::DenseMap<size_t, unsigned> entryCounts;

      for (Operation *user : users) {
        SmallVector<Value> accessIndices = collectAccessIndicesFromUser(user);
        if (accessIndices.empty()) {
          unmatchedUsers.push_back(user);
          continue;
        }

        EntryMatch match =
            matchDbRefEntry(dbRef, accessIndices, original, numEntries);
        if (!match.found && numEntries > 1) {
          ARTS_DEBUG("      WARNING: Could not match db_ref user to partition "
                     "entry, defaulting to entry 0");
        }

        ARTS_DEBUG("      db_ref user matched entry "
                   << match.index
                   << " (access indices: " << accessIndices.size()
                   << ", found: " << match.found << ")");

        matchedUsers.emplace_back(user, match.index);
        entryCounts[match.index]++;
      }

      if (entryCounts.empty()) {
        SmallVector<Value> accessIndices = collectAccessIndices(dbRef);
        EntryMatch match =
            matchDbRefEntry(dbRef, accessIndices, original, numEntries);

        if (!match.found && numEntries > 1) {
          ARTS_DEBUG(
              "      WARNING: Could not match db_ref to partition entry, "
              "defaulting to entry 0");
        }

        ARTS_DEBUG("      db_ref matched entry "
                   << match.index
                   << " (access indices: " << accessIndices.size()
                   << ", found: " << match.found << ")");

        dbRef.getSourceMutable().assign(newBlockArgs[match.index]);
        continue;
      }

      if (entryCounts.size() == 1) {
        size_t entry = entryCounts.begin()->first;
        ARTS_DEBUG("      db_ref matched entry " << entry
                                                 << " (single-entry match)");
        dbRef.getSourceMutable().assign(newBlockArgs[entry]);
        continue;
      }

      /// Multiple entries matched: clone db_ref per entry and remap uses.
      OpBuilder refBuilder(dbRef);
      llvm::DenseMap<size_t, DbRefOp> entryRefs;
      for (const auto &entryPair : entryCounts) {
        size_t entry = entryPair.first;
        DbRefOp newRef =
            refBuilder.create<DbRefOp>(dbRef.getLoc(), dbRef.getType(),
                                       newBlockArgs[entry], dbRef.getIndices());
        entryRefs[entry] = newRef;
      }

      size_t fallbackEntry = entryRefs.count(0) ? 0 : entryRefs.begin()->first;
      DbRefOp fallbackRef = entryRefs[fallbackEntry];

      for (const auto &matchPair : matchedUsers) {
        Operation *user = matchPair.first;
        size_t entry = matchPair.second;
        DbRefOp refForEntry = entryRefs[entry];
        user->replaceUsesOfWith(dbRef.getResult(), refForEntry.getResult());
      }

      for (Operation *user : unmatchedUsers) {
        user->replaceUsesOfWith(dbRef.getResult(), fallbackRef.getResult());
      }

      dbRef.erase();
    }

    /// Create db_release operations for new block arguments (entries 1+)
    /// The original block arg (entry 0) already has a release from CreateDbs
    DbReleaseOp existingRelease = nullptr;
    for (Operation *user : use.blockArg.getUsers()) {
      if (auto rel = dyn_cast<DbReleaseOp>(user)) {
        existingRelease = rel;
        break;
      }
    }
    if (existingRelease) {
      OpBuilder releaseBuilder(existingRelease);
      for (size_t i = 1; i < numEntries; ++i) {
        releaseBuilder.create<DbReleaseOp>(original.getLoc(), newBlockArgs[i]);
      }
      ARTS_DEBUG("    Created " << (numEntries - 1)
                                << " db_release ops for expanded block args");
    }

    /// Erase original acquire
    original.erase();
    changed = true;

    ARTS_DEBUG("    Expanded into " << expandedAcquires.size()
                                    << " separate acquires");
  }

  return changed;
}

/// Partition allocations based on disjoint access proof.
bool DbPartitioningPass::partitionDb() {
  ARTS_DEBUG_HEADER(PartitionDb);
  bool changed = false;

  /// PHASE 1: Partition allocations
  module.walk([&](func::FuncOp func) {
    DbGraph &graph = AM->getDbGraph(func);
    graph.forEachAllocNode([&](DbAllocNode *allocNode) {
      if (!allocNode)
        return;
      DbAllocOp allocOp = allocNode->getDbAllocOp();
      if (!allocOp || !allocNode->canBePartitioned())
        return;

      auto promotedOr = partitionAlloc(allocOp, allocNode);
      if (failed(promotedOr))
        return;

      DbAllocOp promoted = promotedOr.value();
      if (promoted.getOperation() != allocOp.getOperation())
        changed = true;
    });
  });

  if (changed)
    invalidateAndRebuildGraph();

  ARTS_DEBUG_FOOTER(PartitionDb);
  return changed;
}

/// Analyze allocation acquires and apply appropriate partitioning rewriter.
FailureOr<DbAllocOp>
DbPartitioningPass::partitionAlloc(DbAllocOp allocOp, DbAllocNode *allocNode) {
  if (!allocOp || !allocOp.getOperation())
    return failure();

  if (allocOp.getElementSizes().empty())
    return failure();

  auto &heuristics = AM->getHeuristicsConfig();

  /// Step 0: Check existing partition attribute
  if (auto existingMode = getPartitionMode(allocOp)) {
    /// Only skip if SUCCESSFULLY partitioned (element_wise or chunked)
    if (*existingMode != PartitionMode::coarse) {
      ARTS_DEBUG("  Already partitioned as "
                 << mlir::arts::stringifyPartitionMode(*existingMode)
                 << " - SKIP");
      heuristics.recordDecision(
          "Partition-AlreadyPartitioned", false,
          "allocation already has partition attribute, skipping",
          allocOp.getOperation(), {});
      return allocOp; /// Skip - already partitioned!
    }
    /// mode == none means CreateDbs couldn't partition - continue to re-analyze
    ARTS_DEBUG("  Partition mode is "
               << mlir::arts::stringifyPartitionMode(*existingMode)
               << " - re-analyzing with loop structure");
    heuristics.recordDecision(
        "Partition-ReanalyzeNone", true,
        "CreateDbs set none, attempting loop-based block detection",
        allocOp.getOperation(), {});
  }

  /// Already fine-grained by structure - skip
  if (allocNode && DatablockUtils::isFineGrained(allocNode->getDbAllocOp())) {
    ARTS_DEBUG("  Already fine-grained by structure - SKIP");
    heuristics.recordDecision(
        "Partition-AlreadyFineGrained", false,
        "allocation already fine-grained by structure, skipping",
        allocOp.getOperation(), {});
    return allocOp;
  }

  OpBuilder builder(allocOp);
  Location loc = allocOp.getLoc();
  builder.setInsertionPoint(allocOp);

  /// Step 1: Analyze each acquire's PartitionMode
  SmallVector<AcquirePartitionInfo> acquireInfos;
  if (allocNode) {
    allocNode->forEachChildNode([&](NodeBase *child) {
      auto *acqNode = dyn_cast<DbAcquireNode>(child);
      if (!acqNode)
        return;

      auto info = computeAcquirePartitionInfo(acqNode->getDbAcquireOp(),
                                              acqNode, builder);
      if (!info.isValid) {
        ARTS_DEBUG("  Acquire analysis failed for "
                   << acqNode->getDbAcquireOp());
      }
      acquireInfos.push_back(info);
    });
  }

  if (acquireInfos.empty()) {
    ARTS_DEBUG("  No acquires to analyze - keeping original");
    heuristics.recordDecision(
        "Partition-NoAcquires", false,
        "no acquires to analyze, keeping original allocation",
        allocOp.getOperation(), {});
    return allocOp;
  }

  /// Step 2: Build PartitioningContext and check for non-leading offsets
  PartitioningContext ctx;
  ctx.machine = &AM->getAbstractMachine();
  bool canUseBlock = true; /// Assume chunked is possible until proven otherwise

  if (allocNode) {
    ctx.accessPatterns = allocNode->summarizeAcquirePatterns();
    ctx.isUniformAccess = ctx.accessPatterns.hasUniform;
    ARTS_DEBUG("  Access patterns: hasUniform="
               << ctx.accessPatterns.hasUniform
               << ", hasStencil=" << ctx.accessPatterns.hasStencil
               << ", hasIndexed=" << ctx.accessPatterns.hasIndexed
               << ", isMixed=" << ctx.accessPatterns.isMixed());

    if (!allocOp.getElementSizes().empty()) {
      int64_t staticFirstDim = 0;
      if (ValueUtils::getConstantIndex(allocOp.getElementSizes().front(),
                                       staticFirstDim)) {
        ctx.totalElements = staticFirstDim;
      }
    }

    allocNode->forEachChildNode([&](NodeBase *child) {
      auto *acqNode = dyn_cast<DbAcquireNode>(child);
      if (!acqNode)
        return;
      DbAcquireOp acqOp = acqNode->getDbAcquireOp();
      if (!acqOp)
        return;
      bool hasIndirect = acqNode->hasIndirectAccess();
      if (hasIndirect) {
        ctx.hasIndirectAccess = true;
        if (acqNode->hasLoads())
          ctx.hasIndirectRead = true;
        if (acqNode->hasStores())
          ctx.hasIndirectWrite = true;
      }
      if (acqNode->hasDirectAccess())
        ctx.hasDirectAccess = true;
    });

    /// Note: Indirect access is handled via full-range acquires, not by
    /// disabling chunked. Full-range acquires allow indirect reads to access
    /// all chunks.
    if (ctx.hasIndirectAccess) {
      ARTS_DEBUG("  Indirect access detected - indirect acquires will use "
                 "full-range");
    }

    /// Build per-acquire capabilities for heuristic voting
    size_t idx = 0;
    allocNode->forEachChildNode([&](NodeBase *child) {
      auto *acqNode = dyn_cast<DbAcquireNode>(child);
      if (!acqNode || idx >= acquireInfos.size())
        return;

      auto &acqInfo = acquireInfos[idx];

      /// Check access patterns for block capability decisions.
      bool hasIndirect = acqNode->hasIndirectAccess();

      /// Read partition capability from acquire's attribute.
      /// ForLowering sets this to 'chunked' when adding offset/size hints.
      /// CreateDbs sets this based on DbControlOp analysis.
      auto acquire = acqNode->getDbAcquireOp();
      auto acquireMode = getPartitionMode(acquire.getOperation());
      bool hasBlockHints = !acquire.getPartitionOffsets().empty() ||
                           !acquire.getPartitionSizes().empty();
      bool inferredBlock =
          !acqInfo.partitionOffsets.empty() && !acqInfo.partitionSizes.empty();
      bool thisAcquireCanBlock =
          (acquireMode && (*acquireMode == PartitionMode::block ||
                           *acquireMode == PartitionMode::stencil)) ||
          hasBlockHints || inferredBlock;
      /// If access is indirect and no explicit hints exist, still allow block
      /// partitioning so we can fall back to full-range acquires on a blocked
      /// allocation (improves parallelism vs coarse).
      if (!thisAcquireCanBlock &&
          (hasIndirect || (allocNode && allocNode->hasNonAffineAccesses &&
                           *allocNode->hasNonAffineAccesses))) {
        if (!ctx.totalElements || *ctx.totalElements > 1) {
          thisAcquireCanBlock = true;
          ARTS_DEBUG("  Non-affine/indirect access without hints; enabling "
                     "block capability for full-range acquires");
        }
      }
      if (!thisAcquireCanBlock &&
          acqNode->getAccessPattern() == AccessPattern::Stencil)
        thisAcquireCanBlock = true;
      bool thisAcquireCanElementWise =
          acquireMode && *acquireMode == PartitionMode::fine_grained;

      /// If the partition offset does not map to the access pattern, block
      /// partitioning would select the wrong dimension (non-leading) and is
      /// unsafe. Disable block capability in that case.
      SmallVector<Value> offsetVals = getPartitionOffsetsND(acqNode, &acqInfo);
      unsigned offsetIdx = 0;
      Value partitionOffset =
          pickRepresentativePartitionOffset(offsetVals, &offsetIdx);
      if (partitionOffset) {
        auto dimOpt = acqNode->getPartitionOffsetDim(partitionOffset,
                                                     /*requireLeading=*/false);
        if (!dimOpt) {
          ARTS_DEBUG("  Partition offset incompatible with access pattern; "
                     "disabling block capability");
          thisAcquireCanBlock = false;
        }
      }

      /// Build AcquireInfo for heuristic voting
      AcquireInfo info;
      info.accessMode = acquire.getMode();
      info.canElementWise =
          thisAcquireCanElementWise || !acquire.getPartitionIndices().empty();
      info.canBlock = canUseBlock && thisAcquireCanBlock;

      /// Populate unified partition infos from DbAcquireOp.
      /// Heuristics will compute uniformity and decide outerRank.
      info.partitionInfos = acquire.getPartitionInfos();

      /// Populate partition mode from AcquirePartitionInfo
      if (idx < acquireInfos.size()) {
        const auto &acqPartInfo = acquireInfos[idx];
        info.partitionMode = acqPartInfo.mode;
      }

      ctx.acquires.push_back(info);
      ARTS_DEBUG("    Acquire["
                 << idx << "]: mode=" << static_cast<int>(info.accessMode)
                 << ", canEW=" << info.canElementWise
                 << ", canBlock=" << info.canBlock << ", acquireAttr="
                 << (acquireMode ? static_cast<int>(*acquireMode) : -1));
      ++idx;
    });

    /// Collect DbAcquireNode* and partition offsets for heuristic decisions
    SmallVector<DbAcquireNode *> acqNodes;
    SmallVector<Value> partitionOffsets;
    idx = 0;
    allocNode->forEachChildNode([&](NodeBase *child) {
      if (auto *acqNode = dyn_cast<DbAcquireNode>(child)) {
        acqNodes.push_back(acqNode);
        auto offsets = getPartitionOffsetsND(
            acqNode, idx < acquireInfos.size() ? &acquireInfos[idx] : nullptr);
        partitionOffsets.push_back(offsets.empty() ? Value() : offsets.front());
        ++idx;
      }
    });

    /// Get per-acquire decisions from heuristics (calls needsFullRange)
    auto acquireDecisions =
        heuristics.getAcquireDecisions(ctx, acqNodes, partitionOffsets);

    /// Apply decisions to acquireInfos
    for (size_t i = 0; i < acquireDecisions.size() && i < acquireInfos.size();
         ++i) {
      if (acquireDecisions[i].needsFullRange) {
        acquireInfos[i].needsFullRange = true;
      }
    }
  }

  /// Step 3: Reconcile per-acquire capabilities into allocation-level mode.
  AcquireModeReconcileResult modeSummary =
      reconcileAcquireModes(ctx, acquireInfos, canUseBlock);
  bool modesConsistent = modeSummary.modesConsistent;
  if (!modesConsistent)
    ARTS_DEBUG("  Inconsistent partition modes across acquires");
  if (modeSummary.allBlockFullRange) {
    ARTS_DEBUG("  All block-capable acquires are full-range; deferring to "
               "heuristics");
  }

  ARTS_DEBUG("  Per-acquire voting: canEW="
             << ctx.canElementWise << ", canBlock=" << ctx.canBlock
             << ", modesConsistent=" << modesConsistent);

  /// If no structural capability found, let heuristics handle it
  if (!ctx.canElementWise && !ctx.canBlock) {
    ARTS_DEBUG("  Coarse mode - no partitioning needed");
    resetCoarseAcquireRanges(allocOp, allocNode, builder);
    heuristics.recordDecision(
        "Partition-CoarseMode", false,
        "all acquires are coarse mode, no partitioning needed",
        allocOp.getOperation(), {});
    return allocOp;
  }

  /// Set pinnedDimCount for logging (heuristics compute min/max on-the-fly)
  ctx.pinnedDimCount = ctx.maxPinnedDimCount();
  ARTS_DEBUG("  Partition dim counts: min=" << ctx.minPinnedDimCount()
                                            << ", max=" << ctx.pinnedDimCount);

  /// Get access mode from allocNode
  if (allocNode)
    ctx.accessMode = allocOp.getMode();

  /// Get memref rank
  if (auto memrefType = allocOp.getElementType().dyn_cast<MemRefType>()) {
    ctx.memrefRank = memrefType.getRank();
    ctx.elementTypeIsMemRef = true;
  } else {
    ctx.memrefRank = allocOp.getElementSizes().size();
    ctx.elementTypeIsMemRef = false;
  }

  /// Step 4: Initial heuristics arbitration.
  PartitionDecisionArbiterResult arbiterResult =
      arbitrateInitialPartitionDecision(ctx, acquireInfos, allocOp, heuristics);
  PartitioningDecision decision = arbiterResult.decision;
  SmallVector<Value> blockSizesForNDBlock =
      std::move(arbiterResult.blockSizesForNDBlock);

  if (decision.isCoarse()) {
    ARTS_DEBUG("  Heuristics chose coarse - keeping original");
    resetCoarseAcquireRanges(allocOp, allocNode, builder);
    heuristics.recordDecision(
        "Partition-HeuristicsCoarse", false,
        "heuristics chose coarse allocation, keeping original",
        allocOp.getOperation(),
        {{"canBlock", ctx.canBlock ? 1 : 0},
         {"canElementWise", ctx.canElementWise ? 1 : 0}});
    return allocOp;
  }

  /// Disabled: versioned partitioning path is currently off.

  /// For element-wise partitioning, validate that partition indices match
  /// actual accesses. If not (block-wise pattern detected), fall back to
  /// N-D block mode for proper parallelism.
  ///
  /// Block-wise patterns (like Cholesky's 2D block dependencies) use:
  ///   - Partition indices as block identifiers (not element indices)
  ///   - Loop step as block size per dimension
  bool skipAcquireInfoCheck = false;
  if (decision.isFineGrained() && ctx.accessPatterns.hasIndexed) {
    bool validElementWise = validateElementWiseIndices(acquireInfos, allocNode);
    if (validElementWise) {
      /// Valid element-wise: we can skip per-acquire partition info check
      skipAcquireInfoCheck = true;
    } else {
      /// Block-wise pattern detected. Extract block sizes from loop steps
      /// for each partition dimension and use N-D block partitioning.
      ARTS_DEBUG("  Block-wise pattern detected - extracting N-D block sizes");

      /// Find the acquire with partition indices to extract block sizes
      for (auto &info : acquireInfos) {
        auto partitionIndices = info.acquire.getPartitionIndices();
        if (partitionIndices.empty())
          continue;

        /// For each partition index, try to find the enclosing loop step
        for (Value idx : partitionIndices) {
          Value blockSize;
          Operation *parent = info.acquire->getParentOp();
          while (parent) {
            if (auto forOp = dyn_cast<scf::ForOp>(parent)) {
              Value loopIV = forOp.getInductionVar();
              if (ValueUtils::dependsOn(idx, loopIV)) {
                blockSize = forOp.getStep();
                ARTS_DEBUG(
                    "    Found block size from loop step: " << blockSize);
                break;
              }
            }
            parent = parent->getParentOp();
          }
          if (blockSize)
            blockSizesForNDBlock.push_back(blockSize);
        }
        if (!blockSizesForNDBlock.empty())
          break; /// Use first acquire with valid block sizes
      }

      if (!blockSizesForNDBlock.empty()) {
        /// Switch to N-D block mode with extracted block sizes
        unsigned nDims = blockSizesForNDBlock.size();
        decision = PartitioningDecision::blockND(
            ctx, nDims, "Fallback: N-D block-wise pattern");
        ARTS_DEBUG("  Falling back to N-D block mode with "
                   << nDims << " partitioned dimensions");
        heuristics.recordDecision(
            "Partition-ElementWiseFallbackToNDBlock", true,
            "block-wise pattern detected, using N-D block partitioning",
            allocOp.getOperation(), {{"numPartitionedDims", (int64_t)nDims}});
      } else {
        /// No block sizes found - fall back to coarse
        ARTS_DEBUG("  Cannot extract block sizes - falling back to coarse");
        heuristics.recordDecision("Partition-ElementWiseFallbackToCoarse",
                                  false,
                                  "block-wise pattern but no block sizes found",
                                  allocOp.getOperation(), {});
        resetCoarseAcquireRanges(allocOp, allocNode, builder);
        return allocOp;
      }
    }
  }

  /// Also try N-D extraction for explicit block/stencil decisions that haven't
  /// extracted block sizes yet. This handles cases like Cholesky where the
  /// heuristics already chose block mode but we need to extract the N-D
  /// block sizes from loop steps for proper multi-dimensional partitioning.
  /// Check for both indexed and uniform patterns (block-wise access).
  bool hasExplicitBlockSizes = false;
  for (const auto &info : acquireInfos) {
    if (info.mode != PartitionMode::block &&
        info.mode != PartitionMode::stencil)
      continue;
    for (Value sz : info.partitionSizes) {
      if (!sz)
        continue;
      Value stripped = ValueUtils::stripNumericCasts(sz);
      if (!ValueUtils::isOneConstant(stripped)) {
        hasExplicitBlockSizes = true;
        break;
      }
    }
    if (hasExplicitBlockSizes)
      break;
  }

  if (decision.isBlock() &&
      (ctx.accessPatterns.hasIndexed || ctx.accessPatterns.hasUniform) &&
      blockSizesForNDBlock.empty() && !hasExplicitBlockSizes) {
    ARTS_DEBUG(
        "  Block/stencil mode with indexed/uniform patterns - extracting "
        "N-D block sizes");
    for (auto &info : acquireInfos) {
      auto partitionIndices = info.acquire.getPartitionIndices();
      if (partitionIndices.empty())
        continue;

      /// For stencil acquires with multi-entry structure, use first entry's
      /// index to find the loop step (the base index, not the offset versions)
      SmallVector<Value> indicesToCheck;
      if (info.acquire.hasMultiplePartitionEntries()) {
        /// Find the center entry (offset 0) for stencil patterns
        size_t numEntries = info.acquire.getNumPartitionEntries();
        for (size_t i = 0; i < numEntries; ++i) {
          auto entryIndices = info.acquire.getPartitionIndicesForEntry(i);
          if (!entryIndices.empty())
            indicesToCheck.push_back(entryIndices[0]);
        }
        /// Use first unique index for loop detection
        if (!indicesToCheck.empty())
          indicesToCheck.resize(1);
      } else {
        indicesToCheck.assign(partitionIndices.begin(), partitionIndices.end());
      }

      /// For each partition index, try to find the enclosing loop step
      for (Value idx : indicesToCheck) {
        Value blockSize;
        Operation *parent = info.acquire->getParentOp();
        while (parent) {
          if (auto forOp = dyn_cast<scf::ForOp>(parent)) {
            Value loopIV = forOp.getInductionVar();
            if (ValueUtils::dependsOn(idx, loopIV)) {
              blockSize = forOp.getStep();
              ARTS_DEBUG("    Found block size from loop step: " << blockSize);
              break;
            }
          }
          parent = parent->getParentOp();
        }
        if (blockSize)
          blockSizesForNDBlock.push_back(blockSize);
      }
      if (!blockSizesForNDBlock.empty())
        break; /// Use first acquire with valid block sizes
    }

    if (!blockSizesForNDBlock.empty()) {
      /// For stencil mode, keep the mode but update ranks based on block sizes
      /// For non-stencil block mode, upgrade to N-D block mode
      unsigned nDims = blockSizesForNDBlock.size();
      if (decision.mode != PartitionMode::stencil) {
        decision = PartitioningDecision::blockND(ctx, nDims,
                                                 "N-D block pattern detected");
        ARTS_DEBUG("  Upgraded to N-D block mode with " << nDims
                                                        << " dimensions");
        heuristics.recordDecision(
            "Partition-BlockUpgradedToND", true,
            "block mode upgraded with N-D block sizes from loop steps",
            allocOp.getOperation(), {{"numPartitionedDims", (int64_t)nDims}});
      } else {
        /// Stencil mode: keep mode, just log block size extraction
        ARTS_DEBUG("  Stencil mode: extracted " << nDims << " block sizes");
      }
    }
  }

  bool allRewriteable =
      skipAcquireInfoCheck ||
      std::all_of(acquireInfos.begin(), acquireInfos.end(), [&](auto &info) {
        /// Full-range acquires don't need offset/size - they get full
        /// allocation
        if (info.needsFullRange)
          return true;
        return info.isValid && !info.partitionOffsets.empty() &&
               !info.partitionSizes.empty();
      });
  if (!allRewriteable) {
    ARTS_DEBUG("  Some acquires missing partition info - keeping original");
    heuristics.recordDecision(
        "Partition-MissingAcquireInfo", false,
        "some acquires missing partition info, cannot rewrite allocation",
        allocOp.getOperation(), {});
    return allocOp;
  }

  ARTS_DEBUG("  Decision: mode=" << getPartitionModeName(decision.mode)
                                 << ", outerRank=" << decision.outerRank
                                 << ", innerRank=" << decision.innerRank);

  /// Step 5a: Compute stencilInfo early (needed for inner size calculation)
  /// Must be done before computeAllocationShape() for stencil mode.
  std::optional<StencilInfo> stencilInfo;
  if (decision.mode == PartitionMode::stencil && allocNode) {
    StencilInfo info;
    info.haloLeft = 0;
    info.haloRight = 0;

    /// Collect stencil bounds from acquire nodes or multi-entry structure.
    /// For multi-entry stencil acquires (not expanded), extract halo bounds
    /// from the partition indices pattern using DatablockUtils.
    allocNode->forEachChildNode([&](NodeBase *child) {
      auto *acqNode = dyn_cast<DbAcquireNode>(child);
      if (!acqNode)
        return;
      DbAcquireOp acq = acqNode->getDbAcquireOp();
      if (!acq)
        return;

      /// First try: get bounds from DbAcquireNode analysis
      auto stencilBounds = acqNode->getStencilBounds();
      if (stencilBounds && stencilBounds->hasHalo()) {
        info.haloLeft = std::max(info.haloLeft, stencilBounds->haloLeft());
        info.haloRight = std::max(info.haloRight, stencilBounds->haloRight());
        return;
      }

      /// Second try: for multi-entry stencil acquires, extract from structure
      if (acq.hasMultiplePartitionEntries()) {
        int64_t minOffset = 0, maxOffset = 0;
        if (DatablockUtils::hasMultiEntryStencilPattern(acq, minOffset,
                                                        maxOffset)) {
          /// minOffset is negative (left halo), maxOffset is positive (right)
          int64_t haloL = -minOffset;
          int64_t haloR = maxOffset;
          info.haloLeft = std::max(info.haloLeft, haloL);
          info.haloRight = std::max(info.haloRight, haloR);
          ARTS_DEBUG("  Extracted stencil bounds from multi-entry: haloLeft="
                     << haloL << ", haloRight=" << haloR);
        }
      }
    });

    /// Get total rows from first element dimension.
    /// For stencil loops, the logical owned span excludes halo rows, so use
    /// (rows - haloLeft - haloRight) when possible.
    if (!allocOp.getElementSizes().empty()) {
      if (auto staticSize =
              arts::extractBlockSizeFromHint(allocOp.getElementSizes()[0]))
        info.totalRows =
            builder.create<arith::ConstantIndexOp>(loc, *staticSize);
      else
        info.totalRows = allocOp.getElementSizes()[0];

      if (info.totalRows && (info.haloLeft > 0 || info.haloRight > 0)) {
        if (!info.totalRows.getType().isIndex())
          info.totalRows = builder.create<arith::IndexCastOp>(
              loc, builder.getIndexType(), info.totalRows);

        Value haloTotal = builder.create<arith::ConstantIndexOp>(
            loc, info.haloLeft + info.haloRight);
        Value zero = builder.create<arith::ConstantIndexOp>(loc, 0);
        Value canSubtract = builder.create<arith::CmpIOp>(
            loc, arith::CmpIPredicate::uge, info.totalRows, haloTotal);
        Value reduced =
            builder.create<arith::SubIOp>(loc, info.totalRows, haloTotal);
        info.totalRows =
            builder.create<arith::SelectOp>(loc, canSubtract, reduced, zero);
      }
    }

    stencilInfo = info;
    ARTS_DEBUG("  Stencil mode (early): haloLeft="
               << info.haloLeft << ", haloRight=" << info.haloRight);

    /// Check: Require both halos for true ESD stencil mode.
    /// Single-sided patterns (only left or only right halo) break the
    /// 3-buffer approach which assumes data from both neighbors exists.
    if (info.haloLeft == 0 || info.haloRight == 0) {
      ARTS_DEBUG("  Single-sided halo detected (haloLeft="
                 << info.haloLeft << ", haloRight=" << info.haloRight
                 << ") - falling back to BLOCK mode");
      decision = PartitioningDecision::blockND(
          ctx, decision.outerRank,
          "Single-sided stencil pattern - using BLOCK instead");
      stencilInfo = std::nullopt; // Clear stencil info
    }
  }

  /// Step 5b: Compute sizes from decision (uses outerRank/innerRank)
  /// For block mode, resolve block-plan sizes/dimensions behind a dedicated
  /// contract and keep pass logic orchestration-focused.
  SmallVector<Value> blockSizesForPlan;
  SmallVector<unsigned> partitionedDimsForPlan;
  if (decision.isBlock()) {
    bool useNodesForFallback = false;
    if (AM) {
      ArtsAbstractMachine &machine = AM->getAbstractMachine();
      if (machine.hasValidNodeCount() && machine.getNodeCount() > 1)
        useNodesForFallback = true;
    }

    DbBlockPlanInput blockPlanInput{allocOp,
                                    acquireInfos,
                                    ctx.blockSize,
                                    /*dynamicBlockSizeHint=*/Value(),
                                    blockSizesForNDBlock,
                                    &builder,
                                    loc,
                                    useNodesForFallback};
    auto blockPlanOr = resolveDbBlockPlan(blockPlanInput);
    if (failed(blockPlanOr) || blockPlanOr->blockSizes.empty()) {
      ARTS_DEBUG("  No block-plan sizes available - falling back to coarse");
      heuristics.recordDecision(
          "Partition-NoBlockSize", false,
          "block mode requested but no block-plan sizes available",
          allocOp.getOperation(), {});
      resetCoarseAcquireRanges(allocOp, allocNode, builder);
      return allocOp;
    }

    blockSizesForPlan = std::move(blockPlanOr->blockSizes);
    partitionedDimsForPlan = std::move(blockPlanOr->partitionedDims);

    heuristics.recordDecision(
        "Partition-BlockPlanResolved", true,
        "resolved block sizes and partition dimensions via block-plan resolver",
        allocOp.getOperation(),
        {{"blockDims", static_cast<int64_t>(blockSizesForPlan.size())},
         {"partitionedDims",
          static_cast<int64_t>(partitionedDimsForPlan.size())}});

    /// Non-leading partitioning is not supported by stencil ESD. Downgrade
    /// to block mode when needed.
    if (decision.mode == PartitionMode::stencil &&
        !partitionedDimsForPlan.empty()) {
      bool nonLeading = false;
      for (unsigned i = 0; i < partitionedDimsForPlan.size(); ++i) {
        if (partitionedDimsForPlan[i] != i) {
          nonLeading = true;
          break;
        }
      }
      if (nonLeading) {
        decision = PartitioningDecision::blockND(
            ctx, partitionedDimsForPlan.size(),
            "Non-leading partition dims: disable stencil ESD");
        stencilInfo = std::nullopt;
      }
    }

    /// If an acquire's inferred partition dims disagree with the chosen plan,
    /// require full-range to preserve correctness (non-leading mismatch).
    if (!partitionedDimsForPlan.empty()) {
      for (auto &info : acquireInfos) {
        if (info.needsFullRange || info.partitionDims.empty())
          continue;
        bool same = info.partitionDims.size() == partitionedDimsForPlan.size();
        if (same) {
          for (unsigned i = 0; i < info.partitionDims.size(); ++i) {
            if (info.partitionDims[i] != partitionedDimsForPlan[i]) {
              same = false;
              break;
            }
          }
        }
        if (!same) {
          info.needsFullRange = true;
          ARTS_DEBUG("  Acquire partition dims mismatch with plan; "
                     "forcing full-range access");
        }
      }
    }
  }

  SmallVector<Value> newOuterSizes, newInnerSizes;
  computeAllocationShape(decision.mode, allocOp, decision, blockSizesForPlan,
                         partitionedDimsForPlan, newOuterSizes, newInnerSizes);

  if (newOuterSizes.empty()) {
    ARTS_DEBUG("  Size computation failed - keeping original");
    heuristics.recordDecision(
        "Partition-SizeComputationFailed", false,
        "failed to compute partition sizes, keeping original",
        allocOp.getOperation(), {});
    return allocOp;
  }

  /// Step 6: Apply DbRewriter
  DbRewritePlan plan(decision.mode);
  plan.blockSizes = blockSizesForPlan;
  plan.partitionedDims = partitionedDimsForPlan;
  plan.outerSizes.assign(newOuterSizes.begin(), newOuterSizes.end());
  plan.innerSizes.assign(newInnerSizes.begin(), newInnerSizes.end());

  ARTS_DEBUG("  Plan created: blockSizes="
             << plan.blockSizes.size() << ", outerRank=" << plan.outerRank()
             << ", innerRank=" << plan.innerRank()
             << ", numPartitionedDims=" << plan.numPartitionedDims());

  /// Mixed mode: set flag for full-range acquires
  bool hasMixedMode = llvm::any_of(
      acquireInfos, [](const auto &info) { return info.needsFullRange; });
  if (hasMixedMode && decision.isBlock()) {
    plan.isMixedMode = true;
    ARTS_DEBUG("  Mixed mode plan enabled");
  }

  /// ESD: Use early-computed stencilInfo for Stencil mode
  if (stencilInfo)
    plan.stencilInfo = *stencilInfo;
  /// Note: byte_offset/byte_size for ESD is computed in EdtLowering from
  /// element_offsets/element_sizes, using allocation's elementSizes for stride.

  /// Propagate stencil center offset to all acquires when using stencil mode.
  /// This keeps base offsets aligned for non-stencil accesses (e.g., outputs)
  /// when loop bounds are shifted (i starts at 1).
  if (decision.mode == PartitionMode::stencil) {
    std::optional<int64_t> centerOffset;
    for (auto &info : acquireInfos) {
      if (!info.acquire)
        continue;
      if (auto center = getStencilCenterOffset(info.acquire.getOperation())) {
        centerOffset = *center;
        break;
      }
    }
    if (centerOffset) {
      for (auto &info : acquireInfos) {
        if (!info.acquire)
          continue;
        if (!getStencilCenterOffset(info.acquire.getOperation()))
          setStencilCenterOffset(info.acquire.getOperation(), *centerOffset);
      }
    }
  }

  /// Build DbRewriteAcquire list from AcquirePartitionInfo
  /// Include ALL acquires - invalid ones get Coarse fallback (offset=0,
  /// size=totalSize)
  SmallVector<DbRewriteAcquire> rewriteAcquires;
  for (auto &info : acquireInfos) {
    if (!info.acquire)
      continue;

    DbRewriteAcquire rewriteInfo;
    rewriteInfo.acquire = info.acquire;
    rewriteInfo.partitionInfo.mode = decision.mode;

    if (info.acquire.hasMultiplePartitionEntries()) {
      ARTS_DEBUG("  Rewrite multi-entry acquire offset="
                 << (info.partitionOffsets.empty()
                         ? Value()
                         : info.partitionOffsets.front())
                 << " loc=" << info.acquire.getLoc());
    }

    DbAcquirePartitionView view;
    view.acquire = info.acquire;
    auto partitionIndices = info.acquire.getPartitionIndices();
    view.partitionIndices.assign(partitionIndices.begin(),
                                 partitionIndices.end());
    view.partitionOffsets.assign(info.partitionOffsets.begin(),
                                 info.partitionOffsets.end());
    view.partitionSizes.assign(info.partitionSizes.begin(),
                               info.partitionSizes.end());
    view.isValid = info.isValid;
    view.hasIndirectAccess = info.hasIndirectAccess;
    view.needsFullRange = info.needsFullRange;
    buildRewriteAcquire(decision.mode, view, allocOp, plan, rewriteInfo,
                        builder);

    rewriteAcquires.push_back(rewriteInfo);
  }

  auto rewriter = DbRewriter::create(allocOp, rewriteAcquires, plan);
  auto result = rewriter->apply(builder);

  /// Step 7: Set partition attribute on new alloc
  if (succeeded(result)) {
    setPartitionMode(*result, decision.mode);

    AM->getMetadataManager().transferMetadata(allocOp, result.value());

    /// Coarse allocation: clear partition hints on acquires to avoid
    /// unnecessary per-task hint plumbing and enable invariant hoisting.
    if (decision.isCoarse()) {
      for (auto &info : rewriteAcquires) {
        if (!info.acquire)
          continue;
        info.acquire.clearPartitionHints();
      }
    }

    /// Record successful partition decision
    StringRef modeName = getPartitionModeName(decision.mode);
    ARTS_DEBUG("  Set partition attribute: " << modeName);
    heuristics.recordDecision(
        "Partition-Success", true,
        ("successfully partitioned allocation to " + modeName).str(),
        result.value().getOperation(),
        {{"outerRank", static_cast<int64_t>(decision.outerRank)},
         {"innerRank", static_cast<int64_t>(decision.innerRank)},
         {"acquireCount", static_cast<int64_t>(acquireInfos.size())}});
  } else {
    /// Record partition failure
    heuristics.recordDecision(
        "Partition-RewriterFailed", false,
        "DbRewriter failed to apply partition transformation",
        allocOp.getOperation(), {});
  }

  return result;
}

void DbPartitioningPass::analyzeStencilBounds() {
  ARTS_DEBUG_HEADER(AnalyzeStencilBounds);
  lowerStencilAcquireBounds(module, AM->getLoopAnalysis());

  ARTS_DEBUG_FOOTER(AnalyzeStencilBounds);
}

void DbPartitioningPass::invalidateAndRebuildGraph() {
  module.walk([&](func::FuncOp func) {
    AM->invalidateFunction(func);
    (void)AM->getDbGraph(func);
  });
}

namespace mlir {
namespace arts {
std::unique_ptr<Pass> createDbPartitioningPass(ArtsAnalysisManager *AM) {
  return std::make_unique<DbPartitioningPass>(AM);
}
} // namespace arts
} // namespace mlir
