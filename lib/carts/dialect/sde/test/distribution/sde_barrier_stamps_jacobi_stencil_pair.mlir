// RUN: %carts-compile %s --O3 --arts-config %arts_config --start-from sde-planning --pipeline sde-planning --mlir-print-ir-after-all 2>&1 | %FileCheck %s

// Same-shape out-of-place 1-D stencil stages that exchange A/B buffers are a
// timestep candidate even when both stages are classified as stencils.

// CHECK-LABEL: // -----// IR Dump After BarrierElimination (barrier-elimination) //----- //
// CHECK: func.func @jacobi_stencil_pair
// CHECK: sde.su_iterate
// CHECK-SAME: classification(<stencil>)
// CHECK: } {
// CHECK-SAME: asyncStrategy = #sde.async_strategy<advance_stage>
// CHECK-SAME: pattern = #sde.pattern<stencil_tiling_nd>
// CHECK-SAME: repetitionStructure = #sde.repetition_structure<full_timestep>
// CHECK: sde.su_barrier
// CHECK-SAME: barrierReason = #sde.barrier_reason<timestep_stage_boundary>
// CHECK: sde.su_iterate
// CHECK-SAME: classification(<stencil>)
// CHECK: } {
// CHECK-SAME: asyncStrategy = #sde.async_strategy<advance_stage>
// CHECK-SAME: pattern = #sde.pattern<stencil_tiling_nd>
// CHECK-SAME: repetitionStructure = #sde.repetition_structure<full_timestep>
module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i32, dense<32> : vector<2xi64>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi64>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i64>>, llvm.data_layout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128", llvm.target_triple = "aarch64-unknown-linux-gnu"} {
  func.func @jacobi_stencil_pair(%A: memref<18xf64>, %B: memref<18xf64>) {
    %c1 = arith.constant 1 : index
    %c16 = arith.constant 16 : index
    sde.su_iterate (%c1) to (%c16) step (%c1) {
    ^bb0(%i: index):
      sde.cu_region <single> {
        %im1 = arith.subi %i, %c1 : index
        %ip1 = arith.addi %i, %c1 : index
        %n = memref.load %B[%im1] : memref<18xf64>
        %s = memref.load %B[%ip1] : memref<18xf64>
        %sum = arith.addf %n, %s : f64
        memref.store %sum, %A[%i] : memref<18xf64>
        sde.yield
      }
    }
    sde.su_barrier
    sde.su_iterate (%c1) to (%c16) step (%c1) {
    ^bb0(%i: index):
      sde.cu_region <single> {
        %im1 = arith.subi %i, %c1 : index
        %ip1 = arith.addi %i, %c1 : index
        %n = memref.load %A[%im1] : memref<18xf64>
        %s = memref.load %A[%ip1] : memref<18xf64>
        %sum = arith.addf %n, %s : f64
        memref.store %sum, %B[%i] : memref<18xf64>
        sde.yield
      }
    }
    return
  }
}
