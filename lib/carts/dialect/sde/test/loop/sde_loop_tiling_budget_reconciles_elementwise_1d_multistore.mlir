// RUN: %carts-compile %s --O3 --arts-config %inputs_dir/arts_64t.cfg \
// RUN:   --start-from sde-planning --pipeline sde-planning --mlir-print-ir-after-all 2>&1 \
// RUN:   | %FileCheck %s

// A stream-style 1-D writer must realize the committed budget grain in the SU
// step and physical block, even when one nest writes several arrays.

// CHECK-LABEL: // -----// IR Dump After Tiling (tiling) //----- //
// CHECK-LABEL: func.func @budget_reconciles_elementwise_1d_multistore
// CHECK-NOT: arith.constant 10937500 : index
// CHECK: arith.constant 262075 : index
// CHECK: sde.su_iterate (%c0) to (%c700000000) step (%c262075{{(_[0-9]+)?}}) classification(<elementwise>) {
// CHECK: } {
// CHECK-SAME: physicalBlockShape = [262075]
// CHECK-SAME: physicalOwnerDims = [0]

module {
  func.func @budget_reconciles_elementwise_1d_multistore(%A: memref<700000000xf64>, %B: memref<700000000xf64>, %C: memref<700000000xf64>) {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c700000000 = arith.constant 700000000 : index
    %one = arith.constant 1.000000e+00 : f64
    %two = arith.constant 2.000000e+00 : f64
    %zero = arith.constant 0.000000e+00 : f64
    sde.su_iterate (%c0) to (%c700000000) step (%c1) classification(<elementwise>) {
    ^bb0(%i: index):
      sde.cu_region <single> {
        memref.store %one, %A[%i] : memref<700000000xf64>
        memref.store %two, %B[%i] : memref<700000000xf64>
        memref.store %zero, %C[%i] : memref<700000000xf64>
        sde.yield
      }
    }
    return
  }
}
