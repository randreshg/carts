// RUN: %carts-compile %S/../../inputs/uniform_block.mlir --O3 --arts-config %S/../../../examples/arts.cfg --stop-at concurrency-opt | %FileCheck %s

// Verify that uniform direct writes are inferred as block/uniform.
// CHECK: arts.db_alloc[{{.*}}<block>, <uniform>{{.*}}]
// CHECK: partitioning(<block>{{.*}})
