// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-redistribute)' 2>&1 | %FileCheck %s

// Poisson copy-back can carry only the read-side committed fact while the
// write `array_layout_root` is present. SDE must still unify `u` to `unew` grain.

// CHECK-LABEL: func.func @alternating_buffer_root_grain_reconcile
// CHECK: blockShape = [256, 1024]
// CHECK-NOT: blockShape = [512, 1024]

func.func @alternating_buffer_root_grain_reconcile(%U: memref<2x512x1024xf64>,
                                                  %UNEW: memref<4x256x1024xf64>) {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c1024 = arith.constant 1024 : index
  sde.su_iterate (%c0, %c0) to (%c1024, %c1024) step (%c1, %c1)
      classification(<elementwise>) {
  ^bb0(%i: index, %j: index):
    sde.array_layout_root read %UNEW : memref<4x256x1024xf64> array_id(2)
    sde.array_layout_root write %U : memref<2x512x1024xf64> array_id(1)
    sde.cu_region <parallel> {
      %c256 = arith.constant 256 : index
      %rb = arith.divui %i, %c256 : index
      %rl = arith.remui %i, %c256 : index
      %v = memref.load %UNEW[%rb, %rl, %j] : memref<4x256x1024xf64>
      %c512 = arith.constant 512 : index
      %wb = arith.divui %i, %c512 : index
      %wl = arith.remui %i, %c512 : index
      memref.store %v, %U[%wb, %wl, %j] : memref<2x512x1024xf64>
      sde.yield
    }
    sde.yield
  } {arrayLayout = [
    {arrayId = 2 : i64, blockShape = [256, 1024], kind = "block_parallel",
     muBlockCount = 4 : i64, ownerDims = [0], role = "read"}]}
  return
}
