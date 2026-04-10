///==========================================================================///
/// File: Passes.h
///
/// Pass declarations for the SDE (Structured Decomposition Environment) dialect.
///==========================================================================///

#ifndef ARTS_DIALECT_SDE_TRANSFORMS_PASSES_H
#define ARTS_DIALECT_SDE_TRANSFORMS_PASSES_H

#include "mlir/Pass/Pass.h"

#define GEN_PASS_DECL
#include "arts/dialect/sde/Transforms/Passes.h.inc"

#define GEN_PASS_REGISTRATION
#include "arts/dialect/sde/Transforms/Passes.h.inc"

#endif // ARTS_DIALECT_SDE_TRANSFORMS_PASSES_H
