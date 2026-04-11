///==========================================================================///
/// File: Passes.h
///
/// Pass declarations for the SDE (Structured Decomposition Environment)
/// dialect.
///
/// This header is self-contained: it includes all dialect headers required
/// by dependentDialects in SDE Passes.td so that .h.inc can be safely
/// included without manual per-file dependency management.
///==========================================================================///

#ifndef ARTS_DIALECT_SDE_TRANSFORMS_PASSES_H
#define ARTS_DIALECT_SDE_TRANSFORMS_PASSES_H

#include "arts/Dialect.h"
#include "arts/dialect/sde/IR/SdeDialect.h"
#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/Linalg/IR/Linalg.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/OpenMP/OpenMPDialect.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/Pass/Pass.h"

namespace mlir::arts {

#define GEN_PASS_DECL
#include "arts/dialect/sde/Transforms/Passes.h.inc"

} // namespace mlir::arts

#endif // ARTS_DIALECT_SDE_TRANSFORMS_PASSES_H
