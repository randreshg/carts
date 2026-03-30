// RUN: sh -c 'CARTS_COMPILE_WORKDIR=%t.compile %S/../../tools/carts compile %S/../../external/carts-benchmarks/ml-kernels/activations/activations.c --pipeline concurrency-opt --arts-config %S/inputs/arts_1t.cfg -- -DNREPS=1 >/dev/null && cat %t.compile/activations.concurrency-opt.mlir' | %FileCheck %s

// Verify that the late DbModeTightening pass does not degrade write-capable
// activation outputs back to read-only acquires after concurrency-opt rewrites
// the accesses through arts.db_ref.

// CHECK: %[[RELU_GUID:[^,]+]], %[[RELU_PTR:[^ ]+]] = arts.db_alloc[<inout>, <heap>, <write>
// CHECK: %[[GELU_GUID:[^,]+]], %[[GELU_PTR:[^ ]+]] = arts.db_alloc[<inout>, <heap>, <write>
// CHECK: %[[SIGMOID_GUID:[^,]+]], %[[SIGMOID_PTR:[^ ]+]] = arts.db_alloc[<inout>, <heap>, <write>
// CHECK: %[[TANH_GUID:[^,]+]], %[[TANH_PTR:[^ ]+]] = arts.db_alloc[<inout>, <heap>, <write>

// CHECK: arts.db_acquire[<out>] (%[[RELU_GUID]] : memref<?xi64>, %[[RELU_PTR]] : memref<?xmemref<?xf32>>)
// CHECK: arts.db_acquire[<out>] (%[[GELU_GUID]] : memref<?xi64>, %[[GELU_PTR]] : memref<?xmemref<?xf32>>)
// CHECK: arts.db_acquire[<out>] (%[[SIGMOID_GUID]] : memref<?xi64>, %[[SIGMOID_PTR]] : memref<?xmemref<?xf32>>)
// CHECK: arts.db_acquire[<out>] (%[[TANH_GUID]] : memref<?xi64>, %[[TANH_PTR]] : memref<?xmemref<?xf32>>)

module {
}
