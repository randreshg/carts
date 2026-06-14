///==========================================================================///
/// File: MuAccessWindow.h
///
/// Recover the canonical per-CU MU access window from a rank-expanded
/// block-grid MU.
///
/// `queryAccessWindows` is the single scope+geometry gate for SDE boundary
/// lowering and barrier sync analysis. It reads the committed layout shape
/// VERBATIM and returns an empty vector — conservative, not an error — for
/// anything outside the supported elementwise/stencil, committed-owner-grid,
/// fully-static path.
///==========================================================================///

#ifndef CARTS_DIALECT_SDE_UTILS_MUACCESSWINDOW_H
#define CARTS_DIALECT_SDE_UTILS_MUACCESSWINDOW_H

#include "carts/dialect/sde/IR/SdeDialect.h"
#include "mlir/IR/Value.h"
#include "llvm/ADT/SmallVector.h"
#include <optional>

namespace mlir::carts::sde {

/// Block-grid coordinates derived from a rank-expanded `mu_alloc` memref type.
/// `validExtents` are the trailing tile dims (== committed blockExtent on owner
/// dims), never halo-inflated.
struct MuAccessWindowGeometry {
  int64_t ownerDimCount = 0;
  llvm::SmallVector<int64_t, 2> blockLo;
  llvm::SmallVector<int64_t, 2> blockHi;
  llvm::SmallVector<int64_t, 4> validExtents;
};

/// Derive access-window geometry from the committed rank-expanded MU type.
/// Returns whole-object geometry (`ownerDimCount == 0`) when the MU is not a
/// recognized block-grid expansion.
std::optional<MuAccessWindowGeometry> deriveMuAccessWindowGeometry(Value mu);

/// The canonical per-CU access window for one rank-expanded MU.
struct RaisedWindowSpec {
  SdeCuRegionOp cu; ///< the CU this window describes
  mlir::Value mu;   ///< the rank-expanded mu_alloc result
  SdeAccessMode mode = SdeAccessMode::read; ///< read, write, or readwrite
  std::optional<int64_t> arrayId; ///< committed SDE array identity, if known
};

/// Query the access windows for one `sde.mu_alloc`. One window is returned for
/// each CU/mode that accesses the MU. Same-CU read+write normally returns one
/// readwrite spec; committed halo reads are split into read and write specs so
/// ARTS never receives a writable halo dependency. Out-of-scope (all
/// conservative, no error): non-static/dynamic, no committed block-grid writer,
/// classification not elementwise/stencil, a structure that does not recover
/// the committed grain against the writer's iteration domain, SDE accumulator
/// reductions, an unsupported (non-load/store/dealloc) use of the MU root, or
/// SDE reduction accumulators use the same direct block-window shape as
/// elementwise writers.
llvm::SmallVector<RaisedWindowSpec, 4> queryAccessWindows(SdeMuAllocOp mu);

/// Backward-compatible single-window query for callers that only accept the old
/// single-CU scope. Returns nullopt unless exactly one per-CU window is in
/// scope.
std::optional<RaisedWindowSpec> queryAccessWindow(SdeMuAllocOp mu);

} // namespace mlir::carts::sde

#endif // CARTS_DIALECT_SDE_UTILS_MUACCESSWINDOW_H
