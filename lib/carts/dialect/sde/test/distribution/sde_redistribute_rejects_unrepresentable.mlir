// RUN: not %carts-compile %s --pass-pipeline='builtin.module(sde-redistribute)' 2>&1 | %FileCheck %s

// Non-reduction layout mismatches fail closed until SDE commits a real
// redistribution transform for that geometry.
// CHECK: error: {{.*}}redistribut

func.func @unrepresentable(%T: memref<256x256xf32>,
                           %G: memref<256x256xf32>) {
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
  sde.su_iterate (%c0, %c0) to (%c256, %c256) step (%c1, %c1) {
  ^bb0(%i: index, %j: index):
    sde.array_layout_root read %T : memref<256x256xf32> array_id(0)
    sde.array_layout_root write %G : memref<256x256xf32> array_id(1)
    sde.cu_region <single> {
      %t = memref.load %T[%j, %i] : memref<256x256xf32>
      memref.store %t, %G[%i, %j] : memref<256x256xf32>
      sde.yield
    }
  } {arrayLayout = [{arrayId = 0 : i64, kind = "block_parallel", ownerDims = [1], blockShape = [128, 256], muBlockCount = 2 : i64, role = "read"}, {arrayId = 1 : i64, kind = "block_parallel", ownerDims = [0], blockShape = [128, 256], muBlockCount = 2 : i64, role = "write"}]}
  return
}
