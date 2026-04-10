// RUN: %carts-compile %S/../../../inputs/snapshots/specfem3d_stress_openmp_to_arts.mlir --pipeline db-partitioning --arts-config %S/../../../../examples/arts.cfg | %FileCheck %s

// Verify that read and write acquires both carry block partitioning through
// db-partitioning, preserving owner-local blocked acquires with stencil owner
// dims on both sides of the stress worker slices.
// CHECK-LABEL: func.func @main
// CHECK: arts.db_alloc[<in>, <heap>, <read>, <coarse>, <stencil>]
// CHECK: arts.db_acquire[<in>]
// CHECK-SAME: partitioning(<block>
// CHECK-SAME: distribution_pattern = #arts.distribution_pattern<stencil>
// CHECK-SAME: stencil_owner_dims = [2]
// CHECK: arts.db_acquire[<inout>]
// CHECK-SAME: partitioning(<block>
// CHECK-SAME: stencil_owner_dims = [2]
