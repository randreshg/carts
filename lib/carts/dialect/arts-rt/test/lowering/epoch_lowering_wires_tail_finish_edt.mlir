// RUN: %carts-compile %s --pass-pipeline='builtin.module(epoch-lowering)' | %FileCheck %s --implicit-check-not=arts_rt.wait_on_epoch

// CHECK-LABEL: func.func @main
// CHECK: %[[ZERO:.*]] = arith.constant 0 : i32
// CHECK: %[[CONT_PACK:.*]] = arts_rt.edt_param_pack
// CHECK: %[[ONE:.*]] = arith.constant 1 : i32
// CHECK: %[[CONT:.*]] = arts_rt.edt_create(%[[CONT_PACK]] : memref<1xi64>) depCount(%[[ONE]]) route
// CHECK: %[[EPOCH:.*]] = arts_rt.create_epoch finish(%[[CONT]] : i64, %[[ZERO]] : i32) : i64
// CHECK: arts_rt.edt_create
// CHECK-SAME: epoch(%[[EPOCH]] : i64)
// CHECK: return

module attributes {
  arts.runtime_total_nodes = 1 : i64,
  arts.runtime_total_workers = 4 : i64,
  dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<i64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i32, dense<32> : vector<2xi64>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi64>>, #dlti.dl_entry<"dlti.endianness", "little">>,
  llvm.data_layout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128",
  llvm.target_triple = "aarch64-unknown-linux-gnu"
} {
  func.func @main() {
    %route = arith.constant -1 : i32
    %c0 = arith.constant 0 : i32
    arts.epoch {
      %task_pack = arts_rt.edt_param_pack(%c0) : i32 : memref<1xi64>
      %task = arts_rt.edt_create(%task_pack : memref<1xi64>) depCount(%c0) route(%route) {arts.outlined_func = "__task"}
      arts.yield
    } : i64

    %cont_pack = arts_rt.edt_param_pack(%c0) : i32 : memref<1xi64>
    %cont = arts_rt.edt_create(%cont_pack : memref<1xi64>) depCount(%c0) route(%route) {arts.outlined_func = "__cont"}
    return
  }

  func.func private @__cont(%arg0: i32, %arg1: !llvm.ptr, %arg2: i32, %arg3: !llvm.ptr) {
    arts.shutdown
    return
  }
}
