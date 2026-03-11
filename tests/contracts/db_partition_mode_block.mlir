// RUN: %carts-compile %S/../examples/jacobi/for/jacobi-for.mlir --O3 --arts-config %S/../examples/arts.cfg --stop-at concurrency-opt | %FileCheck %s

// CHECK: partitioning(<block>)
