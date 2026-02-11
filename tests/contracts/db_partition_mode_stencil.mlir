// RUN: %carts-run %S/../examples/jacobi/for/jacobi-for.mlir --O3 --arts-config %S/../examples/arts.cfg --stop-at concurrency-opt | %FileCheck %s

// CHECK: arts.db_alloc[{{.*}}<stencil>{{.*}}]
// CHECK: partitioning(<stencil>)
