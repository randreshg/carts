// RUN: not %carts-compile %s --pass-pipeline='builtin.module(canonicalize)' 2>&1 | %FileCheck %s

// A zero-radius halo names no neighbor face: it is an identity move and the
// op verifier rejects it (the movement-op family must name a real movement).
// CHECK: a zero-radius halo is an identity move
func.func @su_halo_identity(%A: memref<8x4xf64>) {
  sde.su_distribute <owner_compute> {
    sde.su_halo %A : memref<8x4xf64> owner [0] block [2, 4] halo [0, 0]
  }
  return
}
