// RUN: sh -c 'CARTS_COMPILE_WORKDIR=%t.compile %carts compile %S/../../../../external/carts-benchmarks/specfem3d/stress/stress_update.c --pipeline db-partitioning --arts-config %S/../../../examples/arts.cfg >/dev/null && cat %t.compile/stress_update.db-partitioning.mlir' | %FileCheck %s

// Verify the block fallback keeps the owner slice for inout acquires while
// widening the read-only stencil acquire window by the halo width.
// CHECK-LABEL: func.func @main
// CHECK: %[[READ_GUID0:.*]], %[[READ_PTR0:.*]] = arts.db_alloc[<in>, <heap>, <read>, <block>, <stencil>]
// CHECK: arts.lowering_contract(%[[READ_PTR0]] : memref<?xmemref<?x?x?xf64>>) block_shape[
// CHECK-SAME: contract(<ownerDims = [2], postDbRefined = true>)
// CHECK: %[[DIRECT_GUID:.*]], %[[DIRECT_PTR:.*]] = arts.db_acquire[<in>] (%{{.*}} : memref<?xi64>, %{{.*}} : memref<?xmemref<?x?x?xf64>>) partitioning(<block>, offsets[
// CHECK-SAME: distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<stencil>
// CHECK-SAME: stencil_owner_dims = [2]
// CHECK: arts.lowering_contract(%[[DIRECT_PTR]] : memref<?xmemref<?x?x?xf64>>) pattern(<depPattern = <stencil_tiling_nd>, distributionKind = <block>, distributionPattern = <stencil>
// CHECK: %[[WRITE_GUID:.*]], %[[WRITE_PTR:.*]] = arts.db_acquire[<inout>] (%{{.*}} : memref<?xi64>, %{{.*}} : memref<?xmemref<?x?x?xf64>>) partitioning(<block>, offsets[
// CHECK-SAME: stencil_owner_dims = [2]
// CHECK: arts.lowering_contract(%[[WRITE_PTR]] : memref<?xmemref<?x?x?xf64>>) pattern(<depPattern = <stencil_tiling_nd>, distributionKind = <block>, distributionPattern = <stencil>
