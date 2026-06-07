// RUN: not %carts-compile %s --pass-pipeline='builtin.module()' 2>&1 | %FileCheck %s

// all_gather_like must move to a replicated target (no owner dims).

// CHECK: error: {{.*}}require a replicated target

func.func @bad_all_gather(%A: memref<128x64xf32>) {
  sde.redist <all_gather_like> %A : memref<128x64xf32> from owner [0] block [16, 64] to owner [0] block [16, 64]
  return
}
