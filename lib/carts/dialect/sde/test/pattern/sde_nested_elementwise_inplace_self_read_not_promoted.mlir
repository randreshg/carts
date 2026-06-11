// RUN: %carts-compile %s --O3 --arts-config %inputs_dir/arts_64t.cfg \
// RUN:   --pipeline=sde-planning 2>&1 | %FileCheck %s

// SDE may promote affine-disjoint elementwise inner loops into real owner-tile
// shape, but not when the same root is read through a non-point self map.

// CHECK-LABEL: func.func @inplace_transpose_self_read
// CHECK-NOT: sde.su_iterate (%c0, %c0) to (%c8, %c8)
// CHECK: sde.su_iterate (%c0) to (%c8) step
// CHECK-SAME: classification(<elementwise>)
// CHECK: scf.for %[[ROW:arg[0-9]+]] = %[[OWNER:arg[0-9]+]] to
// CHECK: scf.for %[[COL:arg[0-9]+]] = %c0 to %c8
// CHECK: memref.load %arg0[%[[COL]], %[[ROW]]] : memref<8x8xf64>
// CHECK: memref.store %{{.*}}, %arg0[%[[ROW]], %[[COL]]] : memref<8x8xf64>
// CHECK: } {arrayLayout =
// CHECK-SAME: inPlaceSharedState
// CHECK-NOT: inPlaceSafe
// CHECK-NOT: sde.su_iterate (%c0, %c0) to (%c8, %c8)

module {
  func.func @inplace_transpose_self_read(%A: memref<8x8xf64>) {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c8 = arith.constant 8 : index
    %one = arith.constant 1.000000e+00 : f64
    omp.parallel {
      omp.wsloop {
        omp.loop_nest (%i) : index = (%c0) to (%c8) step (%c1) {
          scf.for %j = %c0 to %c8 step %c1 {
            %v = memref.load %A[%j, %i] : memref<8x8xf64>
            %next = arith.addf %v, %one : f64
            memref.store %next, %A[%i, %j] : memref<8x8xf64>
          }
          omp.yield
        }
      }
      omp.terminator
    }
    return
  }
}
