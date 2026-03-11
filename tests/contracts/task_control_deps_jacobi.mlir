// RUN: %carts-compile %S/../examples/jacobi/deps/jacobi.mlir --O3 --arts-config %S/../../docker/arts-docker-2node.cfg --stop-at concurrency-opt | %FileCheck %s

// CHECK-COUNT-6: arts.preserve_dependency_mode
// CHECK-NOT: partitioning(<stencil>)
// CHECK-NOT: stencil_center_offset
