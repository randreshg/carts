// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-redistribute)' 2>&1 | %FileCheck %s

// Cross-owner repartition: consumer required-read layout differs from the
// committed home layout -> sde.su_all_to_all.

// CHECK-LABEL: func.func @repartition_owner_dims
// CHECK: sde.su_distribute <owner_compute>
// CHECK: sde.su_all_to_all %{{.*}} : memref<256x256xf32> array_id(0) source owner [0] block [128, 256] target owner [0, 1] block [64, 128]

func.func @repartition_owner_dims(%T: memref<256x256xf32>, %U: memref<256x256xf32>) {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c256 = arith.constant 256 : index
  %cst = arith.constant 0.0 : f32
  sde.su_iterate (%c0, %c0) to (%c256, %c256) step (%c1, %c1) {
  ^bb0(%i: index, %j: index):
    sde.array_layout_root write %T : memref<256x256xf32> array_id(0)
    sde.cu_region <single> {
      memref.store %cst, %T[%i, %j] : memref<256x256xf32>
      sde.yield
    }
  } {arrayLayout = [{arrayId = 0 : i64, kind = "block_parallel", ownerDims = [0], blockShape = [128, 256], muBlockCount = 2 : i64, role = "write"}]}
  sde.su_distribute <owner_compute> {
  sde.su_iterate (%c0, %c0) to (%c256, %c256) step (%c1, %c1) {
  ^bb0(%i: index, %j: index):
    sde.array_layout_root read %T : memref<256x256xf32> array_id(0)
    sde.array_layout_root write %U : memref<256x256xf32> array_id(1)
    sde.cu_region <single> {
      %v = memref.load %T[%i, %j] : memref<256x256xf32>
      memref.store %v, %U[%i, %j] : memref<256x256xf32>
      sde.yield
    }
  } {arrayLayout = [{arrayId = 0 : i64, kind = "block_parallel", ownerDims = [0, 1], blockShape = [64, 128], muBlockCount = 8 : i64, role = "read"}, {arrayId = 1 : i64, kind = "block_parallel", ownerDims = [0, 1], blockShape = [64, 128], muBlockCount = 8 : i64, role = "write"}]}
  }
  return
}
