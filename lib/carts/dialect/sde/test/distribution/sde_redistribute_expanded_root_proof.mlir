// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-redistribute)' 2>&1 | %FileCheck %s

// Redistribution may retile an expanded MU when alternating buffers must share
// a committed DB grain. Equal old block extents in two dimensions must not
// cause a one-dimension retile to rewrite the other dimension's div/rem math.

// CHECK-LABEL: func.func @retile_equal_block_extents_by_owner_dim
// CHECK: sde.array_layout_root write %[[DST:.*]] : memref<4x2x4x8xf32> array_id(1)
// CHECK: memref.load
// CHECK: %[[BJ:.*]] = arith.divui %arg1, %c8
// CHECK: %[[LJ:.*]] = arith.remui %arg1, %c8
// CHECK: %[[C4A:.*]] = arith.constant 4 : index
// CHECK: %[[BI:.*]] = arith.divui %{{.*}}, %[[C4A]]
// CHECK: %[[C4B:.*]] = arith.constant 4 : index
// CHECK: %[[LI:.*]] = arith.remui %{{.*}}, %[[C4B]]
// CHECK: memref.store %{{.*}}, %[[DST]][%[[BI]], %[[BJ]], %[[LI]], %[[LJ]]]
func.func @retile_equal_block_extents_by_owner_dim() {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c4 = arith.constant 4 : index
  %c8 = arith.constant 8 : index
  %c16 = arith.constant 16 : index
  %zero = arith.constant 0.0 : f32
  %src = sde.mu_alloc : memref<4x2x4x8xf32>
  %dst = sde.mu_alloc : memref<2x2x8x8xf32>
  sde.su_iterate (%c0, %c0) to (%c16, %c16) step (%c1, %c1) {
  ^bb0(%i: index, %j: index):
    sde.array_layout_root write %src : memref<4x2x4x8xf32> array_id(0)
    sde.cu_region <parallel> {
      %bi = arith.divui %i, %c4 : index
      %bj = arith.divui %j, %c8 : index
      %li = arith.remui %i, %c4 : index
      %lj = arith.remui %j, %c8 : index
      memref.store %zero, %src[%bi, %bj, %li, %lj]
          : memref<4x2x4x8xf32>
      sde.yield
    }
    sde.yield
  } {arrayLayout = [
    {arrayId = 0 : i64, blockShape = [4, 8], kind = "block_parallel",
     muBlockCount = 8 : i64, ownerDims = [0, 1], role = "write"}]}
  sde.su_iterate (%c0, %c0) to (%c16, %c16) step (%c1, %c1) {
  ^bb0(%i: index, %j: index):
    sde.array_layout_root read %src : memref<4x2x4x8xf32> array_id(0)
    sde.array_layout_root write %dst : memref<2x2x8x8xf32> array_id(1)
    sde.cu_region <parallel> {
      %rbi = arith.divui %i, %c4 : index
      %rbj = arith.divui %j, %c8 : index
      %rli = arith.remui %i, %c4 : index
      %rlj = arith.remui %j, %c8 : index
      %v = memref.load %src[%rbi, %rbj, %rli, %rlj]
          : memref<4x2x4x8xf32>
      %wbi = arith.divui %i, %c8 : index
      %wbj = arith.divui %j, %c8 : index
      %wli = arith.remui %i, %c8 : index
      %wlj = arith.remui %j, %c8 : index
      memref.store %v, %dst[%wbi, %wbj, %wli, %wlj]
          : memref<2x2x8x8xf32>
      sde.yield
    }
    sde.yield
  } {arrayLayout = [
    {arrayId = 0 : i64, blockShape = [4, 8], kind = "block_parallel",
     muBlockCount = 8 : i64, ownerDims = [0, 1], role = "read"},
    {arrayId = 1 : i64, blockShape = [8, 8], kind = "block_parallel",
     muBlockCount = 4 : i64, ownerDims = [0, 1], role = "write"}]}
  return
}

// CHECK-LABEL: func.func @missing_writer_fact_requires_store_proof
// CHECK: sde.array_layout_root write %{{.*}} array_id(7)
// CHECK-NOT: arrayLayout =
// CHECK: return
func.func @missing_writer_fact_requires_store_proof() {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c16 = arith.constant 16 : index
  %zero = arith.constant 0.0 : f32
  %A = sde.mu_alloc : memref<2x2x8x8xf32>
  sde.su_iterate (%c0, %c0) to (%c16, %c16) step (%c1, %c1) {
  ^bb0(%i: index, %j: index):
    sde.array_layout_root write %A : memref<2x2x8x8xf32> array_id(7)
    sde.cu_region <parallel> {
      memref.store %zero, %A[%c0, %c0, %i, %j] : memref<2x2x8x8xf32>
      sde.yield
    }
    sde.yield
  }
  return
}
