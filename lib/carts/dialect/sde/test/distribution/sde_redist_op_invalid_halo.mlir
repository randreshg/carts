// RUN: not %carts-compile %s --pass-pipeline='builtin.module()' 2>&1 | %FileCheck %s

// A haloShape is the halo_like family's signature and is invalid on any other
// family.

// CHECK: error: {{.*}}haloShape is only valid for the halo_like family

func.func @bad_halo(%A: memref<128x64xf32>) {
  sde.redist <all_to_all_like> %A : memref<128x64xf32> from owner [0] block [16, 64] to owner [1] block [128, 16] halo [1, 0]
  return
}
