// RUN: not %carts-compile %s --O3 --arts-config %arts_config --start-from sde-planning --pipeline codir-to-arts --mlir-print-ir-after-all 2>&1 | %FileCheck %s

// Verify that generic completion-token graphs are rejected before CODIR.

// CHECK-LABEL: // -----// IR Dump After ConvertOpenMPToSde (convert-openmp-to-sde) //----- //
// CHECK: sde.mu_reduction_decl @sum_i32 : i32 kind(<add>) identity(0 : i32)
// CHECK: func.func @typed_tokens(%arg0: memref<8xi32>, %arg1: !sde.completion)
// CHECK: sde.cu_task {
// CHECK: memref.store %c0_i32, %arg0[%c0] : memref<8xi32>
// CHECK: sde.su_barrier(%arg1 : !sde.completion)

// CHECK: error: 'sde.su_barrier' op carries a generic token/dataflow dependency graph

module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<i64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i32, dense<32> : vector<2xi64>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi64>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i64>>, llvm.data_layout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128", llvm.target_triple = "aarch64-unknown-linux-gnu"} {
  sde.mu_reduction_decl @sum_i32 : i32 kind(<add>) identity(0 : i32)

  func.func @typed_tokens(%A: memref<8xi32>, %done: !sde.completion) {
    %c0 = arith.constant 0 : index
    %c0_i32 = arith.constant 0 : i32

    sde.cu_region <parallel> {
      sde.cu_task {
        memref.store %c0_i32, %A[%c0] : memref<8xi32>
        sde.yield
      }
      sde.su_barrier(%done : !sde.completion)
      sde.yield
    }
    return
  }
}
