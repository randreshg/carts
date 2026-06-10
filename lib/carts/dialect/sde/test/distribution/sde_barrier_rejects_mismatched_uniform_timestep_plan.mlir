// RUN: %carts-compile %s --O3 --arts-config %arts_config --start-from sde-planning --pipeline codir-to-arts --mlir-print-ir-after-all 2>&1 | %FileCheck %s

// Same trip counts are not enough to form a repeated timestep. SDE must not
// stamp an owner-tile plan when the committed write layout is single-owner; the
// later owner-strip plan remains a real transformed fact.

// CHECK-LABEL: // -----// IR Dump After PatternAnalysis (sde-pattern-analysis) //----- //
// CHECK: func.func @mismatched_uniform_adjacent
// CHECK: sde.su_iterate (%c0, %c0) to (%c8, %c8) step (%c1, %c1) classification(<elementwise>)
// CHECK: sde.su_iterate (%c0) to (%c8) step (%c1) classification(<elementwise>)

// CHECK-LABEL: // -----// IR Dump After DistributionPlanning (distribution-planning) //----- //
// CHECK: func.func @mismatched_uniform_adjacent
// CHECK-NOT: iterationTopology = #sde.iteration_topology<owner_tile>
// CHECK: iterationTopology = #sde.iteration_topology<owner_strip>
// CHECK-SAME: logicalWorkerSlice = [3]
// CHECK-SAME: physicalBlockShape = [3]
// CHECK-SAME: physicalOwnerDims = [0]

// CHECK-LABEL: // -----// IR Dump After ConvertCodirToArts (convert-codir-to-arts) //----- //
// CHECK: attributes {storageBridgeCopy}
// CHECK-NOT: planIterationTopology = #arts.plan_iteration_topology<owner_tile>
// CHECK: arts.barrier
// CHECK: planIterationTopology = #arts.plan_iteration_topology<owner_strip>
// CHECK-SAME: planLogicalWorkerSlice = [3]
// CHECK-SAME: planOwnerDims = [0]
// CHECK-SAME: planPhysicalBlockShape = [3]

module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i32, dense<32> : vector<2xi64>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi64>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i64>>, llvm.data_layout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128", llvm.target_triple = "aarch64-unknown-linux-gnu"} {
  func.func @mismatched_uniform_adjacent(%A: memref<8x8xf64>, %B: memref<8x8xf64>, %Mean: memref<8xf64>) {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c8 = arith.constant 8 : index
    scf.for %t = %c0 to %c8 step %c1 {
      sde.cu_region <parallel> {
        sde.su_iterate (%c0) to (%c8) step (%c1) {
        ^bb0(%b: index):
          scf.for %c = %c0 to %c8 step %c1 {
            %v = memref.load %A[%b, %c] : memref<8x8xf64>
            memref.store %v, %B[%b, %c] : memref<8x8xf64>
          }
          sde.yield
        }
        sde.yield
      }
      sde.cu_region <parallel> {
        sde.su_iterate (%c0) to (%c8) step (%c1) {
        ^bb0(%channel: index):
          %v = memref.load %B[%c0, %channel] : memref<8x8xf64>
          memref.store %v, %Mean[%channel] : memref<8xf64>
          sde.yield
        }
        sde.yield
      }
    }
    return
  }
}
