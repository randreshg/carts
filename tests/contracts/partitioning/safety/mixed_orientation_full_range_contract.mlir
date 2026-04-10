// RUN: sh -c 'CARTS_COMPILE_WORKDIR=%t.compile %carts compile %S/../../../examples/mixed_orientation/poisson_mixed_orientation.c --pipeline db-partitioning --arts-config %S/../../../examples/arts.cfg >/dev/null && cat %t.compile/poisson_mixed_orientation.db-partitioning.mlir' | %FileCheck %s

// CHECK-LABEL: func.func @main
// Mixed-orientation kernels now keep owner-dimension refinement on the
// individual acquires instead of encoding it as an alloc-wide 2-D contract.
// CHECK: %[[GUID0:.*]], %[[PTR0:.*]] = arts.db_alloc[<inout>, <heap>, <write>, <block>, <indexed>]
// CHECK-SAME: : (memref<?xi64>, memref<?xmemref<?x?xf64>>)
// CHECK: arts.lowering_contract(%[[PTR0]] : memref<?xmemref<?x?xf64>>) block_shape[
// CHECK-SAME: contract(<ownerDims = [0], postDbRefined = true>)
// CHECK: arts.db_acquire[<inout>] ({{.*}}) partitioning(<block>, offsets[
// CHECK-SAME: stencil_owner_dims = [1]
// CHECK: arts.lowering_contract(%{{.*}} : memref<?xmemref<?x?xf64>>)
// CHECK-SAME: contract(<ownerDims = [1], postDbRefined = true>)
// CHECK: arts.db_acquire[<inout>] (%[[GUID0]] : memref<?xi64>, %[[PTR0]] : memref<?xmemref<?x?xf64>>) partitioning(<block>, offsets[
// CHECK-SAME: stencil_owner_dims = [0]
// CHECK: arts.lowering_contract(%{{.*}} : memref<?xmemref<?x?xf64>>)
// CHECK-SAME: contract(<ownerDims = [0]>)
