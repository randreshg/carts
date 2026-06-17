///==========================================================================///
/// File: SdeToArtsBoundaryDepAnalysis.h
/// SDE access-window and dependency analysis for ARTS realization.
///==========================================================================///

#ifndef CARTS_DIALECT_ARTS_TRANSFORMS_BOUNDARY_SDETOARTSBOUNDARYDEPANALYSIS_H
#define CARTS_DIALECT_ARTS_TRANSFORMS_BOUNDARY_SDETOARTSBOUNDARYDEPANALYSIS_H

#include "carts/dialect/arts/Transforms/boundary/SdeToArtsBoundaryTypes.h"
#include "carts/dialect/sde/IR/SdeDialect.h"
#include "mlir/Support/LogicalResult.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"

namespace mlir {
namespace carts::arts::boundary {

std::optional<CommittedPhysicalLayout>
readCommittedPhysicalLayout(sde::SdeSuIterateOp source,
                            ArrayRef<DirectDepSpec> deps = {});

bool hasDistributedLaunchStorageFacts(ArrayRef<DirectDepSpec> deps);
bool hasDistributedWriterStorageFacts(ArrayRef<DirectDepSpec> deps);
bool hasDistributedLaunchStorageFacts(ArrayRef<DirectCuDepSpec> deps);

std::optional<unsigned>
findDirectDepIndexForAccess(ArrayRef<DirectDepSpec> deps, arts::DbAllocOp alloc,
                            ArtsMode mode, bool preferHaloRead = false);

FailureOr<ArrayAttr>
buildPartialReductionDepResultDimMap(sde::SdeSuIterateOp source,
                                     const DirectDepSpec &dep);

FailureOr<unsigned> getAccessWindowPayloadDim(sde::SdeSuIterateOp source,
                                              const DirectDepSpec &dep,
                                              unsigned ownerSlot,
                                              unsigned dispatchPhysicalDim);

FailureOr<int64_t> getAccessWindowPayloadExtent(sde::SdeSuIterateOp source,
                                                const DirectDepSpec &dep,
                                                unsigned depPayloadDim);

int64_t ceilDivPositiveI64(int64_t lhs, int64_t rhs);

std::optional<SmallVector<int64_t, 4>>
findLargestOwnerLocalGroupCounts(ArrayRef<WriterGroupingSpec> writerSpecs,
                                 ArrayRef<int64_t> requestedCounts,
                                 int64_t totalNodes);

LogicalResult ensureDistributedWriterOwnerLocalGroups(
    sde::SdeSuIterateOp source, ArrayRef<DirectDepSpec> deps,
    SmallVectorImpl<int64_t> &groupBlockCounts,
    SmallVectorImpl<int64_t> &workerSpans, ArrayRef<int64_t> ownerBlockSizes,
    ArrayRef<int64_t> dispatchedOwnerDims,
    ArrayRef<unsigned> dispatchedLoopDims, int64_t totalNodes,
    bool &splitToOwnerLocalGroups);

LogicalResult
collectCoarseSuDependencies(sde::SdeSuIterateOp source,
                            SmallVectorImpl<CoarseSuDependency> &deps);

LogicalResult
collectSuDependencies(sde::SdeSuIterateOp source,
                      SmallVectorImpl<DirectDepSpec> &deps,
                      DenseSet<Operation *> &consumedCuLevelAccessWindows,
                      SmallVectorImpl<Operation *> &consumedRedists);

// Raw-access coverage gate (owned by SdeToArtsBoundaryRawAccessVerify): fails
// closed when a raw SU body access is not covered by a committed SDE
// access-window dependency. Invoked inline by collectSuDependencies.
LogicalResult verifyRawSuAccessesCoveredByDeps(
    sde::SdeSuIterateOp source,
    DenseMap<Operation *, SmallVector<unsigned, 2>> &depIndex,
    SmallVectorImpl<DirectDepSpec> &deps);

LogicalResult collectStandaloneCuDependencies(
    sde::SdeCuRegionOp source, SmallVectorImpl<DirectCuDepSpec> &deps,
    SmallVectorImpl<arts::DbAccessWindowOp> &windows);

} // namespace carts::arts::boundary
} // namespace mlir

#endif
