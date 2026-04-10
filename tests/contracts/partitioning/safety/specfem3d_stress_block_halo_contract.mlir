// RUN: sh -c 'CARTS_COMPILE_WORKDIR=%t.compile %carts compile %S/../../../../external/carts-benchmarks/specfem3d/stress/stress_update.c --pipeline db-partitioning --arts-config %S/../../../examples/arts.cfg >/dev/null && cat %t.compile/stress_update.db-partitioning.mlir' | %FileCheck %s

// Verify the fallback keeps the read side coarse through db-partitioning while
// preserving owner-local blocked acquires on the write side of the stress
// worker slices.
// CHECK-LABEL: func.func @main
// CHECK: %[[READ_GUID0:.*]], %[[READ_PTR0:.*]] = arts.db_alloc[<in>, <heap>, <read>, <coarse>, <stencil>]
// CHECK: %[[DIRECT_GUID:.*]], %[[DIRECT_PTR:.*]] = arts.db_acquire[<in>] (%{{.*}} : memref<?xi64>, %{{.*}} : memref<?xmemref<?x?x?xf64>>) partitioning(<coarse>)
// CHECK-SAME: distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>
// CHECK-SAME: stencil_owner_dims = [2]
// CHECK: arts.lowering_contract(%[[DIRECT_PTR]] : memref<?xmemref<?x?x?xf64>>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>
// CHECK: %[[WRITE_GUID:.*]], %[[WRITE_PTR:.*]] = arts.db_acquire[<inout>] (%{{.*}} : memref<?xi64>, %{{.*}} : memref<?xmemref<?x?x?xf64>>) partitioning(<block>, offsets[
// CHECK-SAME: stencil_owner_dims = [2]
// CHECK: arts.lowering_contract(%[[WRITE_PTR]] : memref<?xmemref<?x?x?xf64>>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>
