// RUN: %carts-compile %S/../../../inputs/snapshots/specfem3d_stress_openmp_to_arts.mlir --pipeline db-partitioning --arts-config %S/../../../../examples/arts.cfg | %FileCheck %s

// Verify that read and write acquires both carry block partitioning through
// db-partitioning, preserving owner-local blocked acquires with stencil owner
// dims on both sides of the stress worker slices.
// CHECK-LABEL: func.func @main
// CHECK: %[[READ_GUID0:.*]], %[[READ_PTR0:.*]] = arts.db_alloc[<in>, <heap>, <read>, <coarse>, <stencil>]
// CHECK: %[[DIRECT_GUID:.*]], %[[DIRECT_PTR:.*]] = arts.db_acquire[<in>] (%{{.*}} : memref<?xi64>, %{{.*}} : memref<?xmemref<?x?x?xf64>>) partitioning(<block>
// CHECK-SAME: distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>
// CHECK-SAME: stencil_owner_dims = [2]
// CHECK: arts.lowering_contract(%[[DIRECT_PTR]] : memref<?xmemref<?x?x?xf64>>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>
// CHECK: %[[WRITE_GUID:.*]], %[[WRITE_PTR:.*]] = arts.db_acquire[<inout>] (%{{.*}} : memref<?xi64>, %{{.*}} : memref<?xmemref<?x?x?xf64>>) partitioning(<block>, offsets[
// CHECK-SAME: stencil_owner_dims = [2]
// CHECK: arts.lowering_contract(%[[WRITE_PTR]] : memref<?xmemref<?x?x?xf64>>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>
