// RUN: sh -c 'CARTS_COMPILE_WORKDIR=%t.compile %S/../../tools/carts compile %S/../../external/carts-benchmarks/ml-kernels/activations/activations.c --pipeline concurrency-opt --arts-config %S/inputs/arts_2t.cfg -- -DNREPS=1 >/dev/null && cat %t.compile/activations.concurrency-opt.mlir' | %FileCheck %s

// Activations initialization is now fully partition-aware: instead of
// preserving a coarse whole-view helper call, concurrency-opt keeps the
// working arrays block-partitioned and worker EDTs materialize their local
// slot with `arts.db_ref ... [%c0]`.

// CHECK: arts.db_alloc[<inout>, <heap>, <write>, <block>, <uniform>]
// CHECK: arts.db_acquire[<out>] {{.*}}partitioning(<block>, offsets[
// CHECK: arts.db_ref %{{[^ ]+}}[%c0] : memref<?xmemref<?xf32>> -> memref<?xf32>
// CHECK-NOT: call @init_data(

module {
}
