// RUN: %carts-compile %S/../../../inputs/uniform_block.mlir --O3 --arts-config %S/../../../../examples/arts.cfg --pipeline db-partitioning | %FileCheck %s

// Verify that uniform direct writes produce coarse allocations with blocked
// acquires.
// CHECK: arts.db_alloc[{{.*}}<coarse>, <uniform>{{.*}}]
// CHECK: partitioning(<block>{{.*}})
