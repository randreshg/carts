///==========================================================================///
/// File: SdeToArtsBoundaryDepAnalysis.h
/// SDE access-window and dependency analysis for ARTS realization.
///==========================================================================///

#ifndef CARTS_DIALECT_ARTS_TRANSFORMS_BOUNDARY_SDETOARTSBOUNDARYDEPANALYSIS_H
#define CARTS_DIALECT_ARTS_TRANSFORMS_BOUNDARY_SDETOARTSBOUNDARYDEPANALYSIS_H

#include "carts/dialect/arts/Transforms/boundary/SdeToArtsBoundaryTypes.h"
#include "carts/dialect/sde/IR/SdeDialect.h"
#include "mlir/Support/LogicalResult.h"
#include "llvm/ADT/DenseSet.h"

namespace mlir {
namespace carts::arts::boundary {

std::optional<CommittedPhysicalLayout>
readCommittedPhysicalLayout(sde::SdeSuIterateOp source,
                            ArrayRef<DirectDepSpec> deps = {});

LogicalResult collectSuDependencies(
    sde::SdeSuIterateOp source, SmallVectorImpl<DirectDepSpec> &deps,
    DenseSet<Operation *> &consumedCuLevelAccessWindows,
    SmallVectorImpl<Operation *> &consumedRedists);

LogicalResult collectStandaloneCuDependencies(
    sde::SdeCuRegionOp source, SmallVectorImpl<DirectCuDepSpec> &deps,
    SmallVectorImpl<arts::DbAccessWindowOp> &windows);

} // namespace carts::arts::boundary
} // namespace mlir

#endif
