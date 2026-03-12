// RUN: %carts-compile %S/../../seidel-2d.mlir --O3 --arts-config %S/../../docker/arts-docker-2node.cfg --stop-at concurrency-opt | %FileCheck %s

// CHECK: distribution_pattern = #arts.distribution_pattern<stencil>
// CHECK: arts.db_acquire[<inout>] ({{.*}}) partitioning(<coarse>)
// CHECK-NOT: arts.db_acquire[<inout>] ({{.*}}) partitioning(<stencil>)
