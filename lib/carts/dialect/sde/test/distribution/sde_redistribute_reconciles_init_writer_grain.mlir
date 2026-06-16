// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-redistribute)' 2>&1 | %FileCheck %s

// Jacobi-style init writes a coarse blockShape while a later copy-back reader
// commits a finer budget grain on the same owner dim. SDE must unify the write
// and read facts before edge collection instead of failing closed on a
// same-owner retile.

// CHECK-LABEL: func.func @init_writer_grain_reconcile
// CHECK-NOT: same-owner block-grain mismatch must be reconciled
// CHECK-NOT: refusing to invent redistribution

func.func @init_writer_grain_reconcile(%A: memref<2x512x1024xf64>,
                                       %B: memref<2x512x1024xf64>) {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c512 = arith.constant 512 : index
  %c1024 = arith.constant 1024 : index
  %zero = arith.constant 0.0 : f64
  sde.su_distribute <blocked> {
    sde.su_iterate (%c0, %c0) to (%c1024, %c1024) step (%c1, %c1) {
    ^bb0(%i: index, %j: index):
      sde.array_layout_root write %A : memref<2x512x1024xf64> array_id(0)
      sde.array_layout_root write %B : memref<2x512x1024xf64> array_id(1)
      sde.cu_region <parallel> {
        %bi = arith.divui %i, %c512 : index
        %li = arith.remui %i, %c512 : index
        memref.store %zero, %A[%bi, %li, %j] : memref<2x512x1024xf64>
        memref.store %zero, %B[%bi, %li, %j] : memref<2x512x1024xf64>
        sde.yield
      }
      sde.yield
    } {arrayLayout = [
      {arrayId = 0 : i64, blockShape = [512, 1024], kind = "block_parallel",
       muBlockCount = 2 : i64, ownerDims = [0], role = "write"},
      {arrayId = 1 : i64, blockShape = [512, 1024], kind = "block_parallel",
       muBlockCount = 2 : i64, ownerDims = [0], role = "write"}]}
  }
  sde.su_iterate (%c0, %c0) to (%c1024, %c1024) step (%c1, %c1)
      classification(<elementwise>) {
  ^bb0(%i: index, %j: index):
    sde.array_layout_root read %B : memref<2x512x1024xf64> array_id(1)
    sde.array_layout_root write %A : memref<2x512x1024xf64> array_id(0)
    sde.cu_region <parallel> {
      %bi = arith.divui %i, %c512 : index
      %li = arith.remui %i, %c512 : index
      %v = memref.load %B[%bi, %li, %j] : memref<2x512x1024xf64>
      memref.store %v, %A[%bi, %li, %j] : memref<2x512x1024xf64>
      sde.yield
    }
    sde.yield
  } {arrayLayout = [
    {arrayId = 1 : i64, blockShape = [512, 1024],
     budgetBlockShape = [256, 1024], kind = "block_parallel",
     muBlockCount = 4 : i64, ownerDims = [0], role = "read"},
    {arrayId = 0 : i64, blockShape = [512, 1024], kind = "block_parallel",
     muBlockCount = 2 : i64, ownerDims = [0], role = "write"}]}
  return
}
