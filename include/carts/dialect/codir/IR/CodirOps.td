///==========================================================================///
/// File: CodirOps.td
/// This file defines Codelet IR operations.
///==========================================================================///

#ifndef CARTS_CODIR_OPS
#define CARTS_CODIR_OPS

include "carts/dialect/codir/IR/CodirAttrs.td"
include "mlir/IR/OpBase.td"
include "mlir/Interfaces/ControlFlowInterfaces.td"
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
    // First-class CODIR collective family per dep (ADR-0003 §7b), one
    // `CollectiveKindAttr` per dep, parallel to `dep_storage_views`. Stamped
    // by `chooseCollective` during StoragePlanning; read by ConvertCodirToArts
    // to dispatch the realization. ADDITIVE: ARTS lowering is unchanged for
    // `none` deps, so kernels with no redistribution stay byte-identical.
    OptionalAttr<ArrayAttr>:$dep_collectives,
    // WF-3 opt-in: realize a `reduce_scatter`/`allreduce` dep with the
    // block-native per-block summing settle (emitPerBlockSummingSettle) instead
    // of the legacy coarse cross-node gather. ADDITIVE: absent on every kernel
    // today (chooseCollective never sets it), so the coarse path and the 18
    // oracle baselines are unchanged; the keystone fires only where explicitly
    // opted in. The block-native settle becomes the default once the per-tile
    // partial producers are materialized for atax/bicg/3mm.
    OptionalAttr<UnitAttr>:$emit_block_native_settle,
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
    OptionalAttr<AsyncStrategyAttr>:$async_strategy,
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
    // --- SDE module-scoped layout-assignment carriers (WF-5a). Threaded
    // verbatim from the source `sde.su_iterate` so the SDE layout engine's
    // per-array BLOCK decision survives into CODIR instead of being dropped at
    // the SDE -> CODIR boundary. These are pure element-space DATA-LAYOUT facts
    // (no runtime nodes/routes/collective names); see SdeOps.td $arrayLayout,
    // $layoutsDisagree, $commVolumeBytes for the exact schema. ADDITIVE:
    // codir-to-arts lowering ignores them unless a consumer opts in, so IR for
    // kernels whose greedy plan already matches the SDE choice is byte-identical.
    OptionalAttr<ArrayAttr>:$array_layout,
    OptionalAttr<ArrayAttr>:$layouts_disagree,
    OptionalAttr<I64Attr>:$comm_volume_bytes,
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

#endif // CARTS_CODIR_OPS
