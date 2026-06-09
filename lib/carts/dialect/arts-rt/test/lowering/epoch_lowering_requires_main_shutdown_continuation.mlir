// RUN: %carts-compile %s --pass-pipeline='builtin.module(epoch-lowering)' | %FileCheck %s

// ARTS-RT wires a finish continuation only for the final main continuation
// authored by ARTS. Helper functions and main tails without a real shutdown
// continuation remain blocking epoch waits.

// CHECK-LABEL: func.func @helper
// CHECK: arts_rt.create_epoch : i64
// CHECK: arts_rt.wait_on_epoch
// CHECK-LABEL: func.func @main
// CHECK: arts_rt.create_epoch : i64
// CHECK: arts_rt.wait_on_epoch

module attributes {
  arts.runtime_total_nodes = 1 : i64,
  arts.runtime_total_workers = 4 : i64,
  dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<i32, dense<32> : vector<2xi64>>, #dlti.dl_entry<i64, dense<64> : vector<2xi64>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi64>>, #dlti.dl_entry<"dlti.endianness", "little">>,
  llvm.data_layout = "e-m:e-i8:8:32-i16:32-i64:64-n32:64-S128",
  llvm.target_triple = "aarch64-unknown-linux-gnu"
} {
  func.func @helper() {
    %route = arith.constant -1 : i32
    %c0 = arith.constant 0 : i32
    arts.epoch {
      %task_pack = arts_rt.edt_param_pack(%c0) : i32 : memref<1xi64>
      %task = arts_rt.edt_create(%task_pack : memref<1xi64>) depCount(%c0) route(%route) {arts.outlined_func = "__task"}
      arts.yield
    } : i64

    %cont_pack = arts_rt.edt_param_pack(%c0) : i32 : memref<1xi64>
    %cont = arts_rt.edt_create(%cont_pack : memref<1xi64>) depCount(%c0) route(%route) {arts.outlined_func = "__cont_with_shutdown"}
    return
  }

  func.func @main() {
    %route = arith.constant -1 : i32
    %c0 = arith.constant 0 : i32
    arts.epoch {
      %task_pack = arts_rt.edt_param_pack(%c0) : i32 : memref<1xi64>
      %task = arts_rt.edt_create(%task_pack : memref<1xi64>) depCount(%c0) route(%route) {arts.outlined_func = "__task"}
      arts.yield
    } : i64

    %cont_pack = arts_rt.edt_param_pack(%c0) : i32 : memref<1xi64>
    %cont = arts_rt.edt_create(%cont_pack : memref<1xi64>) depCount(%c0) route(%route) {arts.outlined_func = "__cont_without_shutdown"}
    return
  }

  func.func private @__task(%arg0: i32, %arg1: !llvm.ptr, %arg2: i32, %arg3: !llvm.ptr) {
    return
  }

  func.func private @__cont_with_shutdown(%arg0: i32, %arg1: !llvm.ptr, %arg2: i32, %arg3: !llvm.ptr) {
    arts.shutdown
    return
  }

  func.func private @__cont_without_shutdown(%arg0: i32, %arg1: !llvm.ptr, %arg2: i32, %arg3: !llvm.ptr) {
    return
  }
}
