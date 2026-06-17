// RUN: %carts-compile %s --pass-pipeline='builtin.module(convert-openmp-to-sde)' 2>&1 | %FileCheck %s

// CHECK-LABEL: func.func @default_wsloop_requires_completion
// CHECK: sde.su_iterate
// CHECK-SAME: nowait
// CHECK: sde.cu_region <parallel>
// CHECK: sde.su_barrier
func.func @default_wsloop_requires_completion(%A: memref<16xf32>, %v: f32) {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c16 = arith.constant 16 : index
  omp.parallel {
    omp.wsloop schedule(static) {
      omp.loop_nest (%i) : index = (%c0) to (%c16) step (%c1) {
        memref.store %v, %A[%i] : memref<16xf32>
        omp.yield
      }
    }
    omp.terminator
  }
  return
}

// CHECK-LABEL: func.func @explicit_nowait_wsloop_stays_async
// CHECK: sde.su_iterate
// CHECK-SAME: nowait
// CHECK: sde.cu_region <parallel>
// CHECK-NOT: sde.su_barrier
// CHECK: return
func.func @explicit_nowait_wsloop_stays_async(%A: memref<16xf32>, %v: f32) {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c16 = arith.constant 16 : index
  omp.parallel {
    omp.wsloop nowait schedule(static) {
      omp.loop_nest (%i) : index = (%c0) to (%c16) step (%c1) {
        memref.store %v, %A[%i] : memref<16xf32>
        omp.yield
      }
    }
    omp.terminator
  }
  return
}
