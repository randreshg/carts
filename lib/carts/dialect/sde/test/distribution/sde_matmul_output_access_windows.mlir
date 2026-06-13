// RUN: %carts-compile %s --pass-pipeline='builtin.module(loop-interchange,sde-rank-expand-mu,verify-sde-mu-layout,raise-to-mu-access-window,verify-sde-mu-access-window)' 2>&1 | %FileCheck %s

// CHECK-LABEL: func.func @matmul_output_access_windows
// CHECK: %[[OUT:.*]] = sde.mu_alloc : memref<4x4x32x32xf32>
// CHECK: sde.su_iterate
// CHECK-SAME: classification(<matmul>)
// CHECK: sde.mu_access_window readwrite %[[OUT]] : memref<4x4x32x32xf32> array_id(0)
// CHECK: memref.store {{.*}}, %[[OUT]][
// CHECK: arrayLayout = [{arrayId = 0 : i64, blockShape = [32, 32]
// CHECK-SAME: muBlockCount = 16 : i64
// CHECK-SAME: ownerDims = [0, 1]
// CHECK: sde.su_iterate
// CHECK-SAME: classification(<elementwise>)
// CHECK: sde.mu_access_window read %[[OUT]] : memref<4x4x32x32xf32> array_id(0)
// CHECK: memref.load %[[OUT]][

func.func @matmul_output_access_windows() {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c32 = arith.constant 32 : index
  %c128 = arith.constant 128 : index
  %zero = arith.constant 0.0 : f32
  %one = arith.constant 1.0 : f32
  %out = sde.mu_alloc : memref<128x128xf32>

  sde.su_iterate (%c0, %c0) to (%c128, %c128) step (%c1, %c1) classification(<matmul>) {
  ^bb0(%i: index, %j: index):
    sde.array_layout_root write %out : memref<128x128xf32> array_id(0)
    sde.cu_region <parallel> {
      memref.store %zero, %out[%i, %j] : memref<128x128xf32>
      %old = memref.load %out[%i, %j] : memref<128x128xf32>
      %next = arith.addf %old, %one : f32
      memref.store %next, %out[%i, %j] : memref<128x128xf32>
      sde.yield
    }
    sde.yield
  } {arrayLayout = [{arrayId = 0 : i64, blockShape = [32, 32], budgetBlockShape = [32, 128], commVolumeBytes = 0 : i64, kind = "block_parallel", muBlockCount = 16 : i64, ownerDims = [0, 1], role = "write"}]}

  sde.su_iterate (%c0, %c0) to (%c128, %c128) step (%c32, %c32) classification(<elementwise>) {
  ^bb0(%i: index, %j: index):
    sde.array_layout_root read %out : memref<128x128xf32> array_id(0)
    sde.cu_region <single> {
      %v = memref.load %out[%i, %j] : memref<128x128xf32>
      %sink = arith.addf %v, %one : f32
      sde.yield
    }
    sde.yield
  } {arrayLayout = [{arrayId = 0 : i64, blockShape = [32, 32], commVolumeBytes = 0 : i64, kind = "block_parallel", muBlockCount = 16 : i64, ownerDims = [0, 1], role = "read"}]}
  return
}

// CHECK-LABEL: func.func @symmetric_self_gram_mirror_access_windows
// CHECK: %[[OUT:.*]] = sde.mu_alloc : memref<4x4xf32>
// CHECK-NOT: sde.mu_access_window
// CHECK: arrayLayout = [{arrayId = 1 : i64, blockShape = [4, 4]
// CHECK-SAME: ownerDims = [0]
func.func @symmetric_self_gram_mirror_access_windows() {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c4 = arith.constant 4 : index
  %zero = arith.constant 0.0 : f32
  %one = arith.constant 1.0 : f32
  %out = sde.mu_alloc : memref<4x4xf32>

  sde.su_iterate (%c0) to (%c4) step (%c1) classification(<matmul>) {
  ^bb0(%i: index):
    sde.array_layout_root write %out : memref<4x4xf32> array_id(1)
    sde.cu_region <parallel> {
      memref.store %one, %out[%i, %i] : memref<4x4xf32>
      %start = arith.addi %i, %c1 : index
      scf.for %j = %start to %c4 step %c1 {
        %sum = scf.for %k = %c0 to %c4 step %c1 iter_args(%acc = %zero) -> (f32) {
          %next = arith.addf %acc, %one : f32
          scf.yield %next : f32
        }
        memref.store %sum, %out[%i, %j] : memref<4x4xf32>
        memref.store %sum, %out[%j, %i] : memref<4x4xf32>
      }
      sde.yield
    }
    sde.yield
  } {arrayLayout = [{arrayId = 1 : i64, blockShape = [4, 4], commVolumeBytes = 0 : i64, kind = "block_parallel", muBlockCount = 1 : i64, ownerDims = [0], role = "write"}]}
  return
}
