// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-memory-unit-realization)' 2>&1 | %FileCheck %s

// Dynamic extents computed inside a producer CU cannot be hoisted above that CU.
// SDE still owns the storage conversion: realize an MU at the original
// memref allocation site and yield it through the CU result.

// CHECK-LABEL: func.func @dynamic_cu_result_root_realizes_inside_producer
// CHECK: %[[A:.*]] = sde.cu_region <single> -> (memref<?xf32>) {
// CHECK:   %[[MU:.*]] = sde.mu_alloc(%{{.*}}) {{.*}}arrayId = 0 : i64{{.*}} : memref<?xf32>
// CHECK:   sde.yield %[[MU]] : memref<?xf32>
// CHECK: sde.array_layout_root write %[[A]] : memref<?xf32> array_id(0)
// CHECK-NOT: failed to realize SDE memory unit

func.func @dynamic_cu_result_root_realizes_inside_producer(%n: index) {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c16 = arith.constant 16 : index
  %one = arith.constant 1.0 : f32
  %A = sde.cu_region <single> -> (memref<?xf32>) {
    %bytes = arith.muli %n, %c16 : index
    %extent = arith.divui %bytes, %c16 : index
    %alloc = memref.alloc(%extent) : memref<?xf32>
    sde.yield %alloc : memref<?xf32>
  }
  sde.su_iterate (%c0) to (%n) step (%c1) classification(<elementwise>) {
  ^bb0(%i: index):
    sde.array_layout_root write %A : memref<?xf32> array_id(0)
    sde.cu_region <parallel> {
      memref.store %one, %A[%i] : memref<?xf32>
      sde.yield
    }
  } {arrayLayout = [{arrayId = 0 : i64, blockShape = [16], commVolumeBytes = 0 : i64, kind = "block_parallel", muBlockCount = 1 : i64, ownerDims = [0], role = "write"}], physicalOwnerDims = [0], physicalBlockShape = [16]}
  return
}
