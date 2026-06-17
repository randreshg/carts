// RUN: %carts-compile %s --arts-config %S/../../../../../../tests/inputs/arts_1t.cfg --pipeline=epochs | %FileCheck %s

// CHECK-LABEL: func.func @default_wsloop_completion_lowers_to_epoch
// CHECK: arts.epoch
// CHECK: arts.edt
// CHECK: scf.for
// CHECK: func.call @carts_kernel_timer_accum
func.func @default_wsloop_completion_lowers_to_epoch(%v: f32,
                                                     %timer: i64) -> f32 {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c2 = arith.constant 2 : index
  %c16 = arith.constant 16 : index
  %A = memref.alloc() : memref<16xf32>
  scf.for %rep = %c0 to %c2 step %c1 {
    omp.parallel {
      omp.wsloop schedule(static) {
        omp.loop_nest (%i) : index = (%c0) to (%c16) step (%c1) {
          memref.store %v, %A[%i] : memref<16xf32>
          omp.yield
        }
      }
      omp.terminator
    }
    func.call @carts_kernel_timer_accum(%timer) : (i64) -> ()
  }
  %out = memref.load %A[%c0] : memref<16xf32>
  return %out : f32
}

func.func private @carts_kernel_timer_accum(i64)
