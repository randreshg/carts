// RUN: %carts-compile %s --arts-config %arts_config --start-from arts-rt-to-llvm --pipeline arts-rt-to-llvm | %FileCheck %s

// When ARTS authored a real shutdown in a continuation function, ARTS-RT lowers
// that op and does not emit the default main_edt shutdown immediately after
// mainBody returns.

// CHECK-LABEL: func.func @main_edt
// CHECK: call @mainBody
// CHECK-NOT: call @arts_shutdown
// CHECK: return
// CHECK-LABEL: func.func private @__cont
// CHECK: call @arts_shutdown

module attributes {
  dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i32, dense<32> : vector<2xi64>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi64>>, #dlti.dl_entry<"dlti.endianness", "little">>,
  llvm.data_layout = "e-m:e-i8:8:32-i16:32-i64:64-n32:64-S128",
  llvm.target_triple = "aarch64-unknown-linux-gnu"
} {
  func.func private @__cont(%arg0: i32, %arg1: !llvm.ptr, %arg2: i32, %arg3: !llvm.ptr) {
    arts.shutdown
    return
  }

  func.func @main() -> i32 {
    %route = arith.constant -1 : i32
    %c0_i32 = arith.constant 0 : i32
    %c1_i32 = arith.constant 1 : i32
    %pack = arts_rt.edt_param_pack(%c0_i32) : i32 : memref<1xi64>
    %cont = arts_rt.edt_create(%pack : memref<1xi64>) depCount(%c1_i32) route(%route) {arts.outlined_func = "__cont"}
    %epoch = arts_rt.create_epoch finish(%cont : i64, %c0_i32 : i32) : i64
    return %c0_i32 : i32
  }
}
