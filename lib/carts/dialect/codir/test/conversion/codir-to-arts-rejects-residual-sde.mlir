// RUN: not %carts-compile %s --pass-pipeline='builtin.module(convert-codir-to-arts)' 2>&1 \
// RUN:   | %FileCheck %s

// CHECK: SDE operation reached CODIR-to-ARTS after boundary lowering
// CHECK-SAME: convert-sde-to-codir

module {
  func.func @residual_sde_is_rejected(%arg0: memref<256x256xf32>) {
    sde.redist <reduce_scatter_like> %arg0 : memref<256x256xf32> from owner [0] block [128, 256] to owner [0] block [128, 256] cost 2097152
    return
  }
}
