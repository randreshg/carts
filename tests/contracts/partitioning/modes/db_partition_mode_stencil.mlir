// RUN: %carts-compile %S/../../../examples/jacobi/for/jacobi-for.mlir --O3 --arts-config %S/../../../examples/arts.cfg --stop-at concurrency-opt | %FileCheck %s

// CHECK-DAG: arts.db_alloc[{{.*}}<block>, <stencil>{{.*}}]
// CHECK-DAG: arts.db_acquire[<in>]
// CHECK-DAG: partitioning(<block>)
// CHECK-DAG: stencil_center_offset = 1 : i64
