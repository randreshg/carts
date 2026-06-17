// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-redistribute)' 2>&1 | %FileCheck %s

// A single-output initializer with no reads can still be the real producer of a
// later committed DB grid. SDE may author its write fact only when the stores
// prove the same rank-expanded owner dimension and block shape.

// CHECK-LABEL: func.func @rank_expanded_column_init_writer
// CHECK: sde.array_layout_root write %[[A:.*]] : memref<4x16x4xf32> array_id(2)
// CHECK: arrayLayout = [{arrayId = 2 : i64
// CHECK-SAME: blockShape = [16, 4]
// CHECK-SAME: kind = "block_parallel"
// CHECK-SAME: muBlockCount = 4 : i64
// CHECK-SAME: ownerDims = [1]
// CHECK-SAME: role = "write"
// CHECK: sde.array_layout_root read %[[A]] : memref<4x16x4xf32> array_id(2)
func.func @rank_expanded_column_init_writer() {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c4 = arith.constant 4 : index
  %c16 = arith.constant 16 : index
  %zero = arith.constant 0.0 : f32
  %A = sde.mu_alloc : memref<4x16x4xf32>
  %B = sde.mu_alloc : memref<4x16x4xf32>
  sde.su_iterate (%c0, %c0) to (%c16, %c16) step (%c1, %c1) {
  ^bb0(%i: index, %j: index):
    sde.cu_region <parallel> {
      %bj = arith.divui %j, %c4 : index
      %lj = arith.remui %j, %c4 : index
      memref.store %zero, %A[%bj, %i, %lj] : memref<4x16x4xf32>
      sde.yield
    }
    sde.yield
  }
  sde.su_iterate (%c0, %c0) to (%c16, %c16) step (%c1, %c1) {
  ^bb0(%i: index, %j: index):
    sde.array_layout_root read %A : memref<4x16x4xf32> array_id(2)
    sde.array_layout_root write %B : memref<4x16x4xf32> array_id(3)
    sde.cu_region <parallel> {
      %bj = arith.divui %j, %c4 : index
      %lj = arith.remui %j, %c4 : index
      %v = memref.load %A[%bj, %i, %lj] : memref<4x16x4xf32>
      memref.store %v, %B[%bj, %i, %lj] : memref<4x16x4xf32>
      sde.yield
    }
    sde.yield
  } {arrayLayout = [
    {arrayId = 2 : i64, blockShape = [16, 4], kind = "block_parallel",
     muBlockCount = 4 : i64, ownerDims = [1], role = "read"},
    {arrayId = 3 : i64, blockShape = [16, 4], kind = "block_parallel",
     muBlockCount = 4 : i64, ownerDims = [1], role = "write"}]}
  return
}
