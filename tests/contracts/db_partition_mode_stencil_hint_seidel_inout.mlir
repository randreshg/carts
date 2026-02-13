// RUN: %carts-compile %S/../../external/carts-benchmarks/polybench/seidel-2d/seidel-2d.mlir --O3 --arts-config %S/../../docker/arts-docker-2node.cfg --stop-at concurrency-opt | %FileCheck %s

// CHECK: partitioning(<block>
// CHECK-NOT: partitioning(<stencil>)

