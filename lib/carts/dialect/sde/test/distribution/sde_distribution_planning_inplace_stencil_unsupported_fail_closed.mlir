// Multi-worker in-place stencils outside the rectangular row-major wavefront
// schedule still fail closed in SDE.
// RUN: not %carts-compile %s --O3 --arts-config %inputs_dir/arts_64t.cfg \
// RUN:   --start-from sde-planning --pipeline sde-planning 2>&1 \
// RUN:   | %FileCheck %s

// CHECK: in-place self-read stencil (Gauss-Seidel family) has loop-carried neighbor offsets
// CHECK-SAME: wavefront/skew
// CHECK-SAME: not implemented

module {
  func.func @triangular_in_place(%A: memref<8x8xf64>) {
    %c1 = arith.constant 1 : index
    %c7 = arith.constant 7 : index
    omp.parallel {
      omp.wsloop {
        omp.loop_nest (%i) : index = (%c1) to (%c7) step (%c1) {
          scf.for %j = %i to %c7 step %c1 {
            %im1 = arith.subi %i, %c1 : index
            %jm1 = arith.subi %j, %c1 : index
            %n = memref.load %A[%im1, %j] : memref<8x8xf64>
            %w = memref.load %A[%i, %jm1] : memref<8x8xf64>
            %sum = arith.addf %n, %w : f64
            memref.store %sum, %A[%i, %j] : memref<8x8xf64>
          }
          omp.yield
        }
      }
      omp.terminator
    }
    return
  }
}
