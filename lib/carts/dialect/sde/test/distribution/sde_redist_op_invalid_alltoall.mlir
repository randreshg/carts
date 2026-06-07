// RUN: not %carts-compile %s --pass-pipeline='builtin.module()' 2>&1 | %FileCheck %s

// all_to_all_like is a re-layout between two distinct partitionings; identical
// owner dims are not a movement.

// CHECK: error: {{.*}}all_to_all_like requires differing owner dims

func.func @bad_all_to_all(%A: memref<128x64xf32>) {
  sde.redist <all_to_all_like> %A : memref<128x64xf32> from owner [0] block [16, 64] to owner [0] block [32, 64]
  return
}
