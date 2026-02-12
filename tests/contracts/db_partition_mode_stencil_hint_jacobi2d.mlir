// RUN: %carts-run %S/../../external/carts-benchmarks/polybench/jacobi2d/jacobi2d.mlir --O3 --arts-config %S/../../docker/arts-docker-2node.cfg --stop-at concurrency-opt | %FileCheck %s

// CHECK: partitioning(<stencil>)

