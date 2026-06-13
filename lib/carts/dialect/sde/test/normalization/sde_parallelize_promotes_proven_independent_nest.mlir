// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-parallelize)' 2>&1 | %FileCheck %s
// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-parallelize,sde-parallelize)' 2>&1 | %FileCheck %s --check-prefix=IDEMPOTENT

// A perfectly-nested, loop-carried-dependence-free nest wrapped in a
// cu_region<single> must be PROMOTED to a parallel leaf CU. Previously the
// raise stamped <single> on the just-proven-independent nest, which discarded
// the proof: hasParallelLeafCu (SdeLoopPatternFacts) only recognizes
// <parallel>, so the nest was invisible to pattern-facts, interchange, tiling,
// fusion, and layout-assignment. The leaf CU kind is now proof-derived.

// CHECK-LABEL: func.func @promote_independent_nest
// CHECK: sde.su_iterate
// CHECK: sde.cu_region <parallel>
// The dependence-free outer axis becomes the su_iterate domain; the inner axis
// is retained as a leaf scf.for (per-axis emission), not demoted to serial.
// CHECK: scf.for
// CHECK: memref.store
// CHECK-NOT: sde.cu_region <single>

// Promotion is monotone and re-entrant: a second sde-parallelize run is a
// no-op (the already-raised body sits under su_iterate/cu_region<parallel>),
// so the result is byte-identical and never regresses to <single>.
// IDEMPOTENT-LABEL: func.func @promote_independent_nest
// IDEMPOTENT: sde.cu_region <parallel>
// IDEMPOTENT-NOT: sde.cu_region <single>

func.func @promote_independent_nest(%A: memref<8x8xf32>, %v: f32) {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c8 = arith.constant 8 : index
  sde.cu_region <single> {
    scf.for %i = %c0 to %c8 step %c1 {
      scf.for %j = %c0 to %c8 step %c1 {
        memref.store %v, %A[%i, %j] : memref<8x8xf32>
      }
    }
  }
  return
}
