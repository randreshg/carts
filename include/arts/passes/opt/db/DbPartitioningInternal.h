///==========================================================================///
/// File: DbPartitioningInternal.h
///
/// Local implementation contract for DbPartitioning. This header is
/// intentionally private to the db-partitioning implementation split and
/// should not be used as shared compiler infrastructure.
///==========================================================================///

#ifndef ARTS_PASSES_OPT_DB_DBPARTITIONINGINTERNAL_H
#define ARTS_PASSES_OPT_DB_DBPARTITIONINGINTERNAL_H

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
#include "arts/Dialect.h"
#include "arts/analysis/AnalysisDependencies.h"
#include "arts/analysis/AnalysisManager.h"
#include "arts/analysis/db/DbAnalysis.h"
#include "arts/analysis/graphs/db/DbGraph.h"
#include "arts/analysis/heuristics/DbHeuristics.h"
#include "arts/analysis/loop/LoopAnalysis.h"
#include "arts/analysis/loop/LoopNode.h"
#include "arts/analysis/value/ValueAnalysis.h"
#include "arts/passes/Passes.h"
#include "mlir/Pass/Pass.h"
/// Other
#include "mlir/IR/OpDefinition.h"
#include "mlir/IR/Operation.h"
#include "mlir/IR/Visitors.h"
#include "mlir/Support/LogicalResult.h"
/// Debug
#include "arts/analysis/graphs/db/DbNode.h"
#include "arts/analysis/graphs/edt/EdtGraph.h"
#include "arts/analysis/graphs/edt/EdtNode.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Support/raw_ostream.h"
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <optional>

#include "arts/analysis/db/OwnershipProof.h"
#include "arts/transforms/db/DbBlockPlanResolver.h"
#include "arts/transforms/db/DbPartitionPlanner.h"
#include "arts/transforms/db/DbPartitionTypes.h"
#include "arts/transforms/db/DbRewriter.h"
#include "arts/transforms/db/PartitionStrategy.h"
#include "arts/utils/DbUtils.h"
#include "arts/utils/Debug.h"
#include "arts/utils/EdtUtils.h"
#include "arts/utils/LoweringContractUtils.h"
#include "arts/utils/OperationAttributes.h"
#include "arts/utils/PatternSemantics.h"
#include "arts/utils/StencilAttributes.h"
#include "arts/utils/Utils.h"

namespace mlir::arts::db_partitioning {

///===----------------------------------------------------------------------===//
/// Structs
///===----------------------------------------------------------------------===//

struct AcquireModeReconcileResult {
  bool modesConsistent = true;
  bool allBlockFullRange = false;
};

struct EntryMatch {
  size_t index = 0;
  bool found = false;
  int score = -1;
};

///===----------------------------------------------------------------------===//
/// Tracing
///===----------------------------------------------------------------------===//

bool dbPartitionTraceEnabled();
void dbPartitionTraceImpl(llvm::function_ref<void(llvm::raw_ostream &)> fn);

///===----------------------------------------------------------------------===//
/// Helper Functions
///===----------------------------------------------------------------------===//

bool markAcquireNeedsFullRange(AcquirePartitionInfo &info);

bool isLeadingPrefixPartitionPlan(ArrayRef<unsigned> partitionedDims,
                                  size_t numPartitionedDims,
                                  size_t elementRank);

bool blockSizeCoversFullExtent(Value blockSize, Value extent);

std::optional<std::pair<StencilBounds, unsigned>>
getSingleOwnerContractStencilBounds(
    const DbAnalysis::AcquireContractSummary *summary);

void collapseTrailingFullExtentPartitionDims(
    DbAllocOp allocOp, SmallVectorImpl<Value> &blockSizes,
    SmallVectorImpl<unsigned> &partitionedDims);

std::optional<int64_t> computeStaticElementCount(DbAllocOp allocOp);

bool validateElementWiseIndices(ArrayRef<AcquirePartitionInfo> acquireInfos,
                                ArrayRef<DbAcquireNode *> allocAcquireNodes);

SmallVector<Value> getPartitionOffsetsND(DbAcquireNode *acqNode,
                                         const AcquirePartitionInfo *info);

bool hasInternodeAcquireUser(ArrayRef<DbAcquireNode *> acquireNodes);

AcquirePatternSummary
summarizeAcquirePatterns(ArrayRef<DbAcquireNode *> acquireNodes,
                         DbAllocNode *allocNode, DbAnalysis *dbAnalysis);

bool isAcquireInfoConsistent(const AcquirePartitionInfo &lhs,
                             const AcquirePartitionInfo &rhs);

AcquireModeReconcileResult
reconcileAcquireModes(PartitioningContext &ctx,
                      SmallVectorImpl<AcquirePartitionInfo> &acquireInfos,
                      bool canUseBlock);

bool isLowerBoundGuaranteedByControlFlow(Operation *op, Value loopIV);

void applyBoundsValid(DbAcquireOp acquireOp, ArrayRef<int64_t> boundsCheckFlags,
                      Value loopIV);

void lowerStencilAcquireBounds(ModuleOp module, LoopAnalysis &loopAnalysis);

AcquirePartitionInfo
computeAcquirePartitionInfo(DbAnalysis &dbAnalysis, DbAcquireOp acquire,
                            const DbAnalysis::AcquireContractSummary *summary,
                            OpBuilder &builder);

unsigned deriveAuthoritativeOwnerBlockRank(
    DbAllocOp allocOp, ArrayRef<AcquirePartitionInfo> acquireInfos,
    ArrayRef<std::optional<DbAnalysis::AcquireContractSummary>>
        acquireContractSummaries,
    ArrayRef<AcquireInfo> heuristicInfos);

void resetCoarseAcquireRanges(DbAllocOp allocOp, DbAllocNode *allocNode,
                              OpBuilder &builder);

bool feedbackPartitionDecisionToContract(DbAllocOp newAllocOp,
                                         ArrayRef<Value> blockSizes,
                                         ArrayRef<unsigned> partitionedDims,
                                         PartitionMode mode,
                                         OpBuilder &builder);

///===----------------------------------------------------------------------===//
/// Multi-entry Acquire Expansion Helpers
///===----------------------------------------------------------------------===//

SmallVector<DbAcquireOp> createExpandedAcquires(DbAcquireOp original,
                                                OpBuilder &builder);

SmallVector<Value> rebuildEdtDeps(EdtOp edt, DbAcquireOp original,
                                  ArrayRef<DbAcquireOp> expanded);

SmallVector<BlockArgument> insertExpandedBlockArgs(EdtOp edt,
                                                   BlockArgument originalArg,
                                                   size_t numEntries,
                                                   Type argType, Location loc);

SmallVector<DbRefOp> collectDbRefUsers(BlockArgument arg);

SmallVector<Value> collectAccessIndices(DbRefOp dbRef);

SmallVector<Value> collectAccessIndicesFromUser(Operation *refUser);

SmallVector<Value> getEntryAnchors(DbAcquireOp original, size_t entry);

int indexMatchStrength(Value idx, Value anchor);

int scoreEntry(ArrayRef<Value> indices, ArrayRef<Value> anchors);

EntryMatch matchDbRefEntry(DbRefOp dbRef, ArrayRef<Value> accessIndices,
                           DbAcquireOp original, size_t numEntries);

} // namespace mlir::arts::db_partitioning

#endif // ARTS_PASSES_OPT_DB_DBPARTITIONINGINTERNAL_H
