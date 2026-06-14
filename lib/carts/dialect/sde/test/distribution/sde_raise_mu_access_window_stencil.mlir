// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-rank-expand-mu,verify-sde-mu-layout,sde-storage-to-arts-db)' 2>&1 | %FileCheck %s

// Stencil: a stencil-classified, single-owner block layout is in scope.
// The committed layout carries a halo () but rank expansion does NOT
// grow the tile (halo data lives in neighbouring blocks; halo redistribution is
// a later transform). Query-derived boundary lowering stamps an out window with
// in-tile valid extent [16, 16] (NOT halo-grown to [20, ...]); growing it would
// exceed the expanded tile. The expanded A root is write-only.

// CHECK-LABEL: func.func @raise_window_stencil
// CHECK: arts.db_alloc
// CHECK-SAME: <coarse>
// CHECK: memref.cast {{.*}} to memref<128x16xf32>
// CHECK: arts.db_alloc
// CHECK-SAME: <block>
// CHECK: memref.cast {{.*}} to memref<8x16x16xf32>
// CHECK-NOT: sde.mu_access_window
// CHECK: arts.db_access_window
// CHECK-SAME: mode = #arts.mode<out>
// CHECK-SAME: ownerDimCount = 1
// CHECK-SAME: validExtents = [16, 16]

func.func @raise_window_stencil() {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c16 = arith.constant 16 : index
  %c128 = arith.constant 128 : index
  %B = sde.mu_alloc : memref<128x16xf32>
  %A = sde.mu_alloc : memref<128x16xf32>
  sde.su_iterate (%c0) to (%c128) step (%c1) classification(<stencil>) {
  ^bb0(%i: index):
    sde.array_layout_root write %A : memref<128x16xf32> array_id(0)
    sde.cu_region <single> {
      scf.for %j = %c0 to %c16 step %c1 {
        %v = memref.load %B[%i, %j] : memref<128x16xf32>
        memref.store %v, %A[%i, %j] : memref<128x16xf32>
    }
      sde.yield
    }
  } {arrayLayout = [{arrayId = 0 : i64, blockShape = [16, 16], kind = "block_parallel", muBlockCount = 8 : i64, ownerDims = [0], role = "write"}]}
  return
}
