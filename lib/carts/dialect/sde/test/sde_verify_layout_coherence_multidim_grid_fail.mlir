// RUN: not %carts-compile %s --pass-pipeline='builtin.module(verify-sde-layout-coherence)' 2>&1 | %FileCheck %s

// CHECK: ownerDims/blockShape disagree with rank-expanded MU physical layout
func.func @incoherent_multidim_owner_grid() {
  %c0 = arith.constant 0 : index
  %c8 = arith.constant 8 : index
  %c1 = arith.constant 1 : index
  %A = sde.mu_alloc : memref<8x8xf64>
  sde.su_iterate (%c0, %c0) to (%c8, %c8) step (%c1, %c1) {
  ^bb0(%i: index, %j: index):
    sde.array_layout write array_id(0) owner [0, 1] block [8, 8] logical [8, 8]
    sde.array_layout_root write %A : memref<8x8xf64> array_id(0)
    sde.yield
  }
  return
}
