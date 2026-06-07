// RUN: not %carts-compile %s --pass-pipeline='builtin.module(verify-sde-mu-access-window)' 2>&1 | %FileCheck %s

// Verifier R1 (mode faithfulness): the MU is written only (mode = write),
// but a hand-crafted `read` window is present. The window count is exactly one,
// so a mode-blind coverage check would accept it; R1 must reject because the
// window's mode does not describe how the CU accesses the MU.

// CHECK: error: {{.*}}wrong access mode{{.*}}accesses this MU as write

func.func @wrong_mode_window() {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c256 = arith.constant 256 : index
  %c1024 = arith.constant 1024 : index
  %cst = arith.constant 1.0 : f32
  %A = sde.mu_alloc : memref<4x256xf32>
  sde.cu_region <parallel> {
    sde.mu_access_window read %A : memref<4x256xf32> owner_dims(1) block_lo [0] block_hi [4] valid [256]
    sde.su_iterate (%c0) to (%c1024) step (%c1) classification(<elementwise>) {
    ^bb0(%i: index):
      %bid = arith.divui %i, %c256 : index
      %off = arith.remui %i, %c256 : index
      memref.store %cst, %A[%bid, %off] : memref<4x256xf32>
      sde.yield
    } {physicalOwnerDims = [0], physicalBlockShape = [256]}
    sde.yield
  }
  return
}
