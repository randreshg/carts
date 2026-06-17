// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-redistribute)' 2>&1 | %FileCheck %s

// A pure multi-output init writer with same-rank but different owner-dim
// positions is split structurally before ARTS sees the writer facts.

// CHECK-LABEL: func.func @same_rank_mixed_owner_init_split
// CHECK: sde.array_layout_root write %[[A:.*]] : memref<2x8x16xf32> array_id(0)
// CHECK: arrayLayout = [{arrayId = 0 : i64
// CHECK-SAME: ownerDims = [0]
// CHECK-SAME: role = "write"
// CHECK: sde.array_layout_root write %[[B:.*]] : memref<2x16x8xf32> array_id(1)
// CHECK: arrayLayout = [{arrayId = 1 : i64
// CHECK-SAME: ownerDims = [1]
// CHECK-SAME: role = "write"

func.func @same_rank_mixed_owner_init_split() {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c8 = arith.constant 8 : index
  %c16 = arith.constant 16 : index
  %zero = arith.constant 0.0 : f32
  %A = sde.mu_alloc : memref<2x8x16xf32>
  %B = sde.mu_alloc : memref<2x16x8xf32>
  %C = sde.mu_alloc : memref<2x8x16xf32>

  sde.su_distribute <blocked> {
    sde.su_iterate (%c0, %c0) to (%c16, %c16) step (%c1, %c1) {
    ^bb0(%i: index, %j: index):
      sde.cu_region <parallel> {
        %aBlock = arith.divui %i, %c8 : index
        %aLocal = arith.remui %i, %c8 : index
        %bBlock = arith.divui %j, %c8 : index
        %bLocal = arith.remui %j, %c8 : index
        memref.store %zero, %A[%aBlock, %aLocal, %j] : memref<2x8x16xf32>
        memref.store %zero, %B[%bBlock, %i, %bLocal] : memref<2x16x8xf32>
        sde.yield
      }
      sde.yield
    }
  }

  sde.su_iterate (%c0, %c0) to (%c16, %c16) step (%c1, %c1)
      classification(<elementwise>) {
  ^bb0(%i: index, %j: index):
    sde.array_layout_root read %A : memref<2x8x16xf32> array_id(0)
    sde.array_layout_root read %B : memref<2x16x8xf32> array_id(1)
    sde.array_layout_root write %C : memref<2x8x16xf32> array_id(2)
    sde.cu_region <parallel> {
      %aBlock = arith.divui %i, %c8 : index
      %aLocal = arith.remui %i, %c8 : index
      %bBlock = arith.divui %j, %c8 : index
      %bLocal = arith.remui %j, %c8 : index
      %a = memref.load %A[%aBlock, %aLocal, %j] : memref<2x8x16xf32>
      %b = memref.load %B[%bBlock, %i, %bLocal] : memref<2x16x8xf32>
      %sum = arith.addf %a, %b : f32
      memref.store %sum, %C[%aBlock, %aLocal, %j] : memref<2x8x16xf32>
      sde.yield
    }
    sde.yield
  } {arrayLayout = [
    {arrayId = 0 : i64, blockShape = [8, 16],
     kind = "block_parallel", muBlockCount = 2 : i64,
     ownerDims = [0], role = "read"},
    {arrayId = 1 : i64, blockShape = [16, 8],
     kind = "block_parallel", muBlockCount = 2 : i64,
     ownerDims = [1], role = "read"},
    {arrayId = 2 : i64, blockShape = [8, 16],
     kind = "block_parallel", muBlockCount = 2 : i64,
     ownerDims = [0], role = "write"}]}
  return
}
