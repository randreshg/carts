// RUN: sh -c 'CARTS_COMPILE_WORKDIR=%t.compile %carts compile %S/../../external/carts-benchmarks/ml-kernels/activations/activations.c --pipeline db-partitioning --arts-config %S/inputs/arts_2t.cfg -- -DNREPS=1 >/dev/null && cat %t.compile/activations.db-partitioning.mlir' | %FileCheck %s

// Activations initialization is now fully partition-aware: instead of
// preserving a coarse whole-view helper call, db-partitioning keeps the
// working arrays block-partitioned and worker EDTs materialize their local
// slot with `arts.db_ref ... [%c0]`.

// CHECK: arts.db_alloc[<inout>, <heap>, <write>, <block>, <uniform>]
// CHECK: arts.db_acquire[<out>] {{.*}}partitioning(<block>, offsets[
// CHECK: arts.db_ref %{{.+}}[%{{.+}}] : memref<?xmemref<?xf32>> -> memref<?xf32>
// CHECK-NOT: call @init_data(

module {
}
