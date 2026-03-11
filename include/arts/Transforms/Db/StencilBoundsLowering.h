///==========================================================================///
/// File: StencilBoundsLowering.h
///
/// Runtime stencil-bounds guard lowering for DbAcquireOps.
///==========================================================================///

#ifndef ARTS_TRANSFORMS_DATABLOCK_STENCILBOUNDSLOWERING_H
#define ARTS_TRANSFORMS_DATABLOCK_STENCILBOUNDSLOWERING_H

#include "arts/Analysis/Loop/LoopAnalysis.h"
#include "mlir/IR/BuiltinOps.h"

namespace mlir {
namespace arts {

/// Analyze stencil-style index expressions and materialize bounds_valid guards
/// on DbAcquireOps when needed.
void lowerStencilAcquireBounds(ModuleOp module, LoopAnalysis &loopAnalysis);

} // namespace arts
} // namespace mlir

#endif // ARTS_TRANSFORMS_DATABLOCK_STENCILBOUNDSLOWERING_H
