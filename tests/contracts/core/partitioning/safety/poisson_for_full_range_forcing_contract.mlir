// RUN: %carts-compile %S/../../../inputs/snapshots/poisson_for_openmp_to_arts.mlir --pipeline db-partitioning --arts-config %S/../../../../examples/arts.cfg | %FileCheck %s

// Verify that the poisson-for benchmark correctly partitions the stencil kernel
// loop body. The stencil loop gets stencil_tiling_nd pattern with block
// partitioning and owner-dim contracts.
// CHECK-LABEL: func.func @main
// CHECK: arts.db_alloc[<inout>, <heap>, <write>, <coarse>
// CHECK: arts.db_acquire[<out>]
// CHECK-SAME: partitioning(<block>
// CHECK-SAME: stencil_owner_dims
// CHECK: arts.lowering_contract
// CHECK-SAME: contract(<ownerDims
// CHECK: arts.db_acquire[<in>]
// CHECK-SAME: partitioning(<block>
// CHECK: arts.lowering_contract
// CHECK-SAME: contract(<ownerDims
// CHECK: arts.db_acquire[<out>]
// CHECK-SAME: partitioning(<block>
// CHECK: arts.lowering_contract
// CHECK-SAME: contract(<ownerDims
