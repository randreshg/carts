//===----------------------------------------------------------------------===//
// File: CommonAttrs.td
// Shared TableGen fragments for attributes that are defined identically across
// more than one CARTS dialect. This file declares NO `def`/`EnumAttr` (so each
// dialect's -gen-enum-* invocation emits no symbols from it and there are no
// duplicate definitions); it only exposes shared `defvar` case lists that each
// dialect references from its own enum wrapper.
//===----------------------------------------------------------------------===//

#ifndef CARTS_COMMON_ATTRS
#define CARTS_COMMON_ATTRS

include "mlir/IR/EnumAttr.td"

// IterationTopology is defined with identical cases (owner_strip / owner_tile /
// owner_tile_2d) in the SDE, CODIR, and ARTS dialects. Each dialect keeps its
// own enum class (via its own dialect wrapper) but shares this case list so the
// case set stays in lock-step across the SDE->CODIR->ARTS boundary.
defvar CartsIterationTopologyCases = [
    I32EnumAttrCase<"owner_strip", 0, "owner_strip">,
    I32EnumAttrCase<"owner_tile", 1, "owner_tile">,
    I32EnumAttrCase<"owner_tile_2d", 2, "owner_tile_2d">
];

// ReductionStrategy is defined with identical cases (atomic / tree /
// local_accumulate) in the SDE, CODIR, and ARTS dialects.
defvar CartsReductionStrategyCases = [
    I32EnumAttrCase<"atomic", 0, "atomic">,
    I32EnumAttrCase<"tree", 1, "tree">,
    I32EnumAttrCase<"local_accumulate", 2, "local_accumulate">
];

// BarrierReason is defined with identical cases in the SDE and ARTS dialects.
defvar CartsBarrierReasonCases = [
    I32EnumAttrCase<"redundant", 0, "redundant">,
    I32EnumAttrCase<"required_memory", 1, "required_memory">,
    I32EnumAttrCase<"timestep_stage_boundary", 2, "timestep_stage_boundary">,
    I32EnumAttrCase<"wavefront_frontier", 3, "wavefront_frontier">,
    I32EnumAttrCase<"unknown_required", 4, "unknown_required">
];

#endif // CARTS_COMMON_ATTRS
