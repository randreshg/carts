// RUN: sh -c 'CARTS_COMPILE_WORKDIR=%t.compile %carts compile %S/../../../examples/mixed_orientation/poisson_mixed_orientation.c --pipeline db-partitioning --arts-config %S/../../../examples/arts.cfg >/dev/null && cat %t.compile/poisson_mixed_orientation.db-partitioning.mlir' | %FileCheck %s

// CHECK-LABEL: func.func @main
// CHECK: %[[GUID2D:.*]], %[[PTR2D:.*]] = arts.db_alloc[<inout>, <heap>, <write>, <block>, <indexed>]
// CHECK-SAME: : (memref<?x?xi64>, memref<?x?xmemref<?x?xf64>>)
// CHECK: arts.lowering_contract(%[[PTR2D]] : memref<?x?xmemref<?x?xf64>>) block_shape[
// CHECK-SAME: contract(<ownerDims = [0, 1], postDbRefined = true>)
// CHECK: arts.db_acquire[<inout>] (%[[GUID2D]] : memref<?x?xi64>, %[[PTR2D]] : memref<?x?xmemref<?x?xf64>>) partitioning(<block>, offsets[
// CHECK-SAME: offsets[
// CHECK-SAME: -> (memref<?xi64>, memref<?x?xmemref<?x?xf64>>)
