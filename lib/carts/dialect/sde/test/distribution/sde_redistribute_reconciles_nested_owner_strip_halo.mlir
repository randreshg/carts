// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-redistribute)' 2>&1 | %FileCheck %s

// A nested Jacobi-style stencil may arrive after rank expansion with an owner
// strip MU while stale read/write facts still name the full logical owner tile.
// SDE must reconcile the facts to the realized MU grid, wrap the consumer in a
// movement scope, and emit the halo over the owner strip.

// CHECK-LABEL: func.func @nested_owner_strip_halo
// CHECK: sde.su_distribute <owner_compute>
// CHECK: sde.su_halo %[[A:.*]] : memref<2x128x256xf64> array_id(0) owner [0] block [1, 128, 256] halo [1, 0, 0]
// CHECK-NOT: sde.su_halo {{.*}}array_id(2)
// CHECK-NOT: halo [0, 0, 0]
// CHECK: sde.array_layout_root read %[[A]] : memref<2x128x256xf64> array_id(0)
// CHECK: arrayLayout = [{arrayId = 0 : i64, blockShape = [128, 256]
// CHECK-SAME: ownerDims = [0]
// CHECK-NOT: rank-expanded halo edge owner dims do not match
// CHECK-NOT: movement edge for array

func.func @nested_owner_strip_halo(%A: memref<2x128x256xf64>,
                                   %B: memref<2x128x256xf64>,
                                   %C: memref<2x128x256xf64>) {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c128 = arith.constant 128 : index
  %c256 = arith.constant 256 : index
  %zero = arith.constant 0.0 : f64
  sde.su_distribute <blocked> {
    sde.su_iterate (%c0, %c0) to (%c256, %c256) step (%c1, %c1) {
    ^bb0(%i: index, %j: index):
      sde.array_layout_root write %A : memref<2x128x256xf64> array_id(0)
      sde.array_layout_root write %B : memref<2x128x256xf64> array_id(1)
      sde.array_layout_root write %C : memref<2x128x256xf64> array_id(2)
      sde.cu_region <parallel> {
        %bi = arith.divui %i, %c128 : index
        %li = arith.remui %i, %c128 : index
        memref.store %zero, %A[%bi, %li, %j] : memref<2x128x256xf64>
        memref.store %zero, %B[%bi, %li, %j] : memref<2x128x256xf64>
        memref.store %zero, %C[%bi, %li, %j] : memref<2x128x256xf64>
        sde.yield
      }
      sde.yield
    } {arrayLayout = [
      {arrayId = 0 : i64, blockShape = [128, 128],
       budgetBlockShape = [256, 256], kind = "block_parallel",
       muBlockCount = 4 : i64, ownerDims = [0, 1], role = "write"},
      {arrayId = 1 : i64, blockShape = [128, 256],
       kind = "block_parallel", muBlockCount = 2 : i64,
       ownerDims = [0], role = "write"},
      {arrayId = 2 : i64, blockShape = [128, 256],
       kind = "block_parallel", muBlockCount = 2 : i64,
       ownerDims = [0], role = "write"}]}
  }
  sde.su_iterate (%c0) to (%c256) step (%c1) classification(<stencil>) {
  ^bb0(%i: index):
    sde.array_layout_root read %A : memref<2x128x256xf64> array_id(0)
    sde.array_layout_root write %B : memref<2x128x256xf64> array_id(1)
    sde.array_layout_root read %C : memref<2x128x256xf64> array_id(2)
    sde.cu_region <parallel> {
      %im1 = arith.subi %i, %c1 : index
      %ip1 = arith.addi %i, %c1 : index
      scf.for %j = %c0 to %c256 step %c1 {
        %bc = arith.divui %i, %c128 : index
        %lc = arith.remui %i, %c128 : index
        %center = memref.load %C[%bc, %lc, %j] : memref<2x128x256xf64>
        %b0 = arith.divui %im1, %c128 : index
        %l0 = arith.remui %im1, %c128 : index
        %left = memref.load %A[%b0, %l0, %j] : memref<2x128x256xf64>
        %b1 = arith.divui %ip1, %c128 : index
        %l1 = arith.remui %ip1, %c128 : index
        %right = memref.load %A[%b1, %l1, %j] : memref<2x128x256xf64>
        %pair = arith.addf %left, %right : f64
        %sum = arith.addf %pair, %center : f64
        %bo = arith.divui %i, %c128 : index
        %lo = arith.remui %i, %c128 : index
        memref.store %sum, %B[%bo, %lo, %j] : memref<2x128x256xf64>
      }
      sde.yield
    }
    sde.yield
  } {arrayLayout = [
      {arrayId = 0 : i64, blockShape = [128, 128],
       budgetBlockShape = [256, 256], kind = "block_parallel",
       muBlockCount = 4 : i64, ownerDims = [0, 1], role = "read"},
      {arrayId = 1 : i64, blockShape = [128, 256],
       kind = "block_parallel", muBlockCount = 2 : i64,
       ownerDims = [0], role = "write"},
      {arrayId = 2 : i64, blockShape = [128, 256],
       budgetBlockShape = [256, 256], kind = "block_parallel",
       muBlockCount = 2 : i64, ownerDims = [0], role = "read"}],
     accessMinOffsets = [0, -1], accessMaxOffsets = [0, 1],
     ownerDims = [0, 1], spatialDims = [0, 1], writeFootprint = [0, 0]}
  return
}
