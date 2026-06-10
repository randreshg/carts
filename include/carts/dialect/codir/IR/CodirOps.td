///==========================================================================///
/// File: CodirOps.td
/// This file defines Codelet IR operations.
///==========================================================================///

#ifndef CARTS_CODIR_OPS
#define CARTS_CODIR_OPS

include "carts/dialect/codir/IR/CodirAttrs.td"
include "mlir/IR/OpBase.td"
include "mlir/Interfaces/ControlFlowInterfaces.td"
include "mlir/Interfaces/InferTypeOpInterface.td"
include "mlir/Interfaces/SideEffectInterfaces.td"

def CartsCodir_CodeletOp
    : CartsCodir_Op<"codelet", [
        AttrSizedOperandSegments,
        IsolatedFromAbove,
        SingleBlock
      ]> {
  let summary = "isolated codelet boundary with explicit deps and params";
  let description = [{
    `codir.codelet` is the target codelet isolation surface. Dependencies and
    scalar parameters are explicit operands; dependency access modes are
    explicit metadata. The body is isolated from above so conversion cannot
    recover hidden captures later in ARTS or ARTS-RT.
  }];

  let arguments = (ins
    OptionalAttr<ArrayAttr>:$dep_modes,
    OptionalAttr<ArrayAttr>:$dep_storage_views,
    // One SDE arrayId per dependency. Non-negative entries join a dependency
    // to `array_layout`; -1 means no SDE layout fact was available.
    OptionalAttr<ArrayAttr>:$dep_array_ids,
    // One CODIR collective family per dependency, parallel to
    // `dep_storage_views`.
    OptionalAttr<ArrayAttr>:$dep_collectives,
    OptionalAttr<ArrayAttr>:$dep_owner_dims,
    OptionalAttr<UnitAttr>:$task_depend,
    OptionalAttr<UnitAttr>:$ordered_task_depend,
    OptionalAttr<UnitAttr>:$completion_barrier,
    OptionalAttr<PatternAttr>:$pattern,
    OptionalAttr<ReductionStrategyAttr>:$reduction_strategy,
    UnitAttr:$partial_reduction,
    OptionalAttr<ArrayAttr>:$partial_reduction_dims,
    OptionalAttr<ArrayAttr>:$partial_reduction_owner_dims,
    OptionalAttr<ArrayAttr>:$partial_reduction_dep_result_dim_maps,
    UnitAttr:$partial_reduction_split_required,
    OptionalAttr<ArrayAttr>:$partial_reduction_split_dims,
    OptionalAttr<I64Attr>:$partial_reduction_split_factor,
    OptionalAttr<I64Attr>:$partial_reduction_split_owner_task_count,
    OptionalAttr<I64Attr>:$partial_reduction_split_target_worker_count,
    OptionalAttr<DistributionKindAttr>:$distribution_kind,
    OptionalAttr<IterationTopologyAttr>:$iteration_topology,
    OptionalAttr<RepetitionStructureAttr>:$repetition_structure,
    OptionalAttr<ArrayAttr>:$plan_owner_dims,
    OptionalAttr<ArrayAttr>:$tile_owner_dims,
    OptionalAttr<ArrayAttr>:$tile_shape,
    OptionalAttr<ArrayAttr>:$logical_worker_slice,
    OptionalAttr<ArrayAttr>:$halo_shape,
    OptionalAttr<ArrayAttr>:$access_min_offsets,
    OptionalAttr<ArrayAttr>:$access_max_offsets,
    OptionalAttr<ArrayAttr>:$spatial_dims,
    OptionalAttr<ArrayAttr>:$write_footprint,
    UnitAttr:$in_place_safe,
    UnitAttr:$in_place_shared_state,
    // SDE layout facts forwarded in element space for CODIR storage and
    // movement assignment.
    OptionalAttr<ArrayAttr>:$array_layout,
    OptionalAttr<ArrayAttr>:$layouts_disagree,
    Variadic<AnyType>:$deps,
    Variadic<AnyType>:$params
  );
  let regions = (region SizedRegion<1>:$body);

  let assemblyFormat = [{
    (`deps` `(` $deps^ `:` type($deps) `)`)?
    (`params` `(` $params^ `:` type($params) `)`)?
    attr-dict-with-keyword $body
  }];
}

def CartsCodir_YieldOp
    : CartsCodir_Op<"yield", [
        Pure,
        ReturnLike,
        Terminator,
        HasParent<"CodeletOp">
      ]> {
  let summary = "terminator for CODIR codelet bodies";
  let arguments = (ins Variadic<AnyType>:$results);
  let assemblyFormat = "($results^ `:` type($results))? attr-dict";
}

def CartsCodir_AtomicAddOp
    : CartsCodir_Op<"atomic_add", [MemoryEffects<[MemWrite]>]> {
  let summary = "atomic add to a codelet dependency element";
  let description = [{
    CODIR representation of a codelet-local atomic add. SDE owns source
    reduction/atomic legality; CODIR carries the isolated codelet operation so
    CODIR-to-ARTS can translate it mechanically.
  }];

  let arguments = (ins AnyType:$addr, AnyType:$value);
  let assemblyFormat =
      "`(` $addr `,` $value `:` type($addr) `,` type($value) `)` attr-dict";
}

def CartsCodir_HaloExchangeOp
    : CartsCodir_Op<"halo_exchange", [Pure, SameOperandsAndResultType]> {
  let summary = "CODIR halo movement node over a block-storage dependency";
  let description = [{
    `codir.halo_exchange` makes halo movement explicit before CODIR-to-ARTS.
  }];

  let arguments = (ins AnyMemRef:$source);
  let results = (outs AnyMemRef:$result);
  let assemblyFormat =
      "$source attr-dict `:` type($source) `->` type($result)";
}

#endif // CARTS_CODIR_OPS
