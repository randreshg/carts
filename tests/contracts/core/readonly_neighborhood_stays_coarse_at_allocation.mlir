// RUN: %carts-compile %S/../inputs/snapshots/conv2d_openmp_to_arts.mlir --pipeline db-partitioning --arts-config %S/../inputs/arts_64t.cfg | %FileCheck %s

// For single-node convolution-style neighborhood kernels, the read-only input
// stays coarse at allocation time while worker lowering still uses
// block-scoped read acquires for the owner strip. The output is also
// coarse-allocated with block-partitioned acquires.
// CHECK: arts.db_alloc[<in>, <heap>, <read>, <coarse>, <stencil>]
// CHECK-NOT: arts.db_acquire[<in>] {{.*}} arts.partition_hint = {blockSize =
// CHECK: arts.db_alloc[<inout>, <heap>, <write>, <coarse>, <uniform>]
// CHECK: arts.db_acquire[<in>] {{.*}} partitioning(<block>
// CHECK: arts.db_acquire[<inout>] {{.*}} partitioning(<block>
