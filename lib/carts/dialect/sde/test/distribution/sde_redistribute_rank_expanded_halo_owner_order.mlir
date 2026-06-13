// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-redistribute)' 2>&1 | %FileCheck %s

// CHECK-LABEL: func.func @expanded_halo_permuted_owner_order
// CHECK: sde.su_distribute <owner_compute>
// CHECK: sde.su_halo %{{.*}} : memref<4x4x55x72x72x7xf64> array_id(3) owner [0, 1, 2] block [1, 1, 1, 72, 72, 7] halo [1, 1, 1, 0, 0, 0]
// CHECK-NOT: layoutsDisagree

func.func @expanded_halo_permuted_owner_order(%U: memref<4x4x55x72x72x7xf64>) {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c4 = arith.constant 4 : index
  %c55 = arith.constant 55 : index
  %cst = arith.constant 0.0 : f64
  sde.su_iterate (%c0, %c0, %c0) to (%c55, %c4, %c4) step (%c1, %c1, %c1) classification(<elementwise>) {
  ^bb0(%k: index, %j: index, %i: index):
    sde.array_layout_root write %U : memref<4x4x55x72x72x7xf64> array_id(3)
    sde.cu_region <single> {
      memref.store %cst, %U[%i, %j, %k, %c0, %c0, %c0] : memref<4x4x55x72x72x7xf64>
      sde.yield
    }
  } {arrayLayout = [{arrayId = 3 : i64, kind = "block_parallel", ownerDims = [2, 1, 0], blockShape = [72, 72, 7], budgetBlockShape = [144, 288, 7], muBlockCount = 880 : i64, role = "write", commVolumeBytes = 0 : i64}]}
  sde.su_distribute <owner_compute> {
  sde.su_iterate (%c0, %c0, %c0) to (%c55, %c4, %c4) step (%c1, %c1, %c1) classification(<stencil>) {
  ^bb0(%k: index, %j: index, %i: index):
    sde.array_layout_root read %U : memref<4x4x55x72x72x7xf64> array_id(3)
    sde.cu_region <single> {
      %v = memref.load %U[%i, %j, %k, %c0, %c0, %c0] : memref<4x4x55x72x72x7xf64>
      memref.store %v, %U[%i, %j, %k, %c0, %c0, %c0] : memref<4x4x55x72x72x7xf64>
      sde.yield
    }
  } {arrayLayout = [{arrayId = 3 : i64, kind = "block_parallel", ownerDims = [2, 1, 0], blockShape = [72, 72, 7], muBlockCount = 880 : i64, role = "read", commVolumeBytes = 99 : i64}], accessMinOffsets = [-1, -1, -1], accessMaxOffsets = [1, 1, 1]}
  }
  return
}
