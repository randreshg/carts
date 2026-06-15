// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-rank-expand-mu)' 2>&1 | %FileCheck %s

// Repeated accesses to the same block-local coordinate should share the
// rank-expanded block/tile index split instead of emitting div/rem per access.

// CHECK-LABEL: func.func @rank_expand_reuses_block_local_indices
// CHECK: %[[MU:.*]] = sde.mu_alloc : memref<4x16x16xf32>
// CHECK: scf.for
// CHECK: %[[BLOCK:.*]] = arith.divui %{{.*}}, %{{.*}} : index
// CHECK: %[[TILE:.*]] = arith.remui %{{.*}}, %{{.*}} : index
// CHECK: memref.store %{{.*}}, %[[MU]][%[[BLOCK]], %[[TILE]], %{{.*}}] : memref<4x16x16xf32>
// CHECK-NOT: arith.divui
// CHECK-NOT: arith.remui
// CHECK: memref.load %[[MU]][%[[BLOCK]], %[[TILE]], %{{.*}}] : memref<4x16x16xf32>
// CHECK-NOT: arith.divui
// CHECK-NOT: arith.remui
// CHECK: memref.store %{{.*}}, %[[MU]][%[[BLOCK]], %[[TILE]], %{{.*}}] : memref<4x16x16xf32>
// CHECK-NOT: arith.divui
// CHECK-NOT: arith.remui
// CHECK: memref.load %[[MU]][%[[BLOCK]], %[[TILE]], %{{.*}}] : memref<4x16x16xf32>
// CHECK-NOT: arith.divui
// CHECK-NOT: arith.remui
// CHECK: memref.store %{{.*}}, %[[MU]][%[[BLOCK]], %[[TILE]], %{{.*}}] : memref<4x16x16xf32>

func.func @rank_expand_reuses_block_local_indices() {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c16 = arith.constant 16 : index
  %c64 = arith.constant 64 : index
  %zero = arith.constant 0.0 : f32
  %one = arith.constant 1.0 : f32
  %out = sde.mu_alloc : memref<64x16xf32>
  sde.su_iterate (%c0) to (%c64) step (%c1) classification(<elementwise>) {
  ^bb0(%i: index):
    sde.array_layout_root write %out : memref<64x16xf32> array_id(0)
    sde.cu_region <parallel> {
      scf.for %j = %c0 to %c16 step %c1 {
        memref.store %zero, %out[%i, %j] : memref<64x16xf32>
        %old = memref.load %out[%i, %j] : memref<64x16xf32>
        %next = arith.addf %old, %one : f32
        memref.store %next, %out[%i, %j] : memref<64x16xf32>
        %old2 = memref.load %out[%i, %j] : memref<64x16xf32>
        %next2 = arith.addf %old2, %one : f32
        memref.store %next2, %out[%i, %j] : memref<64x16xf32>
      }
      sde.yield
    }
    sde.yield
  } {arrayLayout = [{arrayId = 0 : i64, blockShape = [16, 16], kind = "block_parallel", muBlockCount = 4 : i64, ownerDims = [0], role = "write"}]}
  return
}

// CHECK-LABEL: func.func @rank_expand_preserves_unrelated_index_divrem
// CHECK: %[[MU2:.*]] = sde.mu_alloc : memref<4x16x16xf32>
// CHECK: scf.for
// CHECK: %[[UNRELATED_DIV:.*]] = arith.divui %{{.*}}, %{{.*}} : index
// CHECK: %[[UNRELATED_REM:.*]] = arith.remui %{{.*}}, %{{.*}} : index
// CHECK: arith.addi %[[UNRELATED_DIV]], %[[UNRELATED_REM]] : index
// CHECK: %[[BLOCK2:.*]] = arith.divui %{{.*}}, %{{.*}} : index
// CHECK: %[[TILE2:.*]] = arith.remui %{{.*}}, %{{.*}} : index
// CHECK: memref.store %{{.*}}, %[[MU2]][%[[BLOCK2]], %[[TILE2]], %{{.*}}] : memref<4x16x16xf32>
// CHECK-NOT: arith.divui
// CHECK-NOT: arith.remui
// CHECK: memref.load %[[MU2]][%[[BLOCK2]], %[[TILE2]], %{{.*}}] : memref<4x16x16xf32>
// CHECK-NOT: arith.divui
// CHECK-NOT: arith.remui
// CHECK: memref.store %{{.*}}, %[[MU2]][%[[BLOCK2]], %[[TILE2]], %{{.*}}] : memref<4x16x16xf32>

func.func @rank_expand_preserves_unrelated_index_divrem() {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c16 = arith.constant 16 : index
  %c64 = arith.constant 64 : index
  %zero = arith.constant 0.0 : f32
  %one = arith.constant 1.0 : f32
  %out = sde.mu_alloc : memref<64x16xf32>
  sde.su_iterate (%c0) to (%c64) step (%c1) classification(<elementwise>) {
  ^bb0(%i: index):
    sde.array_layout_root write %out : memref<64x16xf32> array_id(1)
    sde.cu_region <parallel> {
      scf.for %j = %c0 to %c16 step %c1 {
        %unrelated_div = arith.divui %i, %c16 : index
        %unrelated_rem = arith.remui %i, %c16 : index
        %unrelated_sum = arith.addi %unrelated_div, %unrelated_rem : index
        memref.store %zero, %out[%i, %j] : memref<64x16xf32>
        %old = memref.load %out[%i, %j] : memref<64x16xf32>
        %next = arith.addf %old, %one : f32
        memref.store %next, %out[%i, %j] : memref<64x16xf32>
      }
      sde.yield
    }
    sde.yield
  } {arrayLayout = [{arrayId = 1 : i64, blockShape = [16, 16], kind = "block_parallel", muBlockCount = 4 : i64, ownerDims = [0], role = "write"}]}
  return
}

// CHECK-LABEL: func.func @rank_expand_preserves_casted_i64_divrem
// CHECK: %[[MU3:.*]] = sde.mu_alloc : memref<4x16x16xf32>
// CHECK: scf.for
// CHECK: %[[I64_DIV:.*]] = arith.divui %{{.*}}, %{{.*}} : i64
// CHECK: %[[I64_REM:.*]] = arith.remui %{{.*}}, %{{.*}} : i64
// CHECK: arith.addi %[[I64_DIV]], %[[I64_REM]] : i64
// CHECK: %[[BLOCK3:.*]] = arith.divui %{{.*}}, %{{.*}} : index
// CHECK: %[[TILE3:.*]] = arith.remui %{{.*}}, %{{.*}} : index
// CHECK: memref.store %{{.*}}, %[[MU3]][%[[BLOCK3]], %[[TILE3]], %{{.*}}] : memref<4x16x16xf32>
// CHECK-NOT: arith.divui
// CHECK-NOT: arith.remui
// CHECK: memref.load %[[MU3]][%[[BLOCK3]], %[[TILE3]], %{{.*}}] : memref<4x16x16xf32>
// CHECK-NOT: arith.divui
// CHECK-NOT: arith.remui
// CHECK: memref.store %{{.*}}, %[[MU3]][%[[BLOCK3]], %[[TILE3]], %{{.*}}] : memref<4x16x16xf32>

func.func @rank_expand_preserves_casted_i64_divrem() {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c16 = arith.constant 16 : index
  %c64 = arith.constant 64 : index
  %c16_i64 = arith.constant 16 : i64
  %zero = arith.constant 0.0 : f32
  %one = arith.constant 1.0 : f32
  %out = sde.mu_alloc : memref<64x16xf32>
  sde.su_iterate (%c0) to (%c64) step (%c1) classification(<elementwise>) {
  ^bb0(%i: index):
    sde.array_layout_root write %out : memref<64x16xf32> array_id(2)
    sde.cu_region <parallel> {
      scf.for %j = %c0 to %c16 step %c1 {
        %i_i64 = arith.index_cast %i : index to i64
        %i64_div = arith.divui %i_i64, %c16_i64 : i64
        %i64_rem = arith.remui %i_i64, %c16_i64 : i64
        %i64_sum = arith.addi %i64_div, %i64_rem : i64
        memref.store %zero, %out[%i, %j] : memref<64x16xf32>
        %old = memref.load %out[%i, %j] : memref<64x16xf32>
        %next = arith.addf %old, %one : f32
        memref.store %next, %out[%i, %j] : memref<64x16xf32>
      }
      sde.yield
    }
    sde.yield
  } {arrayLayout = [{arrayId = 2 : i64, blockShape = [16, 16], kind = "block_parallel", muBlockCount = 4 : i64, ownerDims = [0], role = "write"}]}
  return
}
