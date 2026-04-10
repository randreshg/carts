// RUN: sh -c 'CARTS_COMPILE_WORKDIR=%t.compile %carts compile %S/../../../external/carts-benchmarks/ml-kernels/activations/activations.c --pipeline post-db-refinement --arts-config %S/../inputs/arts_1t.cfg -- -DNREPS=1 >/dev/null && cat %t.compile/activations.post-db-refinement.mlir' | %FileCheck %s

// Verify that the late DbModeTightening pass does not degrade write-capable
// activation outputs back to read-only acquires after post-db-refinement rewrites
// the accesses through arts.db_ref. Under the single-worker contract these
// outputs should stay coarse, but they still must tighten to `<out>`.

// CHECK: %[[RELU_GUID:[^,]+]], %[[RELU_PTR:[^ ]+]] = arts.db_alloc[<inout>, <heap>, <write>, <coarse>
// CHECK: %[[GELU_GUID:[^,]+]], %[[GELU_PTR:[^ ]+]] = arts.db_alloc[<inout>, <heap>, <write>, <coarse>
// CHECK: %[[SIGMOID_GUID:[^,]+]], %[[SIGMOID_PTR:[^ ]+]] = arts.db_alloc[<inout>, <heap>, <write>, <coarse>
// CHECK: %[[TANH_GUID:[^,]+]], %[[TANH_PTR:[^ ]+]] = arts.db_alloc[<inout>, <heap>, <write>, <coarse>

// CHECK: arts.db_acquire[<out>] (%[[RELU_GUID]] : memref<?xi64>, %[[RELU_PTR]] : memref<?xmemref<?xf32>>) partitioning(<coarse>)
// CHECK: arts.db_acquire[<out>] (%[[GELU_GUID]] : memref<?xi64>, %[[GELU_PTR]] : memref<?xmemref<?xf32>>) partitioning(<coarse>)
// CHECK: arts.db_acquire[<out>] (%[[SIGMOID_GUID]] : memref<?xi64>, %[[SIGMOID_PTR]] : memref<?xmemref<?xf32>>) partitioning(<coarse>)
// CHECK: arts.db_acquire[<out>] (%[[TANH_GUID]] : memref<?xi64>, %[[TANH_PTR]] : memref<?xmemref<?xf32>>) partitioning(<coarse>)

module {
}
