// RUN: %carts-compile %s --pass-pipeline='builtin.module(verify-sde-coarse-avoidance)' 2>&1 | %FileCheck %s

// In-place MUs are accepted unless SDE has a realizable committed block plan.

// CHECK-LABEL: func.func @accepts_inplace

func.func @accepts_inplace() {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c64 = arith.constant 64 : index
  %c128 = arith.constant 128 : index
  %A = sde.mu_alloc : memref<128x64xf32>
  sde.cu_region <parallel> {
    sde.su_iterate (%c0) to (%c128) step (%c1) classification(<elementwise>) {
    ^bb0(%i: index):
      scf.for %j = %c0 to %c64 step %c1 {
        %v = memref.load %A[%i, %j] : memref<128x64xf32>
        %w = arith.addf %v, %v : f32
        memref.store %w, %A[%i, %j] : memref<128x64xf32>
      }
      sde.yield
    } {inPlaceSafe}
    sde.yield
  }
  return
}
