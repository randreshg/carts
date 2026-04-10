// RUN: %carts-compile %S/../inputs/snapshots/activations_openmp_to_arts.mlir --pipeline db-partitioning --arts-config %S/../inputs/arts_2t.cfg | %FileCheck %s

// Activations initialization is now fully partition-aware: instead of
// preserving a coarse whole-view helper call, db-partitioning keeps the
// working arrays block-partitioned and worker EDTs materialize their local
// slot with `arts.db_ref ... [%c0]`.

// CHECK: arts.db_alloc[<inout>, <heap>, <write>, <block>, <indexed>]
// CHECK: arts.db_acquire[<out>] {{.*}}partitioning(<block>, offsets[
// CHECK: arts.db_ref %{{.+}}[%{{.+}}] : memref<?xmemref<?xf32>> -> memref<?xf32>
// CHECK-NOT: call @init_data(
