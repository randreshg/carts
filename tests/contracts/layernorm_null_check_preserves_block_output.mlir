// RUN: sh -c 'CARTS_COMPILE_WORKDIR=%t.compile %S/../../tools/carts compile %S/../../external/carts-benchmarks/ml-kernels/layernorm/layernorm.c --pipeline concurrency-opt --arts-config %S/inputs/arts_2t.cfg -- -DNREPS=1 >/dev/null && cat %t.compile/layernorm.concurrency-opt.mlir' | %FileCheck %s

// The layernorm benchmark guards its DB-backed buffers with `if (!x || !gamma
// || !beta)`. Those checks lower through polygeist.memref2pointer, but they do
// not require a contiguous whole-view layout. Keep the main `x` tensor block-
// partitioned and preserve the null-check pointer cast instead of forcing the
// whole allocation back to coarse.

// CHECK: %[[X_GUID:[^,]+]], %[[X_PTR:[^ ]+]] = arts.db_alloc[<inout>, <heap>, <write>, <block>
// CHECK: %[[X_VIEW:[^ ]+]] = arts.db_ref %[[X_PTR]][%c0] : memref<?xmemref<?x?xf32>> -> memref<?x?xf32>
// CHECK: %[[X_RAW:[^ ]+]] = polygeist.memref2pointer %[[X_VIEW]] : memref<?x?xf32> to !llvm.ptr
// CHECK: arts.db_acquire[<inout>] (%[[X_GUID]] : memref<?xi64>, %[[X_PTR]] : memref<?xmemref<?x?xf32>>) partitioning(<block>

module {
}
