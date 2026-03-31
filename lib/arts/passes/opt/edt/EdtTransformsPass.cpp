///==========================================================================///
/// File: EdtTransformsPass.cpp
///
/// Post-DB EDT transforms pass. Hosts EDT-level transformations that run
/// after DbTransforms and before the final DbModeTightening mode tightening.
///
/// Current transforms:
///   ET-1:      Task granularity control -- estimate task cost from EdtInfo
///              metrics and annotate LoweringContractOps with the cost.
///
///   ET-2:      Data affinity placement -- find the primary write target
///              for each EDT and annotate with affinity_db for NUMA-aware
///              task placement.
///
///   ET-3:      Dep chain narrowing -- for stencil/wavefront EDTs,
///              mark neighbor halo dependencies as narrowable when the
///              actual data footprint is smaller than the full DB, enabling
///              downstream lowering to use arts_add_dependence_at (ESD).
///
///   ET-5:      Reduction strategy selection -- detect scalar reduction
///              patterns (load-modify-store on single-element inout DBs)
///              and annotate EDTs with arts.reduction_strategy for lowering.
///
///   ET-6:      Critical path analysis -- compute longest dependency chain
///              via topological sort and annotate critical_path_distance.
///
///   EXT-EDT-2: Dead dependency elimination -- remove unused dependency
///              slots whose block arguments have zero uses or are only
///              consumed by DbReleaseOps.
///
///==========================================================================///

/// LLVM ADT
#include "llvm/ADT/DenseSet.h"
/// Dialects
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/Pass/Pass.h"
/// Arts
#include "arts/Dialect.h"
#include "arts/analysis/AnalysisManager.h"
#include "arts/analysis/db/DbAnalysis.h"
#include "arts/analysis/edt/EdtInfo.h"
#include "arts/analysis/graphs/base/EdgeBase.h"
#include "arts/analysis/graphs/base/NodeBase.h"
#include "arts/analysis/graphs/edt/EdtGraph.h"
#include "arts/analysis/graphs/edt/EdtNode.h"
#include "arts/analysis/value/ValueAnalysis.h"
#define GEN_PASS_DEF_EDTTRANSFORMS
#include "arts/Dialect.h"
#include "arts/passes/Passes.h"
#include "mlir/Pass/Pass.h"
#include "arts/passes/Passes.h.inc"
#include "arts/passes/Passes.h"
#include "arts/utils/DbUtils.h"
#include "arts/utils/EdtUtils.h"
#include "arts/utils/LoopUtils.h"
#include "arts/utils/LoweringContractUtils.h"
#include "arts/utils/OperationAttributes.h"
/// Debug
#include "arts/utils/Debug.h"
#include <algorithm>
ARTS_DEBUG_SETUP(edt_transforms);

using namespace mlir;
using namespace mlir::func;
using namespace mlir::arts;

namespace {

/// Minimum cost threshold below which an EDT is considered trivially small.
/// Tasks below this threshold emit a diagnostic warning suggesting fusion.
static constexpr int64_t kSmallTaskThreshold = 64;

/// Weight multiplier for loop-nested operations.  Each level of loop nesting
/// multiplies the effective cost of operations inside that loop.
static constexpr int64_t kLoopDepthMultiplier = 8;

/// ET-5: Worker count threshold for choosing atomic vs tree reduction.
/// Below this threshold, atomic updates are preferred; above, tree reduction
/// avoids excessive contention on shared cache lines.
static constexpr int64_t kAtomicWorkerThreshold = 16;

/// ET-5: Reduction strategy name constants.
/// TODO(QUALITY): When a lowering consumer for arts.reduction_strategy is
/// implemented, replace these string constants with a proper MLIR enum
/// attribute (defined in Attributes.td) for full type safety.
namespace ReductionStrategyNames {
constexpr llvm::StringLiteral Atomic = "atomic";
constexpr llvm::StringLiteral Tree = "tree";
constexpr llvm::StringLiteral LocalAccumulate = "local_accumulate";
} // namespace ReductionStrategyNames

struct EdtTransformsPass : public impl::EdtTransformsBase<EdtTransformsPass> {
  EdtTransformsPass(mlir::arts::AnalysisManager *AM) : AM(AM) {
    assert(AM && "AnalysisManager must be provided externally");
  }

  void runOnOperation() override;

private:
  mlir::arts::AnalysisManager *AM = nullptr;

  /// ET-1: Estimate task granularity and annotate dependency contracts.
  /// Returns the number of annotated EDTs.
  unsigned estimateTaskGranularity();

  /// ET-6: Compute critical path distances for all EDTs in a function.
  /// Returns the number of annotated EDTs.
  unsigned analyzeCriticalPath(func::FuncOp func, EdtGraph &edtGraph);

  /// ET-2: Place EDTs near the data they primarily write to.
  /// Returns the number of EDTs annotated with affinity hints.
  unsigned placeEdtsByAffinity();

  /// ET-3: Narrow dependency chains for stencil/wavefront patterns.
  /// For neighbor halo reads, mark the dependency as narrowable when the
  /// actual data footprint is smaller than the full DB.
  /// Returns the number of narrowed dependencies.
  unsigned narrowDepChains();

  /// ET-5: Detect scalar reduction patterns on single-element inout DBs
  /// and annotate EDTs with a strategy recommendation (atomic/tree/
  /// local_accumulate) for lowering to consume.
  /// Returns the number of annotated EDTs.
  unsigned selectReductionStrategies();

  /// EXT-EDT-2: Walk all EDTs and remove unused dependency slots.
  /// Returns the number of eliminated dependencies.
  unsigned eliminateDeadDependencies();

  struct ModuleEdtMetrics {
    unsigned totalEdts = 0;
  };

  ModuleEdtMetrics gatherModuleEdtFacts();
  void logModuleEdtSummary(const ModuleEdtMetrics &metrics) const;
};
} // namespace

/// Iterates EDT dependencies, finds the DbAcquireOp and associated
/// LoweringContractOp for each, and applies the given mutation to the
/// contract info before upserting it back.
static void
annotateEdtDepContracts(EdtOp edt,
                        function_ref<void(DbAcquireOp acquire, Value ptr,
                                          LoweringContractInfo &info)>
                            mutate) {
  for (Value dep : edt.getDependencies()) {
    auto acquire = dep.getDefiningOp<DbAcquireOp>();
    if (!acquire)
      continue;
    Value ptr = acquire.getPtr();
    auto contractOp = getLoweringContractOp(ptr);
    LoweringContractInfo info =
        contractOp ? getLoweringContract(ptr).value_or(LoweringContractInfo{})
                   : LoweringContractInfo{};
    mutate(acquire, ptr, info);
    if (!info.empty()) {
      if (contractOp) {
        OpBuilder builder(contractOp.getContext());
        builder.setInsertionPointAfter(contractOp.getOperation());
        upsertLoweringContract(builder, contractOp.getLoc(), ptr, info);
      } else {
        OpBuilder builder(acquire.getContext());
        builder.setInsertionPointAfter(acquire.getOperation());
        upsertLoweringContract(builder, acquire.getLoc(), ptr, info);
      }
    }
  }
}

void EdtTransformsPass::runOnOperation() {
  ARTS_INFO_HEADER(EdtTransformsPass);
  ModuleEdtMetrics metrics = gatherModuleEdtFacts();

  ///===--------------------------------------------------------------------===///
  /// ET-1: Task granularity control
  ///
  /// Walk all EDTs, compute a cost estimate from EdtInfo metrics, and
  /// annotate each EDT's dependency LoweringContractOps with the cost.
  /// Trivially small tasks (< threshold) emit a diagnostic warning.
  ///===--------------------------------------------------------------------===///
  unsigned et1Count = estimateTaskGranularity();
  if (et1Count > 0)
    ARTS_INFO("ET-1: annotated " << et1Count
                                 << " EDTs with estimated task cost");

  ///===--------------------------------------------------------------------===///
  /// ET-2: Data affinity placement
  ///
  /// For each EDT, find the DB it primarily writes to (the one with the
  /// most access bytes among inout/out acquires).  If the primary write
  /// target is a distributed/partitioned allocation, annotate the EDT
  /// with an "affinity_db" attribute pointing to that DB so downstream
  /// scheduling/lowering can place the task near its dominant data.
  ///===--------------------------------------------------------------------===///
  unsigned et2Count = placeEdtsByAffinity();
  if (et2Count > 0)
    ARTS_INFO("ET-2: annotated " << et2Count
                                 << " EDTs with data affinity placement");

  ///===--------------------------------------------------------------------===///
  /// ET-5: Reduction strategy selection
  ///
  /// Detect scalar reduction patterns (load-modify-store on single-element
  /// inout DBs) and annotate each matching EDT with an
  /// arts.reduction_strategy attribute recommending atomic, tree, or
  /// local_accumulate lowering.
  ///===--------------------------------------------------------------------===///
  unsigned et5Count = selectReductionStrategies();
  if (et5Count > 0)
    ARTS_INFO("ET-5: annotated " << et5Count
                                 << " EDTs with reduction strategy");

  ///===--------------------------------------------------------------------===///
  /// ET-3: Dep chain narrowing
  ///
  /// For stencil and wavefront EDTs, identify neighbor halo dependencies
  /// whose actual data footprint is smaller than the full DB. Mark these
  /// dependencies as narrowable so downstream lowering can emit
  /// arts_add_dependence_at (ESD) instead of a full-DB dependency.
  ///===--------------------------------------------------------------------===///
  unsigned et3Count = narrowDepChains();
  if (et3Count > 0)
    ARTS_INFO("ET-3: narrowed " << et3Count
                                << " dependency chains for stencil/wavefront");

  ///===--------------------------------------------------------------------===///
  /// EXT-EDT-2: Dead dependency elimination
  ///
  /// Walk all EDTs and remove dependency slots whose block arguments are
  /// unused or only consumed by DbReleaseOps. This tightens the task graph
  /// and can unlock further DbScratchElimination.
  ///===--------------------------------------------------------------------===///
  unsigned extEdt2Count = eliminateDeadDependencies();
  if (extEdt2Count > 0)
    ARTS_INFO("EXT-EDT-2: eliminated " << extEdt2Count << " dead dependencies");

  logModuleEdtSummary(metrics);

  ARTS_INFO_FOOTER(EdtTransformsPass);
}

EdtTransformsPass::ModuleEdtMetrics EdtTransformsPass::gatherModuleEdtFacts() {
  ModuleOp module = getOperation();
  ModuleEdtMetrics metrics;

  /// Walk all functions, gather EDT metrics, and run ET-6 critical path
  /// analysis while the graph is already hot.
  module.walk([&](func::FuncOp func) {
    auto &edtGraph = AM->getEdtAnalysis().getOrCreateEdtGraph(func);
    if (edtGraph.size() == 0)
      return;

    ARTS_DEBUG("Processing function: " << func.getName());

    func.walk([&](EdtOp edt) {
      EdtNode *node = edtGraph.getEdtNode(edt);
      if (!node)
        return;

      ++metrics.totalEdts;

      ARTS_DEBUG("  EDT [" << node->getHierId() << "]");
    });

    unsigned et6Count = analyzeCriticalPath(func, edtGraph);
    ARTS_DEBUG("ET-6: Annotated " << et6Count
                                  << " EDTs with critical path distance");
  });

  return metrics;
}

void EdtTransformsPass::logModuleEdtSummary(
    const ModuleEdtMetrics &metrics) const {
  if (metrics.totalEdts > 0) {
    ARTS_INFO("EDT transforms summary: edts=" << metrics.totalEdts);
    return;
  }
  ARTS_INFO("No EDTs found in module");
}

///===----------------------------------------------------------------------===///
/// ET-1: Task granularity control
///===----------------------------------------------------------------------===///
unsigned EdtTransformsPass::estimateTaskGranularity() {
  ModuleOp module = getOperation();
  unsigned count = 0;

  module.walk([&](func::FuncOp func) {
    auto &edtGraph = AM->getEdtAnalysis().getOrCreateEdtGraph(func);
    if (edtGraph.size() == 0)
      return;

    func.walk([&](EdtOp edt) {
      EdtNode *node = edtGraph.getEdtNode(edt);
      if (!node)
        return;

      const EdtInfo &info = node->getInfo();

      /// Cost model: use loop depth as proxy for task weight.
      /// TODO: populate EdtInfo with actual op counts for better estimates.
      int64_t cost = 1;
      if (info.maxLoopDepth > 0) {
        int64_t depthScale = 1;
        for (uint64_t d = 0; d < info.maxLoopDepth; ++d) {
          depthScale *= kLoopDepthMultiplier;
          if (depthScale > 1000000) {
            depthScale = 1000000;
            break;
          }
        }
        cost = depthScale;
      }

      ARTS_DEBUG("ET-1: EDT [" << node->getHierId() << "]"
                               << " maxLoopDepth=" << info.maxLoopDepth
                               << " => estimatedTaskCost=" << cost);

      /// Warn about trivially small tasks that may benefit from fusion.
      if (cost < kSmallTaskThreshold) {
        edt.emitWarning("ET-1: trivially small task (estimated cost ")
            << cost << " < " << kSmallTaskThreshold
            << "); consider fusing with a neighbour EDT";
        ++count;
      }
    });
  });

  return count;
}

///===----------------------------------------------------------------------===///
/// ET-6: Critical path analysis
///===----------------------------------------------------------------------===///
unsigned EdtTransformsPass::analyzeCriticalPath(func::FuncOp func,
                                                EdtGraph &edtGraph) {
  /// Collect all EDT nodes from the graph.
  SmallVector<EdtNode *, 16> allNodes;
  edtGraph.forEachNode([&](NodeBase *base) {
    if (auto *edtNode = dyn_cast<EdtNode>(base))
      allNodes.push_back(edtNode);
  });

  if (allNodes.empty())
    return 0;

  /// Compute in-degree for each node (counting only edges within EDT nodes).
  DenseMap<EdtNode *, unsigned> inDegree;
  for (auto *node : allNodes)
    inDegree[node] = 0;

  for (auto *node : allNodes) {
    for (auto *edge : node->getInEdges()) {
      auto *fromNode = dyn_cast<EdtNode>(edge->getFrom());
      if (fromNode && inDegree.count(fromNode))
        inDegree[node]++;
    }
  }

  /// Kahn's algorithm for topological sort.
  SmallVector<EdtNode *, 16> topoOrder;
  SmallVector<EdtNode *, 16> worklist;

  for (auto *node : allNodes) {
    if (inDegree[node] == 0)
      worklist.push_back(node);
  }

  /// Sort the initial worklist by hier ID for deterministic ordering.
  llvm::sort(worklist, [](EdtNode *a, EdtNode *b) {
    return a->getHierId() < b->getHierId();
  });

  while (!worklist.empty()) {
    EdtNode *current = worklist.pop_back_val();
    topoOrder.push_back(current);

    /// Collect successors from outgoing edges.
    SmallVector<EdtNode *, 8> successors;
    for (auto *edge : current->getOutEdges()) {
      auto *toNode = dyn_cast<EdtNode>(edge->getTo());
      if (toNode && inDegree.count(toNode)) {
        inDegree[toNode]--;
        if (inDegree[toNode] == 0)
          successors.push_back(toNode);
      }
    }

    /// Sort successors for deterministic ordering.
    llvm::sort(successors, [](EdtNode *a, EdtNode *b) {
      return a->getHierId() < b->getHierId();
    });

    worklist.append(successors.begin(), successors.end());
  }

  /// If topological sort did not cover all nodes, there is a cycle.
  /// Assign distance 0 to any remaining nodes (defensive).
  if (topoOrder.size() != allNodes.size()) {
    ARTS_WARN("ET-6: EDT dependency graph has a cycle in function "
              << func.getName() << "; assigning distance 0 to "
              << (allNodes.size() - topoOrder.size()) << " unreachable EDTs");
  }

  /// Compute critical path distance in topological order.
  /// Distance for root EDTs (no predecessors in the graph) is 0.
  /// For others: max(predecessor distances) + 1.
  DenseMap<EdtNode *, int64_t> distance;
  int64_t maxDistance = 0;

  for (auto *node : topoOrder) {
    int64_t dist = 0;
    for (auto *edge : node->getInEdges()) {
      auto *predNode = dyn_cast<EdtNode>(edge->getFrom());
      if (predNode) {
        auto it = distance.find(predNode);
        if (it != distance.end())
          dist = std::max(dist, it->second + 1);
      }
    }
    distance[node] = dist;
    if (dist > maxDistance)
      maxDistance = dist;
  }

  /// Annotate each EDT with the critical_path_distance attribute and
  /// update any LoweringContractOps on its dependencies.
  unsigned annotated = 0;
  auto *ctx = func.getContext();
  auto i64Type = IntegerType::get(ctx, 64);

  for (auto *node : topoOrder) {
    int64_t dist = distance[node];
    EdtOp edt = node->getEdtOp();

    /// Set critical_path_distance directly on the EdtOp.
    edt->setAttr(AttrNames::Operation::Contract::CriticalPathDistance,
                 IntegerAttr::get(i64Type, dist));

    /// Also update LoweringContractOps on the EDT's dependency acquires.
    annotateEdtDepContracts(edt, [&](DbAcquireOp /*acquire*/, Value /*ptr*/,
                                     LoweringContractInfo &info) {
      info.criticalPathDistance = dist;
    });

    ++annotated;

    ARTS_DEBUG("ET-6: EDT [" << node->getHierId()
                             << "] critical_path_distance=" << dist);
  }

  if (annotated > 0) {
    ARTS_INFO("ET-6: function "
              << func.getName() << ": " << annotated
              << " EDTs annotated, max critical path depth=" << maxDistance);
  }

  return annotated;
}

///===----------------------------------------------------------------------===///
/// ET-2: Data affinity placement
///===----------------------------------------------------------------------===///
unsigned EdtTransformsPass::placeEdtsByAffinity() {
  ModuleOp module = getOperation();
  unsigned count = 0;

  module.walk([&](func::FuncOp func) {
    auto &edtGraph = AM->getEdtAnalysis().getOrCreateEdtGraph(func);
    if (edtGraph.size() == 0)
      return;

    func.walk([&](EdtOp edt) {
      EdtNode *node = edtGraph.getEdtNode(edt);
      if (!node)
        return;

      /// Find the first writer acquire's root DbAllocOp as affinity target.
      DbAllocOp bestAlloc = nullptr;
      for (Value dep : edt.getDependencies()) {
        auto acquire = dep.getDefiningOp<DbAcquireOp>();
        if (!acquire)
          continue;

        /// Only consider writer modes (out, inout).
        if (!DbUtils::isWriterMode(acquire.getMode()))
          continue;

        /// Trace this acquire back to its root DbAllocOp.
        auto *rootOp = DbUtils::getUnderlyingDbAlloc(acquire.getSourcePtr());
        auto allocOp = dyn_cast_or_null<DbAllocOp>(rootOp);
        if (!allocOp)
          continue;

        bestAlloc = allocOp;
        break;
      }

      if (!bestAlloc)
        return;

      /// Only annotate if the target allocation is distributed/partitioned.
      /// Non-distributed (coarse) allocations have no meaningful placement
      /// affinity since they reside in a single location anyway.
      if (!hasDistributedDbAllocation(bestAlloc.getOperation()) &&
          !DbAnalysis::isFineGrained(bestAlloc))
        return;

      /// Annotate the EdtOp with the affinity_db attribute.
      /// The value is the MLIR arts.id of the target DbAllocOp, which
      /// downstream passes and the runtime can use for placement decisions.
      int64_t allocId = getArtsId(bestAlloc.getOperation());
      if (allocId <= 0) {
        /// If the alloc does not have an arts.id, fall back to annotating
        /// with a symbolic reference using the alloc's SSA name is not
        /// feasible. Instead, we use a boolean marker that records affinity
        /// was computed. The actual placement will be resolved during
        /// lowering by matching the EDT's dependency list.
        ARTS_DEBUG("ET-2: EDT [" << node->getHierId()
                                 << "] primary write target has no arts.id"
                                 << "; skipping affinity annotation");
        return;
      }

      auto *ctx = edt.getContext();
      auto i64Type = IntegerType::get(ctx, 64);
      edt->setAttr(AttrNames::Operation::Contract::AffinityDb,
                   IntegerAttr::get(i64Type, allocId));
      ++count;

      ARTS_DEBUG("ET-2: EDT ["
                 << node->getHierId() << "]"
                 << " affinity_db=" << allocId << " (distributed="
                 << hasDistributedDbAllocation(bestAlloc.getOperation())
                 << ")");
    });
  });

  return count;
}

///===----------------------------------------------------------------------===///
/// ET-5: Reduction strategy selection
///===----------------------------------------------------------------------===///

unsigned EdtTransformsPass::selectReductionStrategies() {
  /// TODO: arts.reduction_strategy is annotation-only — no lowering pass
  /// currently reads this attribute. When lowering is implemented, it must
  /// handle atomic (via artsAtomicAddInArrayDb), tree (binary tree
  /// reduction EDT topology), and local_accumulate (per-worker accumulators).
  ModuleOp module = getOperation();
  unsigned count = 0;

  /// Retrieve worker count from module attributes (set by runtime config).
  int64_t workerCount = getRuntimeTotalWorkers(module).value_or(0);

  module.walk([&](func::FuncOp func) {
    auto &edtGraph = AM->getEdtAnalysis().getOrCreateEdtGraph(func);
    if (edtGraph.size() == 0)
      return;

    func.walk([&](EdtOp edt) {
      EdtNode *node = edtGraph.getEdtNode(edt);
      if (!node)
        return;

      /// Walk each dependency acquire for this EDT.
      for (Value dep : edt.getDependencies()) {
        auto acquire = dep.getDefiningOp<DbAcquireOp>();
        if (!acquire)
          continue;

        /// Only consider inout acquires — those are the ones that serialize.
        if (acquire.getMode() != ArtsMode::inout)
          continue;

        /// Single-element (size=1) DB — the common scalar reduction pattern.
        if (!DbAnalysis::hasSingleSize(acquire.getOperation()))
          continue;

        /// Trace back to the DbAllocOp and verify it is also single-element.
        auto *rootOp = DbUtils::getUnderlyingDbAlloc(acquire.getSourcePtr());
        auto allocOp = dyn_cast_or_null<DbAllocOp>(rootOp);
        if (!allocOp)
          continue;
        if (!DbAnalysis::isCoarseGrained(allocOp))
          continue;

        /// Find the block argument inside the EDT body for this acquire.
        auto [edtForArg, blockArg] = getEdtBlockArgumentForAcquire(acquire);
        if (!edtForArg || edtForArg != edt || !blockArg)
          continue;

        /// Look for a DbRefOp at index [0] on the block argument for the
        /// memref.
        DbRefOp dbRef = nullptr;
        for (Operation *user : blockArg.getUsers()) {
          if (isa<DbReleaseOp>(user))
            continue;
          auto ref = dyn_cast<DbRefOp>(user);
          if (!ref)
            continue;
          /// Accept only zero-index refs (single element access).
          if (!ref.getIndices().empty() &&
              !llvm::all_of(ref.getIndices(), [](Value v) {
                return ValueAnalysis::isZeroConstant(v);
              }))
            continue;
          if (dbRef) {
            /// Multiple DbRefOps — ambiguous; bail out conservatively.
            dbRef = nullptr;
            break;
          }
          dbRef = ref;
        }

        if (!dbRef)
          continue;

        /// Match the load-modify-store reduction pattern.
        auto reductionKind =
            EdtAnalysis::matchLoadModifyStore(dbRef.getResult(), edt.getBody());
        if (!reductionKind)
          continue;

        /// Select reduction strategy.
        StringRef strategy;
        bool isLoopNested = containsLoop(edt);

        if (isLoopNested) {
          /// Inside a loop nest: accumulate locally, then single atomic at end.
          strategy = ReductionStrategyNames::LocalAccumulate;
        } else if (workerCount > 0 && workerCount < kAtomicWorkerThreshold &&
                   EdtAnalysis::isAtomicCapable(*reductionKind)) {
          /// Small worker count and op maps to ARTS atomic intrinsic.
          strategy = ReductionStrategyNames::Atomic;
        } else if (workerCount >= kAtomicWorkerThreshold) {
          /// Large worker count: binary tree reduction avoids contention.
          strategy = ReductionStrategyNames::Tree;
        } else if (EdtAnalysis::isAtomicCapable(*reductionKind)) {
          /// Worker count unknown but op atomic-capable: default to atomic.
          strategy = ReductionStrategyNames::Atomic;
        } else {
          /// Non-atomic-capable with unknown worker count: tree is safe
          /// default.
          strategy = ReductionStrategyNames::Tree;
        }

        /// Stamp the attribute on the EdtOp.
        edt->setAttr(AttrNames::Operation::Contract::ReductionStrategy,
                     StringAttr::get(edt.getContext(), strategy));
        ++count;

        ARTS_DEBUG("ET-5: EDT [" << node->getHierId() << "]"
                                 << " reduction="
                                 << EdtAnalysis::reductionOpName(*reductionKind)
                                 << " strategy=" << strategy
                                 << " workerCount=" << workerCount
                                 << " loopNested=" << isLoopNested);

        /// One reduction annotation per EDT; first match determines strategy.
        break;
      }
    });
  });

  return count;
}

///===----------------------------------------------------------------------===///
/// Dep chain narrowing helper functions.
///===----------------------------------------------------------------------===///

/// Mark a dependency as narrowable by setting the NarrowableDep unit attribute.
/// Creates a new LoweringContractOp if one does not exist for the pointer.
static void markDepNarrowable(DbAcquireOp acquire, Value ptr,
                              LoweringContractOp contractOp,
                              std::optional<ArtsDepPattern> edtDepPattern,
                              ArrayRef<int64_t> haloFootprint = {}) {
  if (contractOp) {
    contractOp->setAttr(AttrNames::Operation::Contract::NarrowableDep,
                        UnitAttr::get(contractOp.getContext()));
  } else {
    /// No existing contract -- create one with the marker.
    LoweringContractInfo newInfo;
    if (!newInfo.depPattern && edtDepPattern)
      newInfo.depPattern = *edtDepPattern;
    OpBuilder builder(acquire.getContext());
    builder.setInsertionPointAfter(acquire.getOperation());
    auto newContractOp =
        upsertLoweringContract(builder, acquire.getLoc(), ptr, newInfo);
    if (newContractOp) {
      newContractOp->setAttr(AttrNames::Operation::Contract::NarrowableDep,
                             UnitAttr::get(newContractOp.getContext()));
    }
  }
}

/// Try to narrow a halo dependency with known static footprint or partition
/// hints. Returns true if the dependency was marked as narrowable.
static bool
tryNarrowHaloDep(DbAcquireOp acquire, Value ptr, LoweringContractOp contractOp,
                 std::optional<ArtsDepPattern> edtDepPattern,
                 std::optional<LoweringContractInfo> existingContract) {
  /// Read the contract's minOffsets/maxOffsets (consolidated by DT-2).
  /// These define the halo overlap region.
  LoweringContractInfo contractInfo =
      existingContract.value_or(LoweringContractInfo{});

  auto staticMins = contractInfo.getStaticMinOffsets();
  auto staticMaxs = contractInfo.getStaticMaxOffsets();

  if (staticMins && staticMaxs && !staticMins->empty() &&
      !staticMaxs->empty()) {
    /// We have static halo bounds. Compute the halo footprint
    /// as the per-dimension span: size[d] = max[d] - min[d].
    /// If any dimension has a non-zero span, the dependency
    /// can be narrowed to just that halo region.
    bool hasNonZeroHalo = false;
    SmallVector<int64_t, 4> haloFootprint;
    unsigned rank = std::min(staticMins->size(), staticMaxs->size());
    haloFootprint.reserve(rank);
    for (unsigned d = 0; d < rank; ++d) {
      int64_t span = (*staticMaxs)[d] - (*staticMins)[d];
      haloFootprint.push_back(span);
      if (span > 0)
        hasNonZeroHalo = true;
    }

    if (hasNonZeroHalo) {
      markDepNarrowable(acquire, ptr, contractOp, edtDepPattern, haloFootprint);
      ARTS_DEBUG("ET-3:   marked halo dependency as narrowable"
                 << " rank=" << rank << " haloFootprint=[" << haloFootprint[0]
                 << (rank > 1 ? ", ..." : "") << "]");
      return true;
    }
  }

  /// If we cannot statically determine the halo footprint from
  /// the contract, check if the acquire itself carries partition
  /// information that implies a sub-region access.
  auto partOffsets = acquire.getPartitionOffsets();
  auto partSizes = acquire.getPartitionSizes();
  if (!partOffsets.empty() && !partSizes.empty()) {
    /// The acquire has explicit partition offsets/sizes. This
    /// means it accesses a sub-region. Mark as narrowable
    /// even without exact halo bounds -- the downstream pass
    /// can compute the exact ESD from the partition info.
    markDepNarrowable(acquire, ptr, contractOp, edtDepPattern);
    ARTS_DEBUG("ET-3:   marked partition-hinted dependency as narrowable");
    return true;
  }

  return false;
}

/// Try to narrow a wavefront dependency. Returns true if the dependency was
/// marked as narrowable.
static bool tryNarrowWavefrontDep(DbAcquireOp acquire, Value ptr,
                                  LoweringContractOp contractOp,
                                  std::optional<ArtsDepPattern> edtDepPattern) {
  /// For wavefront_2d patterns, each EDT only depends on the diagonal
  /// predecessor, not the full previous row. Mark all read-only
  /// dependencies on the same array as narrowable since the wavefront
  /// cell only needs data from the immediately adjacent diagonal cell.
  markDepNarrowable(acquire, ptr, contractOp, edtDepPattern);
  ARTS_DEBUG("ET-3:   marked wavefront diagonal dependency as narrowable");
  return true;
}

///===----------------------------------------------------------------------===///
/// ET-3: Dep chain narrowing for stencil/wavefront patterns
///===----------------------------------------------------------------------===///
unsigned EdtTransformsPass::narrowDepChains() {
  /// narrowable_dep is consumed conservatively by acquire rewrite planning:
  /// read-only task acquires keep worker-local dependency windows instead of
  /// widening back to the parent range when upstream analysis proved that a
  /// narrower dependence is safe.
  ModuleOp module = getOperation();
  unsigned count = 0;

  module.walk([&](func::FuncOp func) {
    auto &edtGraph = AM->getEdtAnalysis().getOrCreateEdtGraph(func);
    if (edtGraph.size() == 0)
      return;

    func.walk([&](EdtOp edt) {
      EdtNode *node = edtGraph.getEdtNode(edt);
      if (!node)
        return;

      /// Determine if this EDT has a stencil-family depPattern.
      auto edtDepPattern = getEffectiveDepPattern(edt.getOperation());
      if (!edtDepPattern)
        return;

      if (!isStencilFamilyDepPattern(*edtDepPattern))
        return;

      bool isHaloPattern = isStencilHaloDepPattern(*edtDepPattern);
      bool isWavefront = (*edtDepPattern == ArtsDepPattern::wavefront_2d);

      ARTS_DEBUG("ET-3: processing EDT ["
                 << node->getHierId()
                 << "] depPattern=" << static_cast<int>(*edtDepPattern));

      /// Collect the root DbAllocOp for each writer (inout/out) acquire.
      /// These identify the EDT's "own" block; neighbor reads (in mode) on
      /// the same root allocation are halo reads.
      DenseSet<Operation *> ownAllocOps;
      for (Value dep : edt.getDependencies()) {
        auto acquire = dep.getDefiningOp<DbAcquireOp>();
        if (!acquire)
          continue;
        if (!DbUtils::isWriterMode(acquire.getMode()))
          continue;
        auto *rootOp = DbUtils::getUnderlyingDbAlloc(acquire.getSourcePtr());
        if (rootOp)
          ownAllocOps.insert(rootOp);
      }

      /// Walk each dependency acquire. For each read-only acquire that
      /// targets the same root allocation as a writer (i.e., a neighbor
      /// halo read), check whether the dependency can be narrowed.
      for (Value dep : edt.getDependencies()) {
        auto acquire = dep.getDefiningOp<DbAcquireOp>();
        if (!acquire)
          continue;

        /// Only consider read-only (in) acquires as candidates for
        /// narrowing. Writer acquires on a neighbor block would be
        /// unusual and should not be narrowed.
        if (acquire.getMode() != ArtsMode::in)
          continue;

        /// Check that this acquire's root allocation is one of the
        /// EDT's writer targets. If it is on a completely different
        /// DB array, it is not a halo dependency -- skip.
        auto *rootOp = DbUtils::getUnderlyingDbAlloc(acquire.getSourcePtr());
        if (!rootOp || !ownAllocOps.contains(rootOp))
          continue;

        /// This is a read on the same DB array that the EDT writes --
        /// it is a neighbor halo read.
        Value ptr = acquire.getPtr();
        auto existingContract = getLoweringContract(ptr);

        /// Skip if already marked narrowable.
        LoweringContractOp contractOp = getLoweringContractOp(ptr);
        if (contractOp && contractOp->hasAttr(
                              AttrNames::Operation::Contract::NarrowableDep)) {
          ARTS_DEBUG("ET-3:   acquire already marked narrowable, skipping");
          continue;
        }

        /// Try pattern-specific narrowing strategies.
        if (isHaloPattern &&
            tryNarrowHaloDep(acquire, ptr, contractOp, edtDepPattern,
                             existingContract)) {
          ++count;
          continue;
        }

        if (isWavefront &&
            tryNarrowWavefrontDep(acquire, ptr, contractOp, edtDepPattern)) {
          ++count;
          continue;
        }

        ARTS_DEBUG(
            "ET-3:   halo footprint not statically determinable, skipping");
      }
    });
  });

  return count;
}

///===----------------------------------------------------------------------===///
/// EXT-EDT-2: Dead dependency elimination
///===----------------------------------------------------------------------===///
unsigned EdtTransformsPass::eliminateDeadDependencies() {
  ModuleOp module = getOperation();
  unsigned totalEliminated = 0;

  module.walk([&](EdtOp edt) {
    Block &body = edt.getBody().front();
    ValueRange deps = edt.getDependencies();
    unsigned numArgs = body.getNumArguments();

    /// Dependencies and block arguments must be in sync.
    if (deps.size() != numArgs)
      return;

    /// Identify dead block arguments: those with zero uses, or whose
    /// only uses are DbReleaseOps.
    SmallVector<unsigned, 4> deadIndices;
    for (unsigned i = 0; i < numArgs; ++i) {
      BlockArgument arg = body.getArgument(i);
      DbAcquireOp acquire = deps[i].getDefiningOp<DbAcquireOp>();

      if (arg.use_empty()) {
        /// Zero uses -- clearly dead.
        deadIndices.push_back(i);
        ARTS_DEBUG("EXT-EDT-2: block arg " << i << " of EDT has zero uses");
        continue;
      }

      bool releaseOnlyUses = true;
      for (OpOperand &use : arg.getUses()) {
        if (!isa<DbReleaseOp>(use.getOwner())) {
          releaseOnlyUses = false;
          break;
        }
      }
      if (!releaseOnlyUses)
        continue;

      /// Release-only acquires that never feed a db_ref or any other task
      /// computation are stale dependencies. Keep the edge only when an
      /// upstream transform explicitly stamped it as semantically required.
      if (acquire && acquire.getPreserveDepEdge()) {
        ARTS_DEBUG("EXT-EDT-2: keeping release-only dep " << i
                                                          << " due to "
                                                             "preserveDepEdge");
        continue;
      }

      deadIndices.push_back(i);
      ARTS_DEBUG("EXT-EDT-2: block arg " << i
                                          << " is release-only and can be "
                                             "eliminated");
    }

    if (deadIndices.empty())
      return;

    /// Erase DbReleaseOps that use dead block arguments before removing
    /// the arguments (otherwise the uses would dangle).
    for (unsigned idx : deadIndices) {
      BlockArgument arg = body.getArgument(idx);
      SmallVector<Operation *, 4> releasesToErase;
      for (OpOperand &use : arg.getUses())
        if (isa<DbReleaseOp>(use.getOwner()))
          releasesToErase.push_back(use.getOwner());
      for (Operation *rel : releasesToErase)
        rel->erase();
    }

    /// Build the new dependency list, excluding dead indices.
    /// Remove block arguments in reverse order to avoid index invalidation.
    llvm::sort(deadIndices, std::greater<>());
    deadIndices.erase(std::unique(deadIndices.begin(), deadIndices.end()),
                      deadIndices.end());

    DenseSet<unsigned> deadIndexSet(deadIndices.begin(), deadIndices.end());
    SmallVector<Value> newDeps;
    for (unsigned i = 0; i < deps.size(); ++i)
      if (!deadIndexSet.contains(i))
        newDeps.push_back(deps[i]);

    /// Collect the acquiring ops whose results may become dead.
    SmallVector<DbAcquireOp, 4> acquireCandidates;
    for (unsigned idx : deadIndices) {
      Value dep = deps[idx];
      if (auto acquire = dep.getDefiningOp<DbAcquireOp>())
        acquireCandidates.push_back(acquire);
    }

    /// Erase block arguments in reverse index order.
    for (unsigned idx : deadIndices)
      body.eraseArgument(idx);

    edt.setDependencies(newDeps);

    /// Erase now-unused DbAcquireOps (only if both results are dead).
    for (DbAcquireOp acquire : acquireCandidates) {
      if (acquire.getGuid().use_empty() && acquire.getPtr().use_empty())
        acquire.erase();
    }

    totalEliminated += deadIndices.size();
    ARTS_DEBUG("EXT-EDT-2: eliminated " << deadIndices.size()
                                        << " dead deps from EDT");
  });

  return totalEliminated;
}

///===----------------------------------------------------------------------===///
/// Pass creation
///===----------------------------------------------------------------------===///
namespace mlir {
namespace arts {
std::unique_ptr<Pass> createEdtTransformsPass(mlir::arts::AnalysisManager *AM) {
  return std::make_unique<EdtTransformsPass>(AM);
}
} // namespace arts
} // namespace mlir
