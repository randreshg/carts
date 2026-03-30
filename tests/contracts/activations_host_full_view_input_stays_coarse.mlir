// RUN: sh -c 'CARTS_COMPILE_WORKDIR=%t.compile %S/../../tools/carts compile %S/../../external/carts-benchmarks/ml-kernels/activations/activations.c --pipeline concurrency-opt --arts-config %S/inputs/arts_2t.cfg -- -DNREPS=1 >/dev/null && cat %t.compile/activations.concurrency-opt.mlir' | %FileCheck %s

// A fragmented DB layout cannot preserve host-side whole-view helper calls
// like init_data(input, size). Keep the input allocation coarse and keep the
// helper call operating on a single contiguous DB view. The activation outputs
// are different: they are only consumed by worker EDTs and host checksum loops,
// so DbPartitioning should still keep them block-partitioned.

// CHECK: %[[IN_GUID:[^,]+]], %[[IN_PTR:[^ ]+]] = arts.db_alloc[<in>, <heap>, <read>, <coarse>
// CHECK: %[[IN_VIEW:[^ ]+]] = arts.db_ref %[[IN_PTR]][%{{[^]]+}}] : memref<?xmemref<?xf32>> -> memref<?xf32>
// CHECK-DAG: %[[OUT_GUID:[^,]+]], %[[OUT_PTR:[^ ]+]] = arts.db_alloc[<inout>, <heap>, <write>, <block>
// CHECK: call @init_data(%[[IN_VIEW]], %{{[^ )]+}}) : (memref<?xf32>, i32) -> ()
// CHECK: arts.db_acquire[<{{inout|out}}>] (%[[OUT_GUID]] : memref<?xi64>, %[[OUT_PTR]] : memref<?xmemref<?xf32>>) partitioning(<block>

module {
}
