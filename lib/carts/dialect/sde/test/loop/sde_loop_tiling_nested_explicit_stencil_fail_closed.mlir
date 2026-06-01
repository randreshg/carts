// RUN: %carts-compile %s --O3 --arts-config %inputs_dir/arts_64t.cfg \
// RUN:   --start-from sde-planning --pipeline sde-to-codir \
// RUN:   --mlir-print-ir-after-all 2>&1 \
// RUN:   | awk '/IR Dump After Tiling/,/IR Dump After VerifySdeCpsPlan/' \
// RUN:   | %FileCheck %s

// If the nested owner loop cannot be promoted into the SDE loop rank, Tiling
// and DistributionPlanning must not commit a row-only physical stencil plan
// from the outer owner IV.

// CHECK-LABEL: // -----// IR Dump After Tiling (tiling) //----- //
// CHECK: func.func @nested_explicit_unpromotable_stencil
// CHECK: sde.su_iterate (%c1) to (%c255) step (%{{.*}}) classification(<stencil>) {
// CHECK: } {accessMaxOffsets = [1, 1]
// CHECK-SAME: ownerDims = [0, 1]
// CHECK-NOT: distributionKind
// CHECK-NOT: iterationTopology = #sde.iteration_topology<owner_strip>
// CHECK-NOT: logicalWorkerSlice
// CHECK-NOT: partitionGraph
// CHECK-NOT: partitionScore
// CHECK-NOT: physicalOwnerDims
// CHECK-NOT: physicalBlockShape
// CHECK-LABEL: // -----// IR Dump After DistributionPlanning (distribution-planning) //----- //
// CHECK: func.func @nested_explicit_unpromotable_stencil
// CHECK: sde.su_iterate (%c1) to (%c255) step (%{{.*}}) classification(<stencil>) {
// CHECK: } {accessMaxOffsets = [1, 1]
// CHECK-SAME: ownerDims = [0, 1]
// CHECK-NOT: distributionKind
// CHECK-NOT: iterationTopology = #sde.iteration_topology<owner_strip>
// CHECK-NOT: logicalWorkerSlice
// CHECK-NOT: partitionGraph
// CHECK-NOT: partitionScore
// CHECK-NOT: physicalOwnerDims
// CHECK-NOT: physicalBlockShape
// CHECK-LABEL: // -----// IR Dump After VerifySdeCpsPlan

module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i32, dense<32> : vector<2xi64>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi64>>, #dlti.dl_entry<"dlti.endianness", "little">>, llvm.data_layout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128", llvm.target_triple = "aarch64-unknown-linux-gnu"} {
  func.func @nested_explicit_unpromotable_stencil(%A: memref<256x256xf64>, %B: memref<256x256xf64>) {
    %c1 = arith.constant 1 : index
    %c255 = arith.constant 255 : index
    sde.cu_region <parallel> {
      sde.su_iterate (%c1) to (%c255) step (%c1) classification(<stencil>) {
      ^bb0(%i: index):
        %dependent_ub = arith.subi %c255, %i : index
        scf.for %j = %c1 to %dependent_ub step %c1 {
          %im1 = arith.subi %i, %c1 : index
          %ip1 = arith.addi %i, %c1 : index
          %jm1 = arith.subi %j, %c1 : index
          %jp1 = arith.addi %j, %c1 : index
          %x0 = memref.load %A[%im1, %j] : memref<256x256xf64>
          %x1 = memref.load %A[%ip1, %j] : memref<256x256xf64>
          %y0 = memref.load %A[%i, %jm1] : memref<256x256xf64>
          %y1 = memref.load %A[%i, %jp1] : memref<256x256xf64>
          %s0 = arith.addf %x0, %x1 : f64
          %s1 = arith.addf %y0, %y1 : f64
          %sum = arith.addf %s0, %s1 : f64
          memref.store %sum, %B[%i, %j] : memref<256x256xf64>
        }
        sde.yield
      } {accessMaxOffsets = [1, 1],
         accessMinOffsets = [-1, -1],
         ownerDims = [0, 1],
         pattern = #sde.pattern<stencil_tiling_nd>,
         spatialDims = [0, 1],
         writeFootprint = [1, 1]}
      sde.yield
    }
    return
  }
}
