// RUN: %carts-compile %S/../../../../examples/matrixmul/matrixmul.mlir --O3 --arts-config %S/../../../../examples/arts.cfg --pipeline db-partitioning | %FileCheck %s

// CHECK: partitioning(<fine_grained>, indices[
