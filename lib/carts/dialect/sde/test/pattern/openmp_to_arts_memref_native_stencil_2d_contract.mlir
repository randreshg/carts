// RUN: %carts-compile %s --O3 --arts-config %arts_config --start-from sde-planning --pipeline sde-planning --mlir-print-ir-after-all 2>&1 | %FileCheck %s

// Verify that a 2D 5-point Jacobi stencil stays memref-native and is classified
// with nested-IV stencil metadata. SDE owns the layout and access-window facts;
// later stages consume or reject those facts without repairing them.

// CHECK-LABEL: // -----// IR Dump After PatternAnalysis (sde-pattern-analysis) //----- //
// CHECK: func.func @main
// CHECK: sde.su_iterate (%c1, %c1) to (%c63, %c63) step (%c1, %c1) classification(<stencil>) {
// Scalar body preserved:
// CHECK: memref.load
// CHECK: memref.store
// Stencil access metadata stamped by PatternAnalysis:
// CHECK: accessMaxOffsets = [1, 1]
// CHECK-SAME: accessMinOffsets = [-1, -1]
// CHECK-SAME: ownerDims = [0, 1]
// CHECK-SAME: pattern = #sde.pattern<stencil_tiling_nd>
// CHECK-SAME: spatialDims = [0, 1]
// CHECK-SAME: writeFootprint = [1, 1]
// 4 adds for the 5-point stencil computation:
// CHECK: arith.addf
// CHECK: arith.addf
// CHECK: arith.addf
// CHECK: arith.addf

// SDE commits the owner-tile storage/access facts by DistributionPlanning.
// CHECK-LABEL: // -----// IR Dump After DistributionPlanning (distribution-planning) //----- //
// CHECK: sde.su_iterate
// CHECK: iterationTopology = #sde.iteration_topology<owner_tile>
// CHECK-SAME: logicalWorkerSlice = [16, 32]
// CHECK-SAME: physicalBlockShape = [16, 32]
// CHECK-SAME: physicalHaloShape = [1, 1]
// CHECK-SAME: physicalOwnerDims = [0, 1]

module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i32, dense<32> : vector<2xi64>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi64>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i64>>, llvm.data_layout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128", llvm.target_triple = "aarch64-unknown-linux-gnu"} {
  func.func @main(%A: memref<64x64xf64>, %B: memref<64x64xf64>) {
    %c1 = arith.constant 1 : index
    %c63 = arith.constant 63 : index
    omp.parallel {
      omp.wsloop {
        omp.loop_nest (%i) : index = (%c1) to (%c63) step (%c1) {
          scf.for %j = %c1 to %c63 step %c1 {
            %im1 = arith.subi %i, %c1 : index
            %ip1 = arith.addi %i, %c1 : index
            %jm1 = arith.subi %j, %c1 : index
            %jp1 = arith.addi %j, %c1 : index
            %n = memref.load %A[%im1, %j] : memref<64x64xf64>
            %s = memref.load %A[%ip1, %j] : memref<64x64xf64>
            %w = memref.load %A[%i, %jm1] : memref<64x64xf64>
            %e = memref.load %A[%i, %jp1] : memref<64x64xf64>
            %c = memref.load %A[%i, %j] : memref<64x64xf64>
            %s0 = arith.addf %n, %s : f64
            %s1 = arith.addf %w, %e : f64
            %s2 = arith.addf %s0, %s1 : f64
            %sum = arith.addf %s2, %c : f64
            memref.store %sum, %B[%i, %j] : memref<64x64xf64>
          }
          omp.yield
        }
      }
      omp.terminator
    }
    return
  }
}
