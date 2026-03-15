// RUN: %carts-compile %S/../../inputs/uniform_block.mlir --O3 --arts-config %S/../../../examples/arts.cfg --stop-at concurrency-opt | %FileCheck %s

// Verify that this input remains coarse/indexed in partition mode inference.
// CHECK: arts.db_alloc[{{.*}}<coarse>, <indexed>{{.*}}]
// CHECK: partitioning(<coarse>{{.*}})
