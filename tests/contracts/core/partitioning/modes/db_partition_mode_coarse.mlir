// RUN: %carts-compile %S/../../../inputs/uniform_block.mlir --O3 --arts-config %S/../../../../examples/arts.cfg --pipeline db-partitioning | %FileCheck %s

// Verify that uniform direct writes still lower to blocked acquires even when
// the allocation itself is classified as indexed.
// CHECK: arts.db_alloc[{{.*}}<block>, <indexed>{{.*}}]
// CHECK: partitioning(<block>{{.*}})
