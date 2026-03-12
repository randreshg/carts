// RUN: sh -c '%S/../../../../tools/carts compile %S/../../../../external/carts-benchmarks/kastors-jacobi/poisson-for/poisson-for.c --stop-at concurrency-opt --arts-config %S/../../../examples/arts.cfg || true' | %FileCheck %s

// CHECK-LABEL: func.func @main
// CHECK: %[[FG:.*]], %[[FP:.*]] = arts.db_alloc[<inout>, <heap>, <write>, <block>, <uniform>]
// CHECK: sizes[%[[FBLOCKS:.*]]] elementType(f64)
// CHECK: arts.db_acquire[<in>] (%[[FG]] : memref<?xi64>,
// CHECK: partitioning(<block>, offsets[%{{.*}}],
// CHECK: sizes[%{{.*}}]),
// CHECK: offsets[%c0], sizes[%[[FBLOCKS]]]
// CHECK: stencil_center_offset = 1 : i64
