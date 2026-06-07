// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-rank-expand-mu,sde-coarse-avoidance,verify-sde-mu-layout)' 2>&1 | %FileCheck %s

// Idempotence over rank expansion: once the block grid is in the type,
// sde-coarse-avoidance recognizes it (recognizeExpandedBlockGridMu) and does
// nothing — exactly one div/mod localization per access, no re-expansion.

// CHECK-LABEL: func.func @noop_already_expanded
// CHECK: sde.mu_alloc : memref<4x256xf32>
// CHECK-COUNT-1: arith.divui
// CHECK-NOT: arith.divui

func.func @noop_already_expanded() {
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
