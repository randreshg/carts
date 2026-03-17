// RUN: sh -c 'CARTS_COMPILE_WORKDIR=%t.compile %S/../../../../tools/carts compile %S/../../../examples/mixed_orientation/poisson_mixed_orientation.c --pipeline concurrency-opt --arts-config %S/../../../examples/arts.cfg >/dev/null && cat %t.compile/poisson_mixed_orientation.concurrency-opt.mlir' | %FileCheck %s

// CHECK-LABEL: func.func @main
// CHECK: %[[FG:.*]], %[[FP:.*]] = arts.db_alloc[<inout>, <heap>, <write>, <block>, <uniform>]
// CHECK: sizes[%[[FBLOCKS:.*]]] elementType(f64)
// CHECK: arts.db_acquire[<in>] (%[[FG]] : memref<?xi64>,
// CHECK: partitioning(<block>, offsets[%{{.*}}], sizes[%{{.*}}]),
// CHECK: offsets[%c0], sizes[%[[FBLOCKS]]] -> (memref<?xi64>, memref<?xmemref<?x?xf64>>)
