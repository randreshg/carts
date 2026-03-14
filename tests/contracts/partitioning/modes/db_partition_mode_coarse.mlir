// RUN: %carts-compile %S/../../inputs/uniform_block.mlir --O3 --arts-config %S/../../../examples/arts.cfg --stop-at concurrency-opt | %FileCheck %s

// Verify that an elementwise uniform access pattern gets block partitioning.
// CHECK: arts.db_alloc[{{.*}}<block>, <uniform>{{.*}}]
// CHECK: partitioning(<block>{{.*}})
