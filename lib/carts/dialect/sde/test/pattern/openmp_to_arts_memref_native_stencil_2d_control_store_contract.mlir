// RUN: %carts-compile %s --O3 --arts-config %arts_config --start-from sde-planning --pipeline codir-to-arts --mlir-print-ir-after-all 2>&1 | %FileCheck %s

// A loop-invariant rank-0 control store before an inner stencil loop must not
// force a 1-D physical owner-strip fallback. PatternAnalysis promotes the
// stencil to a 2-D SDE scheduling unit, hoists the scalar control store outside
// the promoted tile body, and downstream planning preserves the 2-D owner tile.

// CHECK-LABEL: // -----// IR Dump After PatternAnalysis (sde-pattern-analysis) //----- //
// CHECK: memref.store {{.*}} : memref<i1>
// CHECK-NEXT: sde.su_iterate (%c1, %c1) to (%c63, %c63) step (%c1, %c1) classification(<stencil>) {
// CHECK: sde.cu_region <parallel> {
// CHECK-NOT: memref.store {{.*}} : memref<i1>
// CHECK: memref.load {{.*}} : memref<64x64xf64>
// CHECK: accessMaxOffsets = [1, 1]
// CHECK-SAME: accessMinOffsets = [-1, -1]
// CHECK-SAME: ownerDims = [0, 1]
// CHECK-SAME: pattern = #sde.pattern<stencil_tiling_nd>

// CHECK-LABEL: // -----// IR Dump After DistributionPlanning (distribution-planning) //----- //
// CHECK: sde.su_iterate (%c1, %c1) to (%c63, %c63)
// CHECK: iterationTopology = #sde.iteration_topology<owner_tile>
// CHECK-SAME: physicalOwnerDims = [0, 1]

// CHECK: codir.codelet
// CHECK-SAME: pattern = #codir.pattern<stencil_tiling_nd>
// CHECK-SAME: tile_owner_dims = [0, 1]

module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i32, dense<32> : vector<2xi64>>, #dlti.dl_entry<i1, dense<8> : vector<2xi64>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi64>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i64>>, llvm.data_layout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128", llvm.target_triple = "aarch64-unknown-linux-gnu"} {
  func.func @main(%A: memref<64x64xf64>, %B: memref<64x64xf64>, %flag: memref<i1>) {
    %true = arith.constant true
    %cst = arith.constant 5.000000e-01 : f64
    %c1 = arith.constant 1 : index
    %c63 = arith.constant 63 : index
    omp.parallel {
      omp.wsloop {
        omp.loop_nest (%i) : index = (%c1) to (%c63) step (%c1) {
          memref.store %true, %flag[] : memref<i1>
          scf.for %j = %c1 to %c63 step %c1 {
            %im1 = arith.subi %i, %c1 : index
            %ip1 = arith.addi %i, %c1 : index
            %jm1 = arith.subi %j, %c1 : index
            %jp1 = arith.addi %j, %c1 : index
            %n = memref.load %A[%im1, %j] : memref<64x64xf64>
            %s = memref.load %A[%ip1, %j] : memref<64x64xf64>
            %w = memref.load %A[%i, %jm1] : memref<64x64xf64>
            %e = memref.load %A[%i, %jp1] : memref<64x64xf64>
            %s0 = arith.addf %n, %s : f64
            %s1 = arith.addf %w, %e : f64
            %sum = arith.addf %s0, %s1 : f64
            %scaled = arith.mulf %sum, %cst : f64
            memref.store %scaled, %B[%i, %j] : memref<64x64xf64>
          }
          omp.yield
        }
      }
      omp.terminator
    }
    return
  }
}
