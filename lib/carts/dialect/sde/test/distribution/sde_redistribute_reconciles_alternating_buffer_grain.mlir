// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-redistribute)' 2>&1 | %FileCheck %s

// Poisson-style double-buffer copy reads a finer `unew` grain and writes a
// coarser `u` fact on the same owner dim. SDE must unify the write buffer to
// the read source grain before redistribution edge collection.

// CHECK-LABEL: func.func @alternating_buffer_grain_reconcile
// CHECK: blockShape = [256, 1024]
// CHECK-NOT: blockShape = [512, 1024]
// CHECK-NOT: refusing to invent redistribution

func.func @alternating_buffer_grain_reconcile(%U: memref<2x512x1024xf64>,
                                               %UNEW: memref<4x256x1024xf64>) {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c512 = arith.constant 512 : index
  %c1024 = arith.constant 1024 : index
  %zero = arith.constant 0.0 : f64
  sde.su_iterate (%c0, %c0) to (%c1024, %c1024) step (%c1, %c1)
      classification(<elementwise>) {
  ^bb0(%i: index, %j: index):
    sde.array_layout_root read %UNEW : memref<4x256x1024xf64> array_id(1)
    sde.array_layout_root write %U : memref<2x512x1024xf64> array_id(0)
    sde.cu_region <parallel> {
      %bi = arith.divui %i, %c512 : index
      %li = arith.remui %i, %c512 : index
      %v = memref.load %UNEW[%bi, %li, %j] : memref<4x256x1024xf64>
      memref.store %v, %U[%bi, %li, %j] : memref<2x512x1024xf64>
      sde.yield
    }
    sde.yield
  } {arrayLayout = [
    {arrayId = 1 : i64, blockShape = [256, 1024], kind = "block_parallel",
     muBlockCount = 4 : i64, ownerDims = [0], role = "read"},
    {arrayId = 0 : i64, blockShape = [512, 1024], kind = "block_parallel",
     muBlockCount = 2 : i64, ownerDims = [0], role = "write"}]}
  return
}
