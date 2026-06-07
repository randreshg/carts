// RUN: %carts-compile %s --arts-config %arts_config --start-from arts-rt-to-llvm --pipeline arts-rt-to-llvm | %FileCheck %s

// CHECK-LABEL: func.func @wait_on_epoch_fail_closed
// CHECK: %[[FALSE:.*]] = arith.constant false
// CHECK: %[[OK:.*]] = call @arts_wait_on_handle
// CHECK: %[[FAILED:.*]] = arith.cmpi eq, %[[OK]], %[[FALSE]]
// CHECK: scf.if %[[FAILED]]
// CHECK: call @arts_shutdown
// CHECK: llvm.intr.trap

module attributes {
  dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<i64, dense<64> : vector<2xi64>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi64>>, #dlti.dl_entry<"dlti.endianness", "little">>,
  llvm.data_layout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128",
  llvm.target_triple = "aarch64-unknown-linux-gnu"
} {
  func.func @wait_on_epoch_fail_closed(%epoch: i64) {
    arts_rt.wait_on_epoch %epoch : i64
    return
  }
}
