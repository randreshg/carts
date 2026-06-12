// RUN: not %carts-compile %s --pass-pipeline='builtin.module(verify-sde-redistribute)' 2>&1 | %FileCheck %s

// GROUNDING gate: a well-formed sde.redist whose array has no committed writer
// layout/provenance is rejected before ARTS.

// CHECK: error: {{.*}}not grounded in committed SDE layout

func.func @ungrounded(%A: memref<128x64xf32>) {
  sde.redist <reduce_scatter_like> %A : memref<128x64xf32> from owner [0] block [16, 64] to owner [0] block [16, 64]
  return
}
