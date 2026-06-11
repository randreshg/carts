// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-rank-expand-mu,raise-to-mu-access-window,verify-sde-mu-access-window)' 2>&1 | %FileCheck %s --implicit-check-not=sde.mu_access_window

// Fail-closed / conservative: out-of-scope MUs get NO access window and the
// pass mutates nothing (it only ever inserts windows). Chaining
// verify-sde-mu-access-window also asserts the verifier SKIPS these MUs (it must
// not demand a window for an out-of-scope MU). Cases:
//   * matmul classification is out of the elementwise/stencil scope (left flat),
//   * a dynamic owner extent cannot be statically rank-expanded (left flat),
//   * a multi-owner committed plan is left flat and skipped.
//
// No window op is emitted anywhere; the verifier passes (every MU is skipped).

// CHECK-LABEL: func.func @fail_closed_matmul
func.func @fail_closed_matmul() {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c64 = arith.constant 64 : index
  %c128 = arith.constant 128 : index
  %A = sde.mu_alloc : memref<128x64xf32>
  %C = sde.mu_alloc : memref<128x64xf32>
  sde.su_iterate (%c0) to (%c128) step (%c1) classification(<matmul>) {
  ^bb0(%i: index):
    sde.cu_region <single> {
      scf.for %j = %c0 to %c64 step %c1 {
        %v = memref.load %A[%i, %j] : memref<128x64xf32>
        memref.store %v, %C[%i, %j] : memref<128x64xf32>
    }
      sde.yield
    }
  } {physicalOwnerDims = [0], physicalBlockShape = [16, 64]}
  return
}

// CHECK-LABEL: func.func @fail_closed_dynamic
func.func @fail_closed_dynamic(%n: index) {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c64 = arith.constant 64 : index
  %cst = arith.constant 0.0 : f32
  %A = sde.mu_alloc(%n) : memref<?x64xf32>
  sde.su_iterate (%c0) to (%n) step (%c1) classification(<elementwise>) {
  ^bb0(%i: index):
    sde.cu_region <single> {
      scf.for %j = %c0 to %c64 step %c1 {
        memref.store %cst, %A[%i, %j] : memref<?x64xf32>
    }
      sde.yield
    }
  } {physicalOwnerDims = [0], physicalBlockShape = [16, 64]}
  return
}

// Multi-owner (physicalOwnerDims = [0, 1]) is left flat (single-contiguous owner
// only) and skipped.
// CHECK-LABEL: func.func @fail_closed_multi_owner
func.func @fail_closed_multi_owner() {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c64 = arith.constant 64 : index
  %c128 = arith.constant 128 : index
  %cst = arith.constant 0.0 : f32
  %A = sde.mu_alloc : memref<128x64xf32>
  sde.su_iterate (%c0) to (%c128) step (%c1) classification(<elementwise>) {
  ^bb0(%i: index):
    sde.cu_region <single> {
      scf.for %j = %c0 to %c64 step %c1 {
        memref.store %cst, %A[%i, %j] : memref<128x64xf32>
    }
      sde.yield
    }
  } {physicalOwnerDims = [0, 1], physicalBlockShape = [16, 16]}
  return
}
