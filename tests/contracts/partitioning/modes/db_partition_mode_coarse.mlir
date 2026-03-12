// RUN: %carts-compile %S/inputs/uniform_block.mlir --O3 --arts-config %S/../examples/arts.cfg --stop-at concurrency-opt | %FileCheck %s

// CHECK: arts.db_alloc[{{.*}}<coarse>, <indexed>{{.*}}]
// CHECK: partitioning(<coarse>{{.*}})
