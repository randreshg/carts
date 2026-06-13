///==========================================================================///
/// File: SdeToArtsBoundaryPasses.h
///
/// SDE→ARTS boundary pass runners.
///
/// Three mechanical passes (no SDE policy):
///   1. sde-storage-to-arts-db — MU storage → DbAlloc; validate movement facts;
///      stamp transitional db_access_window carriers.
///   2. sde-accesses-to-arts-deps — realize movements; lower carriers by case:
///        movements → standalone CU → SU iterate → CU task.
///   3. finalize-sde-to-arts — control/resource cleanup; reject residual SDE.
///
/// Implementation is split across boundary/* translation units.
///==========================================================================///

#ifndef CARTS_DIALECT_ARTS_TRANSFORMS_BOUNDARY_SDETOARTSBOUNDARYPASSES_H
#define CARTS_DIALECT_ARTS_TRANSFORMS_BOUNDARY_SDETOARTSBOUNDARYPASSES_H

#include "mlir/IR/BuiltinOps.h"
#include "mlir/Support/LogicalResult.h"

namespace mlir {
namespace carts::arts::boundary {

LogicalResult runSdeStorageToArtsDb(ModuleOp module);
LogicalResult runSdeAccessesToArtsDeps(ModuleOp module);
LogicalResult runFinalizeSdeToArts(ModuleOp module);

} // namespace carts::arts::boundary
} // namespace mlir

#endif
