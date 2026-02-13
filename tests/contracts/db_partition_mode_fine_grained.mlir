// RUN: %carts-compile %S/../examples/matrixmul/matrixmul.mlir --O3 --arts-config %S/../examples/arts.cfg --stop-at concurrency-opt | %FileCheck %s

// CHECK: partitioning(<fine_grained>, indices[
