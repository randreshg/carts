// RUN: sh -c '%S/../../../../tools/carts compile %S/../../../../external/carts-benchmarks/kastors-jacobi/poisson-for/poisson-for.c --stop-at concurrency-opt --arts-config %S/../../../examples/arts.cfg || true' | %FileCheck %s

// Verify that the poisson-for benchmark gets block partitioning with
// stencil center offset on the read acquire for the forcing array.
// CHECK-LABEL: func.func @main
// CHECK: arts.db_alloc[<inout>, <heap>, <write>, <block>, <uniform>]
// CHECK: arts.db_acquire[<in>]
// CHECK: partitioning(<block>, offsets[%{{.*}}],
// CHECK: stencil_center_offset = 1 : i64
