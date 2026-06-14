// RUN: %carts-compile %s --pass-pipeline='builtin.module(loop-interchange)' 2>&1 | %FileCheck %s

// Direct-memory matmul with an affine j-k nest is interchanged to scf k-j; S4d
// re-raise in the planning pipeline recovers affine form later.

// CHECK-LABEL: func.func @affine_matmul_interchange
// CHECK: classification(<matmul>)
// CHECK: scf.for
// CHECK: memref.store {{.*}}, %{{.*}}[{{.*}}, {{.*}}] : memref<8x8xf32>
// CHECK: scf.for %[[K:.*]] =
// CHECK: scf.for %[[J:.*]] =
// CHECK: memref.load %{{.*}}[%[[K]], %[[J]]] : memref<8x8xf32>
// CHECK-NOT: affine.for

func.func @affine_matmul_interchange(
    %A: memref<8x8xf32>, %B: memref<8x8xf32>, %C: memref<8x8xf32>) {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c8 = arith.constant 8 : index
  %zero = arith.constant 0.0 : f32

  sde.su_iterate (%c0) to (%c8) step (%c1) classification(<matmul>) {
  ^bb0(%i: index):
    sde.cu_region <parallel> {
      affine.for %j = 0 to 8 {
        memref.store %zero, %C[%i, %j] : memref<8x8xf32>
        affine.for %k = 0 to 8 {
          %a = memref.load %A[%i, %k] : memref<8x8xf32>
          %b = memref.load %B[%k, %j] : memref<8x8xf32>
          %prod = arith.mulf %a, %b : f32
          %old = memref.load %C[%i, %j] : memref<8x8xf32>
          %new = arith.addf %old, %prod : f32
          memref.store %new, %C[%i, %j] : memref<8x8xf32>
        }
      }
      sde.yield
    }
    sde.yield
  }
  return
}
