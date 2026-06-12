// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-rank-expand-mu,raise-to-mu-access-window,verify-sde-mu-access-window)' 2>&1 | %FileCheck %s

// Stencil: a stencil-classified, single-owner block layout is in scope.
// The committed layout carries a halo (physicalHaloShape = [2]) but rank expansion does NOT
// grow the tile (halo data lives in neighbouring blocks; halo redistribution is
// a later transform). So the raised window's in-tile valid extent is the block
// extent [16, 16] (NOT halo-grown to [20, ...]); growing it would exceed the
// expanded tile and the op verifier would reject it. The expanded A root is
// write-only.

// CHECK-LABEL: func.func @raise_window_stencil
// CHECK: sde.mu_alloc : memref<128x16xf32>
// CHECK: sde.mu_alloc : memref<8x16x16xf32>
// CHECK: sde.mu_access_window write %{{.*}} : memref<8x16x16xf32> owner_dims(1) block_lo [0] block_hi [8] valid [16, 16]

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
  } {arrayLayout = [{arrayId = 0 : i64, blockShape = [16, 16], commVolumeBytes = 0 : i64, kind = "block_parallel", muBlockCount = 8 : i64, ownerDims = [0], role = "write"}], physicalOwnerDims = [0], physicalBlockShape = [16, 16], physicalHaloShape = [2]}
  return
}
