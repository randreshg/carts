// RUN: %carts-compile %s --O3 --arts-config %inputs_dir/arts_64t.cfg \
// RUN:   --start-from sde-planning --pipeline sde-planning --mlir-print-ir-after-all 2>&1 \
// RUN:   | %FileCheck %s

// Tiling must consume the committed array-layout budget for multi-owner
// elementwise writers before DistributionPlanning stamps the physical plan. The
// generic 64-thread 2-D tiler would choose 640-wide chunks for this 5120x5120
// f64 copy, while the committed budget grain is [512, 512].

// CHECK-LABEL: // -----// IR Dump After Tiling (tiling) //----- //
// CHECK-LABEL: func.func @budget_reconciles_elementwise_2d
// CHECK-NOT: arith.constant 640 : index
// CHECK: arith.constant 512 : index
// CHECK: arith.constant 512 : index
// CHECK: sde.su_iterate (%c0, %c0) to (%c5120, %c5120) step (%c512{{(_[0-9]+)?}}, %c512{{(_[0-9]+)?}}) classification(<elementwise>) {
// CHECK: } {
// CHECK-SAME: iterationTopology = #sde.iteration_topology<owner_tile>
// CHECK-SAME: logicalWorkerSlice = [1024, 512]
// CHECK-SAME: physicalBlockShape = [512, 512]
// CHECK-SAME: physicalOwnerDims = [0, 1]

module {
  func.func @budget_reconciles_elementwise_2d(%A: memref<5120x5120xf64>, %B: memref<5120x5120xf64>) {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c5120 = arith.constant 5120 : index
    sde.su_iterate (%c0, %c0) to (%c5120, %c5120) step (%c1, %c1) classification(<elementwise>) {
    ^bb0(%i: index, %j: index):
      sde.cu_region <single> {
        %v = memref.load %B[%i, %j] : memref<5120x5120xf64>
        memref.store %v, %A[%i, %j] : memref<5120x5120xf64>
        sde.yield
      }
    }
    return
  }
}
