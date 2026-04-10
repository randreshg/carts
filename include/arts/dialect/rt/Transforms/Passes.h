///==========================================================================///
/// File: Passes.h
///
/// Pass declarations for the ARTS Runtime (arts_rt) dialect.
///==========================================================================///

#ifndef ARTS_DIALECT_RT_TRANSFORMS_PASSES_H
#define ARTS_DIALECT_RT_TRANSFORMS_PASSES_H

#include "mlir/Pass/Pass.h"

#define GEN_PASS_DECL
#include "arts/dialect/rt/Transforms/Passes.h.inc"

#define GEN_PASS_REGISTRATION
#include "arts/dialect/rt/Transforms/Passes.h.inc"

#endif // ARTS_DIALECT_RT_TRANSFORMS_PASSES_H
