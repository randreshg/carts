// RUN: %carts-compile %s --O3 --arts-config %inputs_dir/arts_64t.cfg \
// RUN:   --start-from sde-planning --pipeline sde-to-codir --mlir-print-ir-after-all 2>&1 \
// RUN:   | %FileCheck %s

// A 2-D writer whose committed storage owner is the row dimension must not be
// promoted into a 2-D physical owner tile by late distribution planning.

// CHECK-LABEL: // -----// IR Dump After DistributionPlanning (distribution-planning) //----- //
// CHECK-LABEL: func.func @budget_reconciles_single_owner_2d_writer
// CHECK-NOT: physicalOwnerDims = [0, 1]
// CHECK: sde.su_iterate
// CHECK: physicalBlockShape = [17, 16320]
// CHECK-SAME: physicalOwnerDims = [0]

module {
  func.func @budget_reconciles_single_owner_2d_writer(%A: memref<8160x16320xf64>, %tmp: memref<8160xf64>) {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c8160 = arith.constant 8160 : index
    %c16320 = arith.constant 16320 : index
    %zero = arith.constant 0.000000e+00 : f64
    sde.cu_region <parallel> {
      sde.su_iterate (%c0, %c0) to (%c8160, %c16320) step (%c1, %c1) classification(<elementwise>) {
      ^bb0(%i: index, %j: index):
        memref.store %zero, %A[%i, %j] : memref<8160x16320xf64>
        sde.yield
      }
      sde.yield
    }
    sde.cu_region <parallel> {
      sde.su_iterate (%c0) to (%c8160) step (%c1) classification(<elementwise_pipeline>) {
      ^bb0(%i: index):
        %acc = memref.load %tmp[%i] : memref<8160xf64>
        scf.for %j = %c0 to %c16320 step %c1 {
          %v = memref.load %A[%i, %j] : memref<8160x16320xf64>
          %sum = arith.addf %acc, %v : f64
          memref.store %sum, %tmp[%i] : memref<8160xf64>
        }
        sde.yield
      }
      sde.yield
    }
    return
  }
}
