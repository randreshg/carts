// RUN: not %carts-compile %s --O3 --arts-config %inputs_dir/arts_64t.cfg \
// RUN:   --start-from sde-planning --pipeline sde-planning 2>&1 \
// RUN:   | %FileCheck %s

// In-place neighbor-dependent updates are not nested-loop parallelism. SDE must
// fail closed with the wavefront/skew blocker instead of promoting the inner
// output dimension as an owner tile.

// CHECK: in-place self-read stencil (Gauss-Seidel family)
// CHECK-SAME: exposing legal parallelism requires an SDE wavefront/skew

module {
  func.func @seidel_inplace_fail_closed(%A: memref<8x8xf64>) {
    %c1 = arith.constant 1 : index
    %c7 = arith.constant 7 : index
    omp.parallel {
      omp.wsloop {
        omp.loop_nest (%i) : index = (%c1) to (%c7) step (%c1) {
          scf.for %j = %c1 to %c7 step %c1 {
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
