///==========================================================================///
/// File: MuAccessWindow.h
///
/// Recover the canonical per-CU MU access window from a rank-expanded
/// block-grid MU.
///
/// `queryAccessWindows` is the SINGLE shared scope+geometry gate used by both
/// the `raise-to-mu-access-window` transform and the
/// `verify-sde-mu-access-window` verifier, so the two never disagree on which
/// MUs are in scope. It is a pure query: it reads the committed layout shape
/// VERBATIM (it never recomputes owner dims or block shape) and returns an
/// empty vector — conservative, not an error — for anything outside the
/// supported elementwise/stencil, committed-owner-grid, fully-static path.
///==========================================================================///

#ifndef CARTS_DIALECT_SDE_UTILS_MUACCESSWINDOW_H
#define CARTS_DIALECT_SDE_UTILS_MUACCESSWINDOW_H

#include "carts/dialect/sde/IR/SdeDialect.h"
#include "mlir/IR/Value.h"
#include "llvm/ADT/SmallVector.h"
#include <optional>

namespace mlir::carts::sde {

/// The canonical per-CU access window for one rank-expanded MU, in block-grid
/// coordinates. All values are static.
struct RaisedWindowSpec {
  SdeCuRegionOp cu; ///< the CU this window describes
  mlir::Value mu;   ///< the rank-expanded mu_alloc result
  SdeAccessMode mode = SdeAccessMode::read; ///< read, write, or readwrite
  std::optional<int64_t> arrayId; ///< committed SDE array identity, if known
  int64_t ownerDimCount = 0;      ///< leading grid (owner) dims
  llvm::SmallVector<int64_t, 2> blockLo; ///< per-owner-dim block lo (==0)
  llvm::SmallVector<int64_t, 2>
      blockHi; ///< per-owner-dim block hi (grid count)
  llvm::SmallVector<int64_t, 4> validExtents; ///< per-logical-dim tile extent
};

/// Query the access windows for one `sde.mu_alloc`. One window is returned for
/// each CU/mode that accesses the MU. Same-CU read+write normally returns one
/// readwrite spec; committed halo reads are split into read and write specs so
/// ARTS never receives a writable halo dependency. Out-of-scope (all
/// conservative, no error): non-static/dynamic, no committed block-grid writer,
/// classification not elementwise/stencil, a structure that does not recover
/// the committed grain against the writer's iteration domain, SDE accumulator
/// reductions, an unsupported (non-load/store/dealloc/window) use of the MU
/// root, or an access outside any CU. Reduction-shaped output writers without
/// SDE reduction accumulators use the same direct block-window shape as
/// elementwise writers.
llvm::SmallVector<RaisedWindowSpec, 4> queryAccessWindows(SdeMuAllocOp mu);

/// Backward-compatible single-window query for callers that only accept the old
/// single-CU scope. Returns nullopt unless exactly one per-CU window is in
/// scope.
std::optional<RaisedWindowSpec> queryAccessWindow(SdeMuAllocOp mu);

} // namespace mlir::carts::sde

#endif // CARTS_DIALECT_SDE_UTILS_MUACCESSWINDOW_H
