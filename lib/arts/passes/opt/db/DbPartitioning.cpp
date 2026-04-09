///==========================================================================///
/// DbPartitioning.cpp - Datablock partitioning controller pass
///
/// Controller responsibilities:
///   Phase 1: query canonical contracts via DbAnalysis plus derived graph
///            legality/detail evidence
///   Phase 2: assemble heuristic snapshots and obtain H1 decisions
///   Phase 3: reconcile/normalize those decisions for one allocation
///   Phase 4: build planner inputs and invoke the rewriter
///
/// Non-responsibilities:
///   - does not own canonical per-acquire semantics (IR contracts do)
///   - does not choose policy directly (DbHeuristics does)
///   - does not localize indices directly (rewriters/indexers do)
///
/// Transformation (shape-focused):
///   BEFORE:
///     %acq = arts.db_acquire ... partition_mode = coarse
///     (single full-range acquire)
///
///   AFTER (example block mode):
///     %acq = arts.db_acquire ... partition_mode = block
///       partition_offsets(...), partition_sizes(...)
///     (task-local range acquire; same program dependencies, smaller transfer)
///
/// Example:
///   coarse single-range acquire -> block/stencil/element-wise layout rewrite
///
/// This pass must preserve dependency semantics: it refines acquire metadata
/// and rewrite plans, but does not weaken EDT/event ordering.
///==========================================================================///

#include "arts/passes/opt/db/DbPartitioningInternal.h"

using namespace mlir;
using namespace mlir::func;
using namespace mlir::arith;
using namespace mlir::polygeist;
using namespace mlir::arts;
using namespace mlir::arts::db_partitioning;

#define GEN_PASS_DEF_DBPARTITIONING
#include "arts/passes/Passes.h.inc"

ARTS_DEBUG_SETUP(db_partitioning);

static const AnalysisKind kDbPartitioning_reads[] = {
    AnalysisKind::DbAnalysis, AnalysisKind::DbHeuristics,
    AnalysisKind::LoopAnalysis, AnalysisKind::AbstractMachine,
    AnalysisKind::MetadataManager};
static const AnalysisKind kDbPartitioning_invalidates[] = {
    AnalysisKind::DbAnalysis};
[[maybe_unused]] static const AnalysisDependencyInfo kDbPartitioning_deps = {
    kDbPartitioning_reads, kDbPartitioning_invalidates};

llvm::Statistic numMultiEntryAcquiresExpanded{
    "db_partitioning", "NumMultiEntryAcquiresExpanded",
    "Number of multi-entry DbAcquireOps expanded into per-entry acquires"};
llvm::Statistic numExpandedAcquireEntriesCreated{
    "db_partitioning", "NumExpandedAcquireEntriesCreated",
    "Number of per-entry DbAcquireOps created while expanding multi-entry "
    "acquires"};
llvm::Statistic numStencilAcquireGroupsPreserved{
    "db_partitioning", "NumStencilAcquireGroupsPreserved",
    "Number of multi-entry stencil acquires preserved for ESD lowering"};
llvm::Statistic numAllocsPreservedForLeafContracts{
    "db_partitioning", "NumAllocsPreservedForLeafContracts",
    "Number of allocations left in place to preserve explicit distributed leaf "
    "layouts"};
llvm::Statistic numAllocsLeftCoarseForHostViews{
    "db_partitioning", "NumAllocsLeftCoarseForHostViews",
    "Number of allocations forced to stay coarse because host-side full-view "
    "users remain"};
llvm::Statistic numAllocsLeftCoarseByHeuristics{
    "db_partitioning", "NumAllocsLeftCoarseByHeuristics",
    "Number of allocations left coarse after heuristic arbitration"};
llvm::Statistic numAcquiresForcedFullRange{
    "db_partitioning", "NumAcquiresForcedFullRange",
    "Number of DbAcquireOps widened to full-range access to preserve "
    "correctness"};
llvm::Statistic numStencilBoundsGuardsInserted{
    "db_partitioning", "NumStencilBoundsGuardsInserted",
    "Number of DbAcquireOps rewritten with bounds-valid guards for stencil "
    "accesses"};
static llvm::Statistic numAllocsPartitionedBlock{
    "db_partitioning", "NumAllocsPartitionedBlock",
    "Number of allocations rewritten to block partitioning"};
static llvm::Statistic numAllocsPartitionedStencil{
    "db_partitioning", "NumAllocsPartitionedStencil",
    "Number of allocations rewritten to stencil partitioning"};
static llvm::Statistic numAllocsPartitionedFineGrained{
    "db_partitioning", "NumAllocsPartitionedFineGrained",
    "Number of allocations rewritten to fine-grained partitioning"};
static llvm::Statistic numPartitionRewriteFailures{
    "db_partitioning", "NumPartitionRewriteFailures",
    "Number of allocation rewrites that failed in DbRewriter"};

namespace {
struct DbPartitioningPass
    : public ::impl::DbPartitioningBase<DbPartitioningPass> {
  DbPartitioningPass(mlir::arts::AnalysisManager *AM) : AM(AM) {
    assert(AM && "AnalysisManager must be provided externally");
  }

  void runOnOperation() override;

  ///===--------------------------------------------------------------------===///
  /// Main Partitioning Methods
  ///===--------------------------------------------------------------------===///

  /// Iterates over allocations and applies partitioning rewriters.
  bool partitionDb();
  bool gatherPartitionFacts();
  bool evaluatePolicyBuildPlansAndApply();
  void validateAndPostCheck(bool changed);

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

  /// Partition an allocation based on per-acquire analysis and heuristics.
  /// Orchestrates the partition decision pipeline (validation, capability
  /// analysis, heuristic arbitration, plan resolution, and rewriting).
  FailureOr<DbAllocOp> partitionAlloc(DbAllocOp allocOp,
                                      DbAllocNode *allocNode);

  /// Expand multi-entry acquires into separate DbAcquireOps.nt.
  bool expandMultiEntryAcquires();

private:
  /// Validate allocation can be partitioned. Returns nullopt if partitioning
  /// should be skipped (with reason logged), or the allocOp if validation
  /// passes.
  std::optional<FailureOr<DbAllocOp>>
  validatePartitionPreconditions(DbAllocOp allocOp, DbAllocNode *allocNode,
                                 OpBuilder &builder);

  /// Build per-acquire capability info (canBlock, canElementWise, etc.) and
  /// populate ctx.acquires for heuristic voting. Returns true if the caller
  /// should early-return (preserve explicit contract layout).
  bool buildPerAcquireCapabilities(
      PartitioningContext &ctx,
      SmallVectorImpl<AcquirePartitionInfo> &acquireInfos,
      ArrayRef<DbAcquireNode *> allocAcquireNodes,
      ArrayRef<std::optional<DbAnalysis::AcquireContractSummary>>
          acquireContractSummaries,
      SmallVectorImpl<Value> &blockSizesForNDBlock, DbAllocOp allocOp,
      DbAllocNode *allocNode, DbAnalysis &dbAnalysis, bool canUseBlock,
      std::optional<int64_t> &writerBlockSizeHint,
      std::optional<int64_t> &anyBlockSizeHint, OpBuilder &builder);

  /// Compute stencil halo bounds from acquire nodes and multi-entry structure.
  /// Returns the computed StencilInfo, or std::nullopt if stencil mode should
  /// be downgraded (e.g. single-sided halo).
  std::optional<StencilInfo> computeStencilHaloInfo(
      DbAllocOp allocOp, DbAllocNode *allocNode,
      ArrayRef<DbAcquireNode *> allocAcquireNodes,
      ArrayRef<std::optional<DbAnalysis::AcquireContractSummary>>
          acquireContractSummaries,
      SmallVectorImpl<AcquirePartitionInfo> &acquireInfos,
      bool allocHasStencilPattern, DbAnalysis &dbAnalysis,
      PartitioningDecision &decision, PartitioningContext &ctx,
      OpBuilder &builder, Location loc);

  /// Resolve block-plan sizes and partition dimensions, and reconcile
  /// per-acquire dimension consistency.
  bool resolveBlockPlanSizes(
      SmallVectorImpl<Value> &blockSizesForPlan,
      SmallVectorImpl<unsigned> &partitionedDimsForPlan,
      SmallVectorImpl<AcquirePartitionInfo> &acquireInfos,
      SmallVectorImpl<Value> &blockSizesForNDBlock,
      PartitioningDecision &decision, PartitioningContext &ctx,
      std::optional<StencilInfo> &stencilInfo, DbAllocOp allocOp,
      DbAllocNode *allocNode, DbHeuristics &heuristics, OpBuilder &builder,
      Location loc);

  /// Assemble the rewrite plan from partition decision and apply it via
  /// DbRewriter. Returns the rewritten DbAllocOp on success.
  FailureOr<DbAllocOp> assembleAndApplyRewritePlan(
      PartitioningDecision &decision,
      SmallVectorImpl<AcquirePartitionInfo> &acquireInfos,
      SmallVectorImpl<Value> &blockSizesForPlan,
      SmallVectorImpl<unsigned> &partitionedDimsForPlan,
      SmallVectorImpl<Value> &newOuterSizes,
      SmallVectorImpl<Value> &newInnerSizes,
      std::optional<StencilInfo> &stencilInfo, DbAllocOp allocOp,
      DbHeuristics &heuristics, OpBuilder &builder);

  ModuleOp module;
  mlir::arts::AnalysisManager *AM = nullptr;
};
} // namespace

void DbPartitioningPass::runOnOperation() {
  module = getOperation();

  ARTS_INFO_HEADER(DbPartitioningPass);
  ARTS_DEBUG_REGION(module.dump(););

  assert(AM && "AnalysisManager must be provided externally");

  /// Wire CLI-tunable pass options to the heuristics instance.
  auto &heuristics = AM->getDbHeuristics();
  heuristics.setMaxOuterDBs(maxOuterDbs);
  heuristics.setMaxDepsPerEDT(maxDepsPerEdt);
  heuristics.setMinInnerBytes(minInnerBytes);

  /// Phase 1: Gather facts (IR normalization for analysis + graph rebuild).
  bool changed = gatherPartitionFacts();

  /// Phase 2-4: Evaluate policy, build rewrite plans, and apply rewrites.
  changed |= evaluatePolicyBuildPlansAndApply();

  /// Phase 5: Validate/post-check and invalidate stale analyses.
  validateAndPostCheck(changed);

  ARTS_INFO_FOOTER(DbPartitioningPass);
  ARTS_DEBUG_REGION(module.dump(););
}

bool DbPartitioningPass::gatherPartitionFacts() {
  /// Expand multi-entry acquires before graph construction so each acquire can
  /// be analyzed independently.
  bool changed = expandMultiEntryAcquires();
  invalidateAndRebuildGraph();
  return changed;
}

bool DbPartitioningPass::evaluatePolicyBuildPlansAndApply() {
  return partitionDb();
}

void DbPartitioningPass::validateAndPostCheck(bool changed) {
  analyzeStencilBounds();
  if (!changed)
    return;
  ARTS_INFO(" Module has changed, invalidating analyses");
  AM->invalidate();
}

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
    if (DbAnalysis::hasMultiEntryStencilPattern(original, minOffset,
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
        ++numStencilAcquireGroupsPreserved;
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
    auto [edtUser, blockArg] = getEdtBlockArgumentForAcquire(original);
    if (!edtUser) {
      ARTS_DEBUG("    No EDT user found, skipping expansion");
      continue;
    }

    /// Create separate acquires for each partition entry
    SmallVector<DbAcquireOp> expandedAcquires =
        createExpandedAcquires(original, builder);
    ++numMultiEntryAcquiresExpanded;
    numExpandedAcquireEntriesCreated += expandedAcquires.size();

    /// Update EDT dependencies: replace original with all expanded acquires
    edtUser.setDependencies(
        rebuildEdtDeps(edtUser, original, expandedAcquires));

    /// Add new block arguments for expanded acquires (first replaces original)
    Type argType = blockArg.getType();
    SmallVector<BlockArgument> newBlockArgs = insertExpandedBlockArgs(
        edtUser, blockArg, numEntries, argType, original.getLoc());

    /// Remap DbRef operations to use their correct block arguments.
    for (DbRefOp dbRef : collectDbRefUsers(blockArg)) {
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
    for (Operation *user : blockArg.getUsers()) {
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
    DbGraph &graph = AM->getDbAnalysis().getOrCreateGraph(func);
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

/// Validate allocation can be partitioned. Returns nullopt if validation
/// passes, or a FailureOr<DbAllocOp> early-exit result if partitioning should
/// be skipped.
std::optional<FailureOr<DbAllocOp>>
DbPartitioningPass::validatePartitionPreconditions(DbAllocOp allocOp,
                                                   DbAllocNode *allocNode,
                                                   OpBuilder &builder) {
  auto &heuristics = AM->getDbHeuristics();

  if (!allocOp || !allocOp.getOperation())
    return FailureOr<DbAllocOp>(failure());

  if (allocOp.getElementSizes().empty())
    return FailureOr<DbAllocOp>(failure());

  /// Check existing partition attribute.
  if (auto existingMode = getPartitionMode(allocOp)) {
    /// Fine-grained allocations are already in their terminal shape.
    if (*existingMode == PartitionMode::fine_grained) {
      ARTS_DEBUG("  Already partitioned as "
                 << mlir::arts::stringifyPartitionMode(*existingMode)
                 << " - SKIP");
      heuristics.recordDecision("Partition-AlreadyPartitioned", false,
                                "allocation already fine-grained, skipping",
                                allocOp.getOperation(), {});
      return FailureOr<DbAllocOp>(allocOp);
    }

    /// For block/stencil, continue with loop-aware re-analysis.
    ARTS_DEBUG("  Partition mode is "
               << mlir::arts::stringifyPartitionMode(*existingMode)
               << " - re-analyzing with loop structure");
    heuristics.recordDecision(
        "Partition-ReanalyzeExisting", true,
        "re-analyzing existing partition mode with loop-aware rewrite",
        allocOp.getOperation(), {});
  }

  /// Already fine-grained by structure - skip.
  if (allocNode && DbAnalysis::isFineGrained(allocNode->getDbAllocOp())) {
    ARTS_DEBUG("  Already fine-grained by structure - SKIP");
    heuristics.recordDecision(
        "Partition-AlreadyFineGrained", false,
        "allocation already fine-grained by structure, skipping",
        allocOp.getOperation(), {});
    return FailureOr<DbAllocOp>(allocOp);
  }

  /// Check for non-partitionable host-side uses.
  if (DbUtils::hasNonPartitionableHostViewUses(allocOp.getPtr())) {
    ARTS_DEBUG("  Host-side opaque/full-view users require coarse layout");
    resetCoarseAcquireRanges(allocOp, allocNode, builder);
    ++numAllocsLeftCoarseForHostViews;
    heuristics.recordDecision(
        "Partition-UnsafeHostView", false,
        "host-side opaque/full-view uses require coarse allocation",
        allocOp.getOperation(), {});
    return FailureOr<DbAllocOp>(allocOp);
  }

  /// Validation passed - proceed with partitioning.
  return std::nullopt;
}

/// Collect all DbAcquireOps from the entire module that reference the given
/// GUID, not just those attached to the DbAllocNode. This is crucial for
/// detecting transpose patterns where different EDT families access the same
/// DB with different owner_dims (e.g., atax matrix A accessed by row and
/// column).
static void collectAllAcquiresByGuid(DbAnalysis &dbAnalysis, Value allocGuid,
                                     SmallVectorImpl<DbAcquireNode *> &result) {
  if (!allocGuid)
    return;

  /// Get the module to walk all functions
  auto moduleOp = allocGuid.getDefiningOp()->getParentOfType<ModuleOp>();
  if (!moduleOp)
    return;

  /// Walk all DbAcquireOps in the module
  moduleOp.walk([&](DbAcquireOp acquireOp) {
    /// Check if this acquire references our GUID
    Value sourceGuid = acquireOp.getSourceGuid();
    if (!sourceGuid)
      return;

    /// Trace the source GUID back through the SSA chain
    /// (could be from DbAllocOp or from another DbAcquireOp)
    Value currentGuid = sourceGuid;
    while (currentGuid) {
      if (currentGuid == allocGuid) {
        /// This acquire uses our DB GUID - add it to the result
        if (auto *acqNode = dbAnalysis.getDbAcquireNode(acquireOp)) {
          result.push_back(acqNode);
        }
        return;
      }

      /// Trace through DbAcquireOp chains
      if (auto definingAcquire =
              dyn_cast_or_null<DbAcquireOp>(currentGuid.getDefiningOp())) {
        currentGuid = definingAcquire.getSourceGuid();
      } else {
        break;
      }
    }
  });
}

/// Analyze allocation acquires and apply appropriate partitioning rewriter.
///
/// Orchestration pipeline:
///   1. Validate preconditions (early exits for already-partitioned, host-view)
///   2. Analyze per-acquire partition modes and contract summaries
///   3. Build partitioning context and per-acquire capabilities
///   4. Reconcile per-acquire modes into allocation-level decision
///   5. Invoke heuristics for partition mode arbitration
///   6. Resolve block-plan sizes and stencil halo info (if applicable)
///   7. Assemble rewrite plan and apply via DbRewriter
FailureOr<DbAllocOp>
DbPartitioningPass::partitionAlloc(DbAllocOp allocOp, DbAllocNode *allocNode) {
  OpBuilder builder(allocOp);
  Location loc = allocOp.getLoc();
  builder.setInsertionPoint(allocOp);

  /// Phase 1: Validate preconditions.
  if (auto earlyExit =
          validatePartitionPreconditions(allocOp, allocNode, builder))
    return *earlyExit;

  auto &heuristics = AM->getDbHeuristics();

  /// Collect acquires from ALL EDT families that reference this DB GUID,
  /// not just from the current allocNode. This is critical for H1.C3a
  /// transpose detection (e.g., atax matrix A accessed with different
  /// owner_dims in different EDT families).
  SmallVector<DbAcquireNode *, 16> allocAcquireNodes;
  if (allocNode) {
    DbAnalysis &dbAnalysis = AM->getDbAnalysis();
    collectAllAcquiresByGuid(dbAnalysis, allocOp.getGuid(), allocAcquireNodes);

    /// Fallback to local collection if global search finds nothing
    if (allocAcquireNodes.empty()) {
      allocAcquireNodes = allocNode->collectAllAcquireNodes();
    }
  }

  /// Phase 2: Analyze each acquire's PartitionMode.
  SmallVector<AcquirePartitionInfo, 4> acquireInfos;
  SmallVector<std::optional<DbAnalysis::AcquireContractSummary>, 8>
      acquireContractSummaries;
  DbAnalysis &dbAnalysis = AM->getDbAnalysis();
  if (allocNode) {
    for (DbAcquireNode *acqNode : allocAcquireNodes) {
      auto contractSummary =
          dbAnalysis.getAcquireContractSummary(acqNode->getDbAcquireOp());
      acquireContractSummaries.push_back(contractSummary);
      auto info = computeAcquirePartitionInfo(
          dbAnalysis, acqNode->getDbAcquireOp(),
          contractSummary ? &*contractSummary : nullptr, builder);
      if (!info.isValid) {
        ARTS_DEBUG("  Acquire analysis failed for "
                   << acqNode->getDbAcquireOp());
      }
      acquireInfos.push_back(info);
    }
  }

  if (acquireInfos.empty()) {
    ARTS_DEBUG("  No acquires to analyze - keeping original");
    heuristics.recordDecision(
        "Partition-NoAcquires", false,
        "no acquires to analyze, keeping original allocation",
        allocOp.getOperation(), {});
    return allocOp;
  }

  /// Phase 3: Build PartitioningContext and per-acquire capabilities.
  PartitioningContext ctx;
  SmallVector<Value> blockSizesForNDBlock;
  ctx.machine = &AM->getAbstractMachine();
  bool canUseBlock = true; /// Assume block is possible until proven otherwise

  /// Avoid multinode-style block partitioning when this allocation has no
  /// internode EDT users. Keep block disabled and rely on existing
  /// fine-grained/coarse semantics inferred from DB analysis.
  bool hasInternodeUsers = hasInternodeAcquireUser(allocAcquireNodes);
  bool isMultinodeMachine = AM->getAbstractMachine().hasValidNodeCount() &&
                            AM->getAbstractMachine().getNodeCount() > 1;
  if (isMultinodeMachine && !hasInternodeUsers) {
    canUseBlock = false;
    heuristics.recordDecision("Partition-DisableBlockNoInternodeUsers", false,
                              "no internode EDT users for allocation",
                              allocOp.getOperation(), {});
    ARTS_DEBUG("  No internode EDT users - disabling block partitioning");
  }

  if (allocNode) {
    ctx.accessPatterns = summarizeAcquirePatterns(allocAcquireNodes, allocNode,
                                                  &AM->getDbAnalysis());
    ctx.isUniformAccess = ctx.accessPatterns.hasUniform;
    ARTS_DEBUG("  Access patterns: hasUniform="
               << ctx.accessPatterns.hasUniform
               << ", hasStencil=" << ctx.accessPatterns.hasStencil
               << ", hasIndexed=" << ctx.accessPatterns.hasIndexed
               << ", isMixed=" << ctx.accessPatterns.isMixed());

    if (!allocOp.getElementSizes().empty()) {
      int64_t staticFirstDim = 0;
      if (ValueAnalysis::getConstantIndex(allocOp.getElementSizes().front(),
                                          staticFirstDim)) {
        ctx.totalElements = staticFirstDim;
      }
    }

    for (size_t factIdx = 0; factIdx < acquireContractSummaries.size();
         ++factIdx) {
      const auto &contractSummary = acquireContractSummaries[factIdx];
      if (!contractSummary)
        continue;

      const ArtsMode mode = factIdx < acquireInfos.size()
                                ? acquireInfos[factIdx].acquire.getMode()
                                : ArtsMode::uninitialized;
      if (contractSummary->hasIndirectAccess()) {
        ctx.hasIndirectAccess = true;
        if (mode == ArtsMode::in || mode == ArtsMode::inout)
          ctx.hasIndirectRead = true;
        if (DbUtils::isWriterMode(mode))
          ctx.hasIndirectWrite = true;
      }
      if (contractSummary->hasDirectAccess())
        ctx.hasDirectAccess = true;
    }

    /// Note: Indirect access is handled via full-range acquires, not by
    /// disabling chunked. Full-range acquires allow indirect reads to access
    /// all chunks.
    if (ctx.hasIndirectAccess) {
      ARTS_DEBUG("  Indirect access detected - indirect acquires will use "
                 "full-range");
    }

    /// Build per-acquire capabilities for heuristic voting
    std::optional<int64_t> writerBlockSizeHint;
    std::optional<int64_t> anyBlockSizeHint;
    if (buildPerAcquireCapabilities(
            ctx, acquireInfos, allocAcquireNodes, acquireContractSummaries,
            blockSizesForNDBlock, allocOp, allocNode, dbAnalysis, canUseBlock,
            writerBlockSizeHint, anyBlockSizeHint, builder))
      return allocOp;

    /// Build per-acquire policy inputs for H1.7 full-range arbitration.
    SmallVector<AcquirePolicyInput> acquirePolicyInputs;
    acquirePolicyInputs.reserve(acquireInfos.size());
    for (size_t i = 0; i < acquireInfos.size(); ++i) {
      const auto &acqInfo = acquireInfos[i];
      const DbAnalysis::AcquireContractSummary *contractSummary =
          i < acquireContractSummaries.size() && acquireContractSummaries[i]
              ? &*acquireContractSummaries[i]
              : nullptr;

      AcquirePolicyInput input;
      input.acquire = acqInfo.acquire;
      input.depPattern = i < ctx.acquires.size()
                             ? ctx.acquires[i].depPattern
                             : dbAnalysis.resolveCanonicalAcquireDepPattern(
                                   acqInfo.acquire, contractSummary);
      input.hasPartitionDims = !acqInfo.partitionDims.empty();
      input.hasOwnerDims = contractSummary &&
                           !contractSummary->contract.spatial.ownerDims.empty();
      input.hasBlockHints = contractSummary && contractSummary->hasBlockHints();
      input.hasExplicitStencilContract =
          contractSummary &&
          contractSummary->contract.hasExplicitStencilContract();
      input.hasIndirectAccess = contractSummary
                                    ? contractSummary->hasIndirectAccess()
                                    : acqInfo.hasIndirectAccess;
      input.needsFullRange = acqInfo.needsFullRange;
      input.preservesDistributedContract =
          contractSummary &&
          contractSummary->preservesDistributedContractEntry();
      acquirePolicyInputs.push_back(input);
    }
    auto acquireDecisions =
        heuristics.chooseAcquirePolicies(acquirePolicyInputs);

    /// Apply decisions to acquireInfos
    for (size_t i = 0; i < acquireDecisions.size() && i < acquireInfos.size();
         ++i) {
      if (acquireDecisions[i].needsFullRange) {
        markAcquireNeedsFullRange(acquireInfos[i]);
      }
    }

    if (writerBlockSizeHint)
      ctx.blockSize = writerBlockSizeHint;
    else if (anyBlockSizeHint)
      ctx.blockSize = anyBlockSizeHint;
  }

  /// Phase 4: Reconcile per-acquire modes into allocation-level decision.
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
  if (auto memrefType = dyn_cast<MemRefType>(allocOp.getElementType())) {
    ctx.memrefRank = memrefType.getRank();
    ctx.elementTypeIsMemRef = true;
  } else {
    ctx.memrefRank = allocOp.getElementSizes().size();
    ctx.elementTypeIsMemRef = false;
  }

  unsigned authoritativeOwnerBlockRank = deriveAuthoritativeOwnerBlockRank(
      allocOp, acquireInfos, acquireContractSummaries, ctx.acquires);
  if (authoritativeOwnerBlockRank > ctx.preferredOuterRank) {
    ctx.preferredOuterRank = authoritativeOwnerBlockRank;
    ctx.preferBlockND = authoritativeOwnerBlockRank > 1;
    if (ctx.preferBlockND) {
      ARTS_DEBUG("  Preserving multi-dim block layout from authoritative "
                 "owner dims (rank="
                 << ctx.preferredOuterRank << ")");
    }
  }

  /// Feed raw allocation facts to H1 so the heuristic layer, not the
  /// controller, decides whether special coarse cases (for example tiny
  /// read-only stencil coefficient tables) should remain coarse.
  ctx.staticElementCount = computeStaticElementCount(allocOp);
  ctx.existingAllocMode =
      getPartitionMode(allocOp).value_or(PartitionMode::coarse);
  ctx.allocAccessPattern = getDbAccessPattern(allocOp.getOperation())
                               .value_or(DbAccessPattern::unknown);
  ctx.allocDbMode = allocOp.getDbMode();

  /// Phase 4.5: Inject structured kernel plan hints when available.
  /// Walk EdtOp parents of acquires to find plan attrs. If the plan provides
  /// a physical block shape, feed it as a hint to the partitioning context.
  for (size_t i = 0; i < allocAcquireNodes.size(); ++i) {
    auto *acqNode = allocAcquireNodes[i];
    if (!acqNode)
      continue;
    auto acqOp = acqNode->getDbAcquireOp();
    if (!acqOp)
      continue;
    Operation *forOpParent = acqOp->getParentOfType<ForOp>();
    if (!forOpParent)
      continue;
    if (auto familyAttr = forOpParent->getAttrOfType<StringAttr>(
            AttrNames::Operation::Plan::KernelFamily)) {
      ARTS_DEBUG("  Plan hint for acquire: family=" << familyAttr.getValue());
      /// Plan-driven block shape overrides.
      if (auto planBlockShape = readI64ArrayAttr(
              forOpParent, AttrNames::Operation::Plan::PhysicalBlockShape)) {
        if (!planBlockShape->empty())
          ctx.planBlockShapeHint = *planBlockShape;
      }
      /// Plan-driven stencil vs uniform classification.
      if (familyAttr.getValue() == "stencil")
        ctx.planHintIsStencil = true;
      break;
    }
  }

  /// Phase 5: Invoke heuristics for partition mode arbitration.
  /// Primary path: use existing heuristics (maintains 121/121 test
  /// compatibility). Strategy objects are also evaluated for validation and
  /// future migration.
  PartitioningDecision decision = heuristics.choosePartitioning(ctx);

  /// Parallel evaluation: invoke strategy objects alongside heuristics.
  /// This wires the strategies into the dispatch without changing behavior yet.
  auto strategies = PartitionStrategyFactory::createStandardStrategies();
  const AbstractMachine *machine = &AM->getAbstractMachine();

  for (const auto &strategy : strategies) {
    if (auto strategyDecision = strategy->evaluate(ctx, machine)) {
      /// Strategy matched - log for validation but use heuristics decision.
      ARTS_DEBUG("  Strategy " << strategy->getName() << " would select: "
                               << getPartitionModeName(strategyDecision->mode)
                               << " (heuristics chose: "
                               << getPartitionModeName(decision.mode) << ")");
      break;
    }
  }
  dbPartitionTraceImpl([&](llvm::raw_ostream &os) {
    os << "alloc=" << getArtsId(allocOp.getOperation())
       << " patterns(uniform=" << ctx.accessPatterns.hasUniform
       << ", stencil=" << ctx.accessPatterns.hasStencil
       << ", indexed=" << ctx.accessPatterns.hasIndexed
       << ") canBlock=" << ctx.canBlock << " canEW=" << ctx.canElementWise
       << " hasDirect=" << ctx.hasDirectAccess
       << " hasIndirect=" << ctx.hasIndirectAccess
       << " accessMode=" << static_cast<int>(ctx.accessMode)
       << " decision=" << getPartitionModeName(decision.mode)
       << " rationale=" << decision.rationale;
  });

  if (forceCoarseAllocId >= 0 &&
      getArtsId(allocOp.getOperation()) == forceCoarseAllocId) {
    decision = PartitioningDecision::coarse(
        ctx, "Debug override: forced coarse allocation by arts.id");
    ARTS_DEBUG("  Forcing coarse allocation via " << "--force-coarse-alloc-id="
                                                  << forceCoarseAllocId);
    heuristics.recordDecision(
        "Partition-DebugForceCoarse", true,
        "forced coarse allocation via --force-coarse-alloc-id",
        allocOp.getOperation(), {{"forcedArtsId", forceCoarseAllocId}});
  }

  if (decision.isCoarse() && ctx.existingAllocMode == PartitionMode::coarse) {
    ARTS_DEBUG("  Heuristics chose coarse - allocation already coarse");
    resetCoarseAcquireRanges(allocOp, allocNode, builder);
    ++numAllocsLeftCoarseByHeuristics;
    heuristics.recordDecision(
        "Partition-HeuristicsCoarse", false,
        "heuristics chose coarse allocation, existing allocation already "
        "matches",
        allocOp.getOperation(),
        {{"canBlock", ctx.canBlock ? 1 : 0},
         {"canElementWise", ctx.canElementWise ? 1 : 0}});
    return allocOp;
  }
  if (decision.isCoarse()) {
    ARTS_DEBUG("  Heuristics chose coarse - rewriting allocation to coarse");
  }

  bool hasContractBackedStencil =
      llvm::any_of(acquireContractSummaries, [](const auto &summary) {
        return summary && summary->hasExplicitStencilLayout();
      });
  if (decision.mode == PartitionMode::stencil && !hasContractBackedStencil) {
    unsigned outerRank = std::max(1u, decision.outerRank);
    decision = PartitioningDecision::blockND(
        ctx, outerRank, "Fallback: stencil facts without explicit contract");
    ARTS_DEBUG("  Downgrading stencil decision to block mode because no "
               "explicit stencil contract was seeded pre-DB");
    heuristics.recordDecision(
        "Partition-StencilDowngradedToBlock", true,
        "stencil mode requires an explicit pre-DB contract; using block "
        "partitioning instead",
        allocOp.getOperation(), {{"outerRank", (int64_t)outerRank}});
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
    bool validElementWise =
        validateElementWiseIndices(acquireInfos, allocAcquireNodes);
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
        auto &loopAnalysis = AM->getLoopAnalysis();
        for (Value idx : partitionIndices) {
          if (LoopNode *drivingLoop = loopAnalysis.findEnclosingLoopDrivenBy(
                  info.acquire.getOperation(), idx)) {
            Value blockSize = drivingLoop->getStep();
            if (blockSize) {
              ARTS_DEBUG("    Found block size from loop step: " << blockSize);
              blockSizesForNDBlock.push_back(blockSize);
            }
          }
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
        /// No loop-step block sizes found. Keep the block path alive and let
        /// the shared resolver synthesize a safe 1-D plan from canonical
        /// acquire facts or the default machine-derived block size.
        ARTS_DEBUG("  Cannot extract N-D block sizes - deferring to shared "
                   "block-plan resolver");
        decision = PartitioningDecision::block(
            ctx, "Fallback: block-wise pattern via shared block-plan");
        heuristics.recordDecision(
            "Partition-ElementWiseFallbackToBlock", true,
            "block-wise pattern without explicit loop-step sizes, using "
            "shared block-plan resolver",
            allocOp.getOperation(), {});
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
    if (!usesBlockLayout(info.mode))
      continue;
    for (Value sz : info.partitionSizes) {
      if (!sz)
        continue;
      Value stripped = ValueAnalysis::stripNumericCasts(sz);
      if (!ValueAnalysis::isOneConstant(stripped)) {
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
      auto &loopAnalysis = AM->getLoopAnalysis();
      for (Value idx : indicesToCheck) {
        if (LoopNode *drivingLoop = loopAnalysis.findEnclosingLoopDrivenBy(
                info.acquire.getOperation(), idx)) {
          Value blockSize = drivingLoop->getStep();
          if (blockSize) {
            ARTS_DEBUG("    Found block size from loop step: " << blockSize);
            blockSizesForNDBlock.push_back(blockSize);
          }
        }
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

  bool hasMissingAcquireInfo = false;
  if (!skipAcquireInfoCheck) {
    hasMissingAcquireInfo = llvm::any_of(acquireInfos, [&](auto &info) {
      if (info.needsFullRange)
        return false;
      return !info.isValid || info.partitionOffsets.empty() ||
             info.partitionSizes.empty();
    });
  }

  ARTS_DEBUG("  Decision: mode=" << getPartitionModeName(decision.mode)
                                 << ", outerRank=" << decision.outerRank
                                 << ", innerRank=" << decision.innerRank);

  auto allocPattern = getDbAccessPattern(allocOp.getOperation());
  bool allocHasStencilPattern =
      (allocPattern && *allocPattern == DbAccessPattern::stencil) ||
      ctx.accessPatterns.hasStencil;

  if (decision.mode == PartitionMode::stencil) {
    if (allocHasStencilPattern) {
      for (auto &info : acquireInfos) {
        if (!info.acquire)
          continue;
        bool hasPartitionRange =
            !info.partitionOffsets.empty() && !info.partitionSizes.empty();
        bool blockLikeMode =
            usesBlockLayout(info.mode) ||
            (info.mode == PartitionMode::coarse && info.isValid);
        if (blockLikeMode && hasPartitionRange)
          info.needsFullRange = false;
      }
    }
  }

  if (hasMissingAcquireInfo) {
    if (decision.isBlock()) {
      int64_t widenedAcquires = 0;
      for (auto &info : acquireInfos) {
        if (info.needsFullRange)
          continue;
        if (info.isValid && !info.partitionOffsets.empty() &&
            !info.partitionSizes.empty())
          continue;
        if (markAcquireNeedsFullRange(info))
          ++widenedAcquires;
      }

      ARTS_DEBUG("  Some acquires missing partition info - forcing full-range "
                 "on block allocation");
      heuristics.recordDecision(
          "Partition-MissingAcquireInfo", true,
          "some acquires missing partition info, forcing full-range on block "
          "allocation",
          allocOp.getOperation(), {{"widenedAcquires", widenedAcquires}});
    } else {
      ARTS_DEBUG("  Some acquires missing partition info - keeping original");
      heuristics.recordDecision(
          "Partition-MissingAcquireInfo", false,
          "some acquires missing partition info, cannot rewrite allocation",
          allocOp.getOperation(), {});
      return allocOp;
    }
  }

  /// Phase 6a: Compute stencilInfo (needed for inner size calculation).
  /// Must be done before computeAllocationShape() for stencil mode.
  std::optional<StencilInfo> stencilInfo;
  if (decision.mode == PartitionMode::stencil && allocNode) {
    stencilInfo = computeStencilHaloInfo(allocOp, allocNode, allocAcquireNodes,
                                         acquireContractSummaries, acquireInfos,
                                         allocHasStencilPattern, dbAnalysis,
                                         decision, ctx, builder, loc);
  }

  /// Phase 6b: Resolve block-plan sizes and reconcile partition dimensions.
  SmallVector<Value> blockSizesForPlan;
  SmallVector<unsigned> partitionedDimsForPlan;
  if (decision.isBlock()) {
    if (!resolveBlockPlanSizes(blockSizesForPlan, partitionedDimsForPlan,
                               acquireInfos, blockSizesForNDBlock, decision,
                               ctx, stencilInfo, allocOp, allocNode, heuristics,
                               builder, loc))
      return allocOp;
  }

  /// Mixed block/stencil lowering keeps the allocation block-partitioned, but
  /// still needs halo metadata so read-only stencil acquires can take the ESD
  /// path. Only enable that when the resolved block plan stays on leading
  /// dimensions, which is the shape DbStencilRewriter supports today.
  if (!stencilInfo && decision.mode == PartitionMode::block && allocNode &&
      allocHasStencilPattern &&
      isLeadingPrefixPartitionPlan(partitionedDimsForPlan,
                                   blockSizesForPlan.size(),
                                   allocOp.getElementSizes().size())) {
    stencilInfo = computeStencilHaloInfo(allocOp, allocNode, allocAcquireNodes,
                                         acquireContractSummaries, acquireInfos,
                                         allocHasStencilPattern, dbAnalysis,
                                         decision, ctx, builder, loc);
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

  /// Phase 7: Assemble rewrite plan and apply via DbRewriter.
  return assembleAndApplyRewritePlan(
      decision, acquireInfos, blockSizesForPlan, partitionedDimsForPlan,
      newOuterSizes, newInnerSizes, stencilInfo, allocOp, heuristics, builder);
}

/// Build per-acquire capability info and populate ctx.acquires for heuristic
/// voting. Returns true if the caller should early-return the original allocOp
/// (e.g. when an explicit contract layout must be preserved).
bool DbPartitioningPass::buildPerAcquireCapabilities(
    PartitioningContext &ctx,
    SmallVectorImpl<AcquirePartitionInfo> &acquireInfos,
    ArrayRef<DbAcquireNode *> allocAcquireNodes,
    ArrayRef<std::optional<DbAnalysis::AcquireContractSummary>>
        acquireContractSummaries,
    SmallVectorImpl<Value> &blockSizesForNDBlock, DbAllocOp allocOp,
    DbAllocNode *allocNode, DbAnalysis &dbAnalysis, bool canUseBlock,
    std::optional<int64_t> &writerBlockSizeHint,
    std::optional<int64_t> &anyBlockSizeHint, OpBuilder &builder) {
  Location loc = allocOp.getLoc();
  auto &heuristics = AM->getDbHeuristics();

  size_t idx = 0;
  bool preserveExplicitContractLayout = false;
  for (DbAcquireNode *acqNode : allocAcquireNodes) {
    if (!acqNode || idx >= acquireInfos.size())
      continue;

    auto &acqInfo = acquireInfos[idx];
    const DbAnalysis::AcquireContractSummary *contractSummary =
        idx < acquireContractSummaries.size() && acquireContractSummaries[idx]
            ? &*acquireContractSummaries[idx]
            : nullptr;

    /// Check access patterns for block capability decisions.
    bool hasIndirect = contractSummary ? contractSummary->hasIndirectAccess()
                                       : acqInfo.hasIndirectAccess;

    /// Read partition capability from acquire's attribute.
    /// ForLowering sets this to 'chunked' when adding offset/size hints.
    /// CreateDbs sets this based on DbControlOp analysis.
    auto acquire = acqNode->getDbAcquireOp();
    auto acquireMode = getPartitionMode(acquire.getOperation());
    bool hasBlockHints = contractSummary
                             ? contractSummary->hasBlockHints()
                             : (!acquire.getPartitionOffsets().empty() ||
                                !acquire.getPartitionSizes().empty());
    /// Preserve graph-recovered block structure even when a contract summary
    /// exists but does not carry an explicit inferredBlock bit. Coarse shell
    /// acquires lowered by distribution often recover their true block window
    /// only through computeAcquirePartitionInfo()/computeBlockInfo().
    bool inferredBlock =
        (contractSummary && contractSummary->inferredBlock()) ||
        (!acqInfo.partitionOffsets.empty() && !acqInfo.partitionSizes.empty());
    if (auto blockHint = getPartitioningHint(acquire.getOperation())) {
      if (blockHint->mode == PartitionMode::block && blockHint->blockSize &&
          *blockHint->blockSize > 0) {
        if (!anyBlockSizeHint)
          anyBlockSizeHint = blockHint->blockSize;
        else
          anyBlockSizeHint = std::min(*anyBlockSizeHint, *blockHint->blockSize);

        if (acquire.getMode() != ArtsMode::in) {
          if (!writerBlockSizeHint)
            writerBlockSizeHint = blockHint->blockSize;
          else
            writerBlockSizeHint =
                std::min(*writerBlockSizeHint, *blockHint->blockSize);
        }
      }
    }
    bool hasDistributionContract =
        contractSummary ? contractSummary->hasDistributionContract() : false;
    bool thisAcquireCanBlock = (acquireMode && usesBlockLayout(*acquireMode)) ||
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
    if (!thisAcquireCanBlock && contractSummary &&
        contractSummary->supportsContractDrivenBlockCapability())
      thisAcquireCanBlock = true;
    if (!thisAcquireCanBlock && contractSummary &&
        contractSummary->getDerivedAccessPattern() == AccessPattern::Stencil)
      thisAcquireCanBlock = true;
    if (!thisAcquireCanBlock && !contractSummary &&
        acqInfo.accessPattern == AccessPattern::Stencil)
      thisAcquireCanBlock = true;
    bool thisAcquireCanElementWise =
        (acquireMode && *acquireMode == PartitionMode::fine_grained) ||
        (contractSummary && contractSummary->hasFineGrainedEntries());

    /// If the current acquire pointer cannot represent the contract's owner
    /// dimensions, block partitioning is not sound on this view. This happens
    /// for promoted array-of-arrays containers where the contract refers to a
    /// logical inner dimension (e.g. owner_dims=[1]) but the active acquire
    /// ptr is only rank-1 at this stage.
    if (thisAcquireCanBlock && contractSummary) {
      if (auto sourcePtrTy = dyn_cast<MemRefType>(acquire.getSourcePtr().getType())) {
        unsigned sourceRank = sourcePtrTy.getRank();
        bool ownerDimsOutOfRange = llvm::any_of(
            contractSummary->contract.spatial.ownerDims, [&](int64_t dim) {
              return dim >= 0 && static_cast<unsigned>(dim) >= sourceRank;
            });
        if (ownerDimsOutOfRange) {
          ARTS_DEBUG("  Contract owner dims exceed source ptr rank; "
                     "disabling block capability for acquire");
          thisAcquireCanBlock = false;
        }
      }
    }

    /// If the partition offset does not map to the access pattern, block
    /// partitioning would select the wrong dimension (non-leading) and is
    /// unsafe. Disable block capability in that case.
    SmallVector<Value> offsetVals = getPartitionOffsetsND(acqNode, &acqInfo);
    unsigned offsetIdx = 0;
    Value partitionOffset =
        DbUtils::pickRepresentativePartitionOffset(offsetVals, &offsetIdx);
    if (partitionOffset) {
      bool hasUnmappedEntry =
          contractSummary && contractSummary->hasUnmappedPartitionEntry();
      if (hasUnmappedEntry) {
        bool preserveFromStencilContract =
            contractSummary &&
            contractSummary->prefersSemanticOwnerLayoutPreservation();
        bool writesThroughAcquire = acquire.getMode() == ArtsMode::out ||
                                    acquire.getMode() == ArtsMode::inout ||
                                    acqNode->hasStores();
        bool preserveFromExplicitContract =
            contractSummary &&
            contractSummary->preservesDistributedContractEntry();
        if (preserveFromExplicitContract || preserveFromStencilContract) {
          ARTS_DEBUG("  Partition offset not mappable by DbAcquireNode, but "
                     "preserving block capability from "
                     << (preserveFromExplicitContract ? "EDT distribution"
                                                      : "N-D stencil halo")
                     << " contract"
                     << (writesThroughAcquire ? " (write acquire)" : ""));
          bool acquireSelectsLeafDb = false;
          if (auto ptrTy = dyn_cast<MemRefType>(acquire.getPtr().getType()))
            acquireSelectsLeafDb = !isa<MemRefType>(ptrTy.getElementType());
          if (acquireSelectsLeafDb)
            preserveExplicitContractLayout = true;
        } else if (writesThroughAcquire) {
          bool hasDirectWriteAccess = contractSummary
                                          ? contractSummary->hasDirectAccess()
                                          : !hasIndirect;
          bool canPreserveWriteFullRange =
              !hasIndirect && hasDirectWriteAccess &&
              (!ctx.totalElements || *ctx.totalElements > 1);
          if (canPreserveWriteFullRange) {
            ARTS_DEBUG("  Partition offset incompatible with write access; "
                       "preserving block capability with full-range write "
                       "acquire");
            markAcquireNeedsFullRange(acqInfo);
          } else {
            ARTS_DEBUG("  Partition offset incompatible with write access; "
                       "disabling block capability");
            thisAcquireCanBlock = false;
          }
        } else {
          ARTS_DEBUG("  Partition offset incompatible with read-only access "
                     "pattern; preserving block capability with full-range");
          markAcquireNeedsFullRange(acqInfo);
        }
      }
    }

    /// Build AcquireInfo for heuristic voting
    AcquireInfo info;
    info.accessMode = acquire.getMode();
    info.canElementWise =
        thisAcquireCanElementWise || !acquire.getPartitionIndices().empty();
    info.canBlock = canUseBlock && thisAcquireCanBlock;
    info.needsFullRange = acqInfo.needsFullRange;
    info.hasDistributionContract = hasDistributionContract;
    info.partitionDimsFromPeers =
        contractSummary ? contractSummary->partitionDimsFromPeers() : false;
    info.explicitCoarseRequest =
        acquireMode && *acquireMode == PartitionMode::coarse;
    if (contractSummary &&
        !contractSummary->contract.spatial.ownerDims.empty()) {
      info.ownerDimsCount = static_cast<uint8_t>(std::min<size_t>(
          4, contractSummary->contract.spatial.ownerDims.size()));
      for (unsigned dim = 0; dim < info.ownerDimsCount; ++dim)
        info.ownerDims[dim] = static_cast<int16_t>(
            contractSummary->contract.spatial.ownerDims[dim]);
    } else if (auto stencilOwnerDims = getStencilOwnerDims(acquire)) {
      // Fallback: read from stencil_owner_dims attribute if contract is empty
      info.ownerDimsCount =
          static_cast<uint8_t>(std::min<size_t>(4, stencilOwnerDims->size()));
      for (unsigned dim = 0; dim < info.ownerDimsCount; ++dim)
        info.ownerDims[dim] = static_cast<int16_t>((*stencilOwnerDims)[dim]);
    }

    /// Populate unified partition infos from DbAcquireOp.
    /// Heuristics will compute uniformity and decide outerRank.
    info.partitionInfos = acquire.getPartitionInfos();

    /// Populate partition mode from AcquirePartitionInfo
    if (idx < acquireInfos.size()) {
      const auto &acqPartInfo = acquireInfos[idx];
      info.partitionMode = acqPartInfo.mode;
    }

    /// Populate access pattern and dep pattern using canonical ordering.
    info.accessPattern = dbAnalysis.resolveCanonicalAcquireAccessPattern(
        acquire, contractSummary);
    info.depPattern =
        dbAnalysis.resolveCanonicalAcquireDepPattern(acquire, contractSummary);

    if (info.accessPattern == AccessPattern::Unknown &&
        info.accessMode == ArtsMode::in &&
        PatternSemantics::isStencilFamily(info.depPattern)) {
      info.accessPattern = AccessPattern::Stencil;
    }

    if (!ctx.preferBlockND && acqInfo.partitionSizes.size() >= 2) {
      bool preferContractND =
          contractSummary &&
          contractSummary->prefersSemanticOwnerLayoutPreservation();
      bool preferTiling2D = acquire.getMode() == ArtsMode::inout &&
                            DbAnalysis::isTiling2DTaskAcquire(acquire);
      bool preferWavefrontStencilMode =
          contractSummary && contractSummary->usesWavefrontStencilContract();
      if (preferWavefrontStencilMode) {
        ARTS_DEBUG("    Acquire[" << idx
                                  << "]: explicit wavefront stencil "
                                     "contract bypasses N-D block override");
      }
      if ((preferContractND || preferTiling2D) && !preferWavefrontStencilMode) {
        SmallVector<Value> candidateBlockSizes;
        unsigned targetRank =
            static_cast<unsigned>(acqInfo.partitionSizes.size());
        if (preferContractND)
          targetRank = std::min<unsigned>(
              targetRank,
              static_cast<unsigned>(
                  contractSummary->contract.spatial.ownerDims.size()));
        if (preferContractND) {
          if (contractSummary && contractSummary->prefersNDBlock(targetRank)) {
            auto staticBlockShape =
                contractSummary->contract.getStaticBlockShape();
            candidateBlockSizes.reserve(targetRank);
            for (unsigned dim = 0; dim < targetRank; ++dim) {
              int64_t dimSize = 0;
              Value blockSize = nullptr;
              if (dim < contractSummary->contract.spatial.blockShape.size())
                blockSize = contractSummary->contract.spatial.blockShape[dim];
              if (blockSize) {
                if (!ValueAnalysis::getConstantIndex(blockSize, dimSize) ||
                    dimSize <= 0) {
                  candidateBlockSizes.clear();
                  break;
                }
              } else if (staticBlockShape &&
                         dim <
                             static_cast<unsigned>(staticBlockShape->size()) &&
                         (*staticBlockShape)[dim] > 0) {
                dimSize = (*staticBlockShape)[dim];
                blockSize = arts::createConstantIndex(builder, loc, dimSize);
              } else {
                candidateBlockSizes.clear();
                break;
              }
              candidateBlockSizes.push_back(blockSize);
            }
          }
        }
        if (candidateBlockSizes.empty())
          candidateBlockSizes.reserve(targetRank);
        for (unsigned dim = 0; dim < targetRank; ++dim) {
          if (dim < candidateBlockSizes.size())
            continue;
          Value blockSize = acqInfo.partitionSizes[dim];
          if (!blockSize || ValueAnalysis::isZeroConstant(
                                ValueAnalysis::stripNumericCasts(blockSize))) {
            candidateBlockSizes.clear();
            break;
          }
          candidateBlockSizes.push_back(blockSize);
        }
        if (!candidateBlockSizes.empty()) {
          ctx.preferBlockND = true;
          ctx.preferredOuterRank = targetRank;
          blockSizesForNDBlock.assign(candidateBlockSizes.begin(),
                                      candidateBlockSizes.end());
          ARTS_DEBUG("    Acquire[" << idx
                                    << "]: explicit distributed contract "
                                       "prefers N-D block partitioning");
        }
      }
    }

    ctx.acquires.push_back(info);
    ARTS_DEBUG("    Acquire["
               << idx << "]: mode=" << static_cast<int>(info.accessMode)
               << ", canEW=" << info.canElementWise
               << ", canBlock=" << info.canBlock << ", ownerDimsCount="
               << static_cast<unsigned>(info.ownerDimsCount) << ", ownerDims=["
               << info.ownerDims[0] << "," << info.ownerDims[1] << ","
               << info.ownerDims[2] << "," << info.ownerDims[3] << "]"
               << ", acquireAttr="
               << (acquireMode ? static_cast<int>(*acquireMode) : -1));
    dbPartitionTraceImpl([&](llvm::raw_ostream &os) {
      os << "alloc=" << getArtsId(allocOp.getOperation())
         << " acquire=" << getArtsId(acquire.getOperation())
         << " accessMode=" << static_cast<int>(info.accessMode)
         << " accessPattern=" << static_cast<int>(info.accessPattern)
         << " depPattern=" << static_cast<int>(info.depPattern)
         << " canBlock=" << info.canBlock << " canEW=" << info.canElementWise
         << " hasDirect="
         << (contractSummary ? contractSummary->hasDirectAccess() : false)
         << " hasIndirect=" << hasIndirect
         << " inferredOffsets=" << acqInfo.partitionOffsets.size()
         << " inferredSizes=" << acqInfo.partitionSizes.size()
         << " needsFullRange=" << acqInfo.needsFullRange;
    });
    ++idx;
  }

  if (preserveExplicitContractLayout) {
    ARTS_DEBUG("  Preserving explicit block-contract leaf acquire layout; "
               "skipping allocation rewrite");
    ++numAllocsPreservedForLeafContracts;
    heuristics.recordDecision(
        "Partition-PreserveLeafContractLayout", true,
        "leaf acquire uses explicit distributed contract offsets; preserving "
        "layout",
        allocOp.getOperation(), {});
    return true;
  }

  return false;
}

/// Compute stencil halo bounds from acquire nodes and multi-entry structure.
/// Returns the computed StencilInfo, or std::nullopt if stencil mode should
/// be downgraded (e.g. single-sided halo).
std::optional<StencilInfo> DbPartitioningPass::computeStencilHaloInfo(
    DbAllocOp allocOp, DbAllocNode *allocNode,
    ArrayRef<DbAcquireNode *> allocAcquireNodes,
    ArrayRef<std::optional<DbAnalysis::AcquireContractSummary>>
        acquireContractSummaries,
    SmallVectorImpl<AcquirePartitionInfo> &acquireInfos,
    bool allocHasStencilPattern, DbAnalysis &dbAnalysis,
    PartitioningDecision &decision, PartitioningContext &ctx,
    OpBuilder &builder, Location loc) {
  StencilInfo info;
  info.haloLeft = 0;
  info.haloRight = 0;

  /// Collect stencil bounds from persisted lowering contracts. For multi-entry
  /// acquires, recover the same halo directly from the explicit IR structure
  /// instead of consulting graph-only stencil facts.
  for (auto [acqNode, contractSummary] :
       llvm::zip_equal(allocAcquireNodes, acquireContractSummaries)) {
    if (!acqNode)
      continue;
    DbAcquireOp acq = acqNode->getDbAcquireOp();
    if (!acq)
      continue;

    if (auto contractBounds = getSingleOwnerContractStencilBounds(
            contractSummary ? &*contractSummary : nullptr)) {
      info.haloLeft = std::max(info.haloLeft, contractBounds->first.haloLeft());
      info.haloRight =
          std::max(info.haloRight, contractBounds->first.haloRight());
      continue;
    }

    /// Last resort: for multi-entry stencil acquires, recover halo widths from
    /// the explicit entry structure.
    if (acq.hasMultiplePartitionEntries()) {
      int64_t minOffset = 0, maxOffset = 0;
      if (DbAnalysis::hasMultiEntryStencilPattern(acq, minOffset, maxOffset)) {
        /// minOffset is negative (left halo), maxOffset is positive (right)
        int64_t haloL = -minOffset;
        int64_t haloR = maxOffset;
        info.haloLeft = std::max(info.haloLeft, haloL);
        info.haloRight = std::max(info.haloRight, haloR);
        ARTS_DEBUG("  Extracted stencil bounds from multi-entry: haloLeft="
                   << haloL << ", haloRight=" << haloR);
      }
    }
  }

  bool hasReadStencilAcquire = false;
  for (auto [acqNode, contractSummary] :
       llvm::zip_equal(allocAcquireNodes, acquireContractSummaries)) {
    if (!acqNode)
      continue;
    DbAcquireOp acquire = acqNode->getDbAcquireOp();
    if (!acquire || acquire.getMode() != ArtsMode::in)
      continue;
    if (dbAnalysis.hasCanonicalAcquireStencilSemantics(
            acquire, contractSummary ? &*contractSummary : nullptr) ||
        allocHasStencilPattern) {
      hasReadStencilAcquire = true;
      break;
    }
  }

  auto allocPattern = getDbAccessPattern(allocOp.getOperation());
  if ((info.haloLeft == 0 || info.haloRight == 0) && hasReadStencilAcquire &&
      allocHasStencilPattern) {
    if (allocPattern && *allocPattern == DbAccessPattern::stencil) {
      info.haloLeft = std::max<int64_t>(info.haloLeft, 1);
      info.haloRight = std::max<int64_t>(info.haloRight, 1);
      ARTS_DEBUG("  Applying stencil halo fallback from db_alloc access "
                 "pattern: haloLeft="
                 << info.haloLeft << ", haloRight=" << info.haloRight);
    }
  }

  /// Get total rows from first element dimension.
  /// For stencil loops, the logical owned span excludes halo rows, so use
  /// (rows - haloLeft - haloRight) when possible.
  if (!allocOp.getElementSizes().empty()) {
    if (auto staticSize =
            arts::extractBlockSizeFromHint(allocOp.getElementSizes()[0]))
      info.totalRows = arts::createConstantIndex(builder, loc, *staticSize);
    else
      info.totalRows = allocOp.getElementSizes()[0];

    if (info.totalRows && (info.haloLeft > 0 || info.haloRight > 0)) {
      if (!info.totalRows.getType().isIndex())
        info.totalRows = builder.create<arith::IndexCastOp>(
            loc, builder.getIndexType(), info.totalRows);

      Value haloTotal = arts::createConstantIndex(
          builder, loc, info.haloLeft + info.haloRight);
      Value zero = arts::createZeroIndex(builder, loc);
      Value canSubtract = builder.create<arith::CmpIOp>(
          loc, arith::CmpIPredicate::uge, info.totalRows, haloTotal);
      Value reduced =
          builder.create<arith::SubIOp>(loc, info.totalRows, haloTotal);
      info.totalRows =
          builder.create<arith::SelectOp>(loc, canSubtract, reduced, zero);
    }
  }

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
    return std::nullopt;
  }

  return info;
}

/// Resolve block-plan sizes and partition dimensions, and reconcile
/// per-acquire dimension consistency. Returns false if the caller should
/// early-return the original allocOp (block plan resolution failed).
bool DbPartitioningPass::resolveBlockPlanSizes(
    SmallVectorImpl<Value> &blockSizesForPlan,
    SmallVectorImpl<unsigned> &partitionedDimsForPlan,
    SmallVectorImpl<AcquirePartitionInfo> &acquireInfos,
    SmallVectorImpl<Value> &blockSizesForNDBlock,
    PartitioningDecision &decision, PartitioningContext &ctx,
    std::optional<StencilInfo> &stencilInfo, DbAllocOp allocOp,
    DbAllocNode *allocNode, DbHeuristics &heuristics, OpBuilder &builder,
    Location loc) {
  bool useNodesForFallback = false;
  bool clampStencilFallbackWorkers = false;
  if (AM) {
    AbstractMachine &machine = AM->getAbstractMachine();
    if (machine.hasValidNodeCount() && machine.getNodeCount() > 1) {
      /// Stencil ESD requires block-aligned worker chunk offsets.
      /// Worker chunks are produced by EDT lowering using worker-level
      /// decomposition. Keeping stencil fallback sizing worker-based avoids
      /// misalignment (e.g. node-sized blocks with worker-sized tasks) that
      /// would otherwise force DbStencilRewriter into block fallback.
      useNodesForFallback = (decision.mode != PartitionMode::stencil);
    }
  }
  if (decision.mode == PartitionMode::stencil) {
    bool hasInternodeUsers =
        llvm::any_of(acquireInfos, [](const AcquirePartitionInfo &info) {
          if (!info.acquire)
            return false;
          auto edt = info.acquire->getParentOfType<EdtOp>();
          return edt && edt.getConcurrency() == EdtConcurrency::internode;
        });
    clampStencilFallbackWorkers = !hasInternodeUsers;
  }

  DbBlockPlanInput blockPlanInput{allocOp,
                                  acquireInfos,
                                  ctx.blockSize,
                                  /*dynamicBlockSizeHint=*/Value(),
                                  blockSizesForNDBlock,
                                  decision.outerRank,
                                  &builder,
                                  loc,
                                  useNodesForFallback,
                                  clampStencilFallbackWorkers};
  auto blockPlanOr = resolveDbBlockPlan(blockPlanInput);
  if (failed(blockPlanOr) || blockPlanOr->blockSizes.empty()) {
    ARTS_DEBUG("  No block-plan sizes available - falling back to coarse");
    heuristics.recordDecision(
        "Partition-NoBlockSize", false,
        "block mode requested but no block-plan sizes available",
        allocOp.getOperation(), {});
    resetCoarseAcquireRanges(allocOp, allocNode, builder);
    return false;
  }

  blockSizesForPlan.assign(
      std::make_move_iterator(blockPlanOr->blockSizes.begin()),
      std::make_move_iterator(blockPlanOr->blockSizes.end()));
  partitionedDimsForPlan.assign(
      std::make_move_iterator(blockPlanOr->partitionedDims.begin()),
      std::make_move_iterator(blockPlanOr->partitionedDims.end()));

  collapseTrailingFullExtentPartitionDims(allocOp, blockSizesForPlan,
                                          partitionedDimsForPlan);

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
  /// require full-range to preserve correctness.
  ///
  /// Allow 1-D plans to keep the historical leading-prefix behavior. For N-D
  /// plans, allow acquires to localize any subset of the chosen plan dims;
  /// omitted dims widen to full-range later in the planner.
  if (!partitionedDimsForPlan.empty()) {
    for (auto &info : acquireInfos) {
      if (info.needsFullRange)
        continue;
      if (info.partitionDims.empty()) {
        markAcquireNeedsFullRange(info);
        ARTS_DEBUG("  Acquire missing partition dims under concrete plan; "
                   "forcing full-range access");
        continue;
      }
      bool compatible = true;
      if (partitionedDimsForPlan.size() > 1) {
        unsigned explicitRank =
            std::min(info.partitionOffsets.size(), info.partitionSizes.size());
        if (explicitRank > info.partitionDims.size())
          compatible = false;

        SmallVector<bool> seenDim(allocOp.getElementSizes().size(), false);
        for (unsigned dim : info.partitionDims) {
          if (dim >= seenDim.size() || seenDim[dim] ||
              !llvm::is_contained(partitionedDimsForPlan, dim)) {
            compatible = false;
            break;
          }
          seenDim[dim] = true;
        }
      } else {
        compatible = info.partitionDims.size() >= partitionedDimsForPlan.size();
        if (compatible) {
          for (unsigned i = 0; i < partitionedDimsForPlan.size(); ++i) {
            if (info.partitionDims[i] != partitionedDimsForPlan[i]) {
              compatible = false;
              break;
            }
          }
        }
      }
      if (!compatible) {
        markAcquireNeedsFullRange(info);
        ARTS_DEBUG("  Acquire partition dims mismatch with plan; "
                   "forcing full-range access");
      }
    }
  }

  return true;
}

/// Assemble the rewrite plan from partition decision and apply it via
/// DbRewriter. Returns the rewritten DbAllocOp on success.
FailureOr<DbAllocOp> DbPartitioningPass::assembleAndApplyRewritePlan(
    PartitioningDecision &decision,
    SmallVectorImpl<AcquirePartitionInfo> &acquireInfos,
    SmallVectorImpl<Value> &blockSizesForPlan,
    SmallVectorImpl<unsigned> &partitionedDimsForPlan,
    SmallVectorImpl<Value> &newOuterSizes,
    SmallVectorImpl<Value> &newInnerSizes,
    std::optional<StencilInfo> &stencilInfo, DbAllocOp allocOp,
    DbHeuristics &heuristics, OpBuilder &builder) {
  DbRewritePlan plan(decision.mode);
  plan.blockSizes.assign(blockSizesForPlan.begin(), blockSizesForPlan.end());
  plan.partitionedDims.assign(partitionedDimsForPlan.begin(),
                              partitionedDimsForPlan.end());
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

  /// Propagate stencil center offset to all acquires when using allocation-wide
  /// stencil mode or mixed block+stencil rewriting. This keeps base offsets
  /// aligned for non-stencil accesses (e.g., outputs) when loop bounds are
  /// shifted (i starts at 1).
  if (decision.mode == PartitionMode::stencil ||
      (decision.mode == PartitionMode::block && plan.stencilInfo)) {
    std::optional<int64_t> centerOffset;
    for (auto &info : acquireInfos) {
      if (!info.acquire)
        continue;
      auto contract = getLoweringContract(info.acquire.getPtr());
      if (!contract)
        contract = getLoweringContract(info.acquire.getOperation(), builder,
                                       info.acquire.getLoc());
      if (contract && contract->spatial.centerOffset) {
        centerOffset = *contract->spatial.centerOffset;
        break;
      }
    }
    if (centerOffset) {
      for (auto &info : acquireInfos) {
        if (!info.acquire)
          continue;
        auto contract = getLoweringContract(info.acquire.getPtr());
        if (!contract)
          contract = getLoweringContract(info.acquire.getOperation(), builder,
                                         info.acquire.getLoc());
        if (contract && contract->spatial.centerOffset)
          continue;

        LoweringContractInfo updated =
            contract.value_or(LoweringContractInfo{});
        updated.spatial.centerOffset = *centerOffset;
        upsertLoweringContract(builder, info.acquire.getLoc(),
                               info.acquire.getPtr(), updated);
      }
    }
  }

  /// Build DbRewriteAcquire list from AcquirePartitionInfo
  /// Include ALL acquires - invalid ones get Coarse fallback (offset=0,
  /// size=totalSize)
  SmallVector<DbRewriteAcquire> rewriteAcquires;
  bool enableMixedStencilRewrite =
      decision.mode == PartitionMode::block && plan.stencilInfo.has_value();

  for (auto &info : acquireInfos) {
    if (!info.acquire)
      continue;

    DbRewriteAcquire rewriteInfo;
    rewriteInfo.acquire = info.acquire;

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
    view.partitionDims.assign(info.partitionDims.begin(),
                              info.partitionDims.end());
    view.accessPattern = info.accessPattern;
    view.isValid = info.isValid;
    view.hasIndirectAccess = info.hasIndirectAccess;
    view.needsFullRange = info.needsFullRange;
    PartitionMode rewriteMode =
        enableMixedStencilRewrite ? PartitionMode::stencil : decision.mode;
    buildRewriteAcquire(rewriteMode, view, allocOp, plan, rewriteInfo, builder);

    rewriteAcquires.push_back(rewriteInfo);
  }

  auto rewriter = DbRewriter::create(allocOp, rewriteAcquires, plan);
  auto result = rewriter->apply(builder);

  /// Set partition attribute on new alloc
  if (succeeded(result)) {
    setPartitionMode(*result, decision.mode);
    switch (decision.mode) {
    case PartitionMode::block:
      ++numAllocsPartitionedBlock;
      break;
    case PartitionMode::stencil:
      ++numAllocsPartitionedStencil;
      break;
    case PartitionMode::fine_grained:
      ++numAllocsPartitionedFineGrained;
      break;
    case PartitionMode::coarse:
      break;
    }

    AM->getMetadataManager().transferMetadata(allocOp, result.value());

    /// EXT-PART-5: Write resolved partition decisions back to the
    /// LoweringContractOp on the alloc's ptr so downstream passes can read
    /// the block shape, owner dims, and distribution version directly.
    if (!decision.isCoarse()) {
      feedbackPartitionDecisionToContract(result.value(), blockSizesForPlan,
                                          partitionedDimsForPlan, decision.mode,
                                          builder);
    }

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
    ++numPartitionRewriteFailures;
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
  AM->invalidateAndRebuildGraphs(module);
}

namespace mlir {
namespace arts {
std::unique_ptr<Pass>
createDbPartitioningPass(mlir::arts::AnalysisManager *AM) {
  return std::make_unique<DbPartitioningPass>(AM);
}
} // namespace arts
} // namespace mlir
