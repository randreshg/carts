// RUN: %carts-compile %S/../examples/jacobi/for/jacobi-for.mlir --O3 --arts-config %S/../examples/arts.cfg --stop-at concurrency-opt | %FileCheck %s

// CHECK: arts.db_alloc[{{.*}}<stencil>{{.*}}]
// CHECK: partitioning(<block>)
// CHECK: stencil_center_offset = 1 : i64
