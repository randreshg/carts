// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-coarse-avoidance,verify-sde-mu-layout)' 2>&1 | %FileCheck %s
// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-coarse-avoidance,verify-sde-coarse-avoidance)'

// Best-effort realize: a flat MU with a committed single-contiguous-owner
// elementwise BLOCK plan (owner [0], block 256) is rank-expanded to the block
// grid exactly as rank expansion would: memref<1024xf32> -> memref<4x256xf32>, A[i] ->
// A[i/256, i%256] — so coarse allocation is AVOIDED by realizing the committed
// finest grain as structure. The chained verify-sde-mu-layout proves the
// structure (ownerDims == recover(structure)); the second RUN proves
// verify-sde-coarse-avoidance accepts the realized MU (exit 0).

// CHECK-LABEL: func.func @realize_1d
// CHECK: sde.mu_alloc : memref<4x256xf32>
// CHECK: %[[BID:.*]] = arith.divui %{{.*}}, %c256
// CHECK: %[[OFF:.*]] = arith.remui %{{.*}}, %c256
// CHECK: memref.store %{{.*}}, %{{.*}}[%[[BID]], %[[OFF]]] : memref<4x256xf32>

func.func @realize_1d() {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c1024 = arith.constant 1024 : index
  %cst = arith.constant 1.0 : f32
  %A = sde.mu_alloc : memref<1024xf32>
  sde.cu_region <parallel> {
    sde.su_iterate (%c0) to (%c1024) step (%c1) classification(<elementwise>) {
    ^bb0(%i: index):
      memref.store %cst, %A[%i] : memref<1024xf32>
      sde.yield
    } {physicalOwnerDims = [0], physicalBlockShape = [256]}
    sde.yield
  }
  return
}
