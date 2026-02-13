// RUN: %carts-compile %S/../../external/carts-benchmarks/polybench/jacobi2d/jacobi2d.mlir --O3 --arts-config %S/../../docker/arts-docker-2node.cfg --stop-at concurrency-opt | %FileCheck %s

// CHECK: distribution_pattern = #arts.distribution_pattern<stencil>
// CHECK: partitioning(<block>)
// CHECK: stencil_center_offset = 1 : i64
