///==========================================================================///
/// File: MuAccessWindow.h
///
/// Recover the canonical per-CU MU access window from a rank-expanded
/// block-grid MU.
///
/// `planAccessWindows` is the SINGLE shared scope+geometry gate used by both
/// the `raise-to-mu-access-window` transform and the
/// `verify-sde-mu-access-window` verifier, so the two never disagree on which
/// MUs are in scope. It is a pure query: it reads the committed plan VERBATIM
/// (it never recomputes owner dims or block shape) and returns an empty vector
/// — conservative, not an error — for anything outside the supported
/// elementwise/stencil, single-contiguous-owner, fully-static path.
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
struct RaisedWindowPlan {
  SdeCuRegionOp cu; ///< the CU this window describes
  mlir::Value mu;   ///< the rank-expanded mu_alloc result
  SdeAccessMode mode = SdeAccessMode::read; ///< read or write (never readwrite)
  int64_t ownerDimCount = 0;                ///< leading grid (owner) dims (==1)
  llvm::SmallVector<int64_t, 2> blockLo;    ///< per-owner-dim block lo (==0)
  llvm::SmallVector<int64_t, 2>
      blockHi; ///< per-owner-dim block hi (grid count)
  llvm::SmallVector<int64_t, 4> validExtents; ///< per-logical-dim tile extent
};

/// Plan the access windows for one `sde.mu_alloc`. One window is returned for
/// each CU that accesses the MU in a single mode. Out-of-scope (all
/// conservative, no error): non-static/dynamic, no committed block-plan writer,
/// classification not elementwise/stencil, multi-owner, a structure that does
/// not recover the committed grain against the writer's iteration domain, an
/// unsupported (non-load/store/dealloc/window) use of the MU root, an access
/// outside any CU, or an in-place (read+write) access in that CU.
llvm::SmallVector<RaisedWindowPlan, 4> planAccessWindows(SdeMuAllocOp mu);

/// Backward-compatible single-window query for callers that only accept the old
/// single-CU scope. Returns nullopt unless exactly one per-CU window is in
/// scope.
std::optional<RaisedWindowPlan> planAccessWindow(SdeMuAllocOp mu);

} // namespace mlir::carts::sde

#endif // CARTS_DIALECT_SDE_UTILS_MUACCESSWINDOW_H
