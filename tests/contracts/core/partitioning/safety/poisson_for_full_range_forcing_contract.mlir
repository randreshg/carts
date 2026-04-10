// RUN: %carts-compile %S/../../../inputs/snapshots/poisson_for_openmp_to_arts.mlir --pipeline db-partitioning --arts-config %S/../../../../examples/arts.cfg | %FileCheck %s

// Verify that the poisson-for benchmark lowers forcing-array reads to
// stencil-partitioned halo slices with explicit element windows, while the
// owned writeback acquire remains block-partitioned under the same owner-dim
// contract without requiring an explicit element window on the writeback edge.
// CHECK-LABEL: func.func @main
// CHECK: arts.db_alloc[<inout>, <heap>, <write>, <block>, <stencil>]
// CHECK: arts.db_acquire[<in>] {{.*}} partitioning(<block>
// CHECK-SAME: element_offsets[
// CHECK-SAME: element_sizes[
// CHECK-SAME: stencil_owner_dims = [0]
// CHECK: arts.lowering_contract(%{{.*}} : memref<?xmemref<?x?xf64>>)
// CHECK-SAME: contract(<ownerDims = [0]
// CHECK: arts.db_acquire[<out>]
// CHECK-SAME: partitioning(<block>
// CHECK-SAME: stencil_owner_dims = [0]
// CHECK: arts.lowering_contract(%{{.*}} : memref<?xmemref<?x?xf64>>)
// CHECK-SAME: contract(<ownerDims = [0]>)
