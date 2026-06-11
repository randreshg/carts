// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-rank-expand-mu,verify-sde-mu-layout)' 2>&1 | %FileCheck %s

// Fail-closed / conservative: out-of-scope plans are NOT converted and get
// NO op-attribute promise and NO compatibility attr — the mu_alloc stays
// flat (logical rank) and accesses keep their logical indices.
//
//   * matmul classification is out of the elementwise/stencil scope.
//   * a dynamic owner extent cannot be statically rank-expanded.
//
// Neither array is expanded; no div/mod localization is emitted anywhere.

// CHECK-NOT: arith.divui
// CHECK-NOT: arith.remui

// CHECK-LABEL: func.func @fail_closed_matmul
// CHECK: sde.mu_alloc : memref<128x64xf32>
// CHECK: memref.load %{{.*}}[%{{.*}}, %{{.*}}] : memref<128x64xf32>
// CHECK: memref.store %{{.*}}, %{{.*}}[%{{.*}}, %{{.*}}] : memref<128x64xf32>

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
// CHECK: sde.mu_alloc({{.*}}) : memref<?x64xf32>
// CHECK: memref.store %{{.*}}, %{{.*}}[%{{.*}}, %{{.*}}] : memref<?x64xf32>

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
