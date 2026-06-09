// RUN: %carts-compile %s --pass-pipeline='builtin.module(arts-epoch-tail-continuation)' | %FileCheck %s

// A final standalone EDT is not enough to order a shutdown-owning tail. The
// pass must fail closed unless it exposes a real final arts.epoch frontier.

// CHECK-LABEL: func.func @main
// CHECK: arts.edt
// CHECK: call @tail
// CHECK-NOT: arts.shutdown
// CHECK: return %{{.*}} : i32

module attributes {
  arts.runtime_total_nodes = 1 : i64,
  arts.runtime_total_workers = 4 : i64,
  dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<i32, dense<32> : vector<2xi64>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi64>>, #dlti.dl_entry<"dlti.endianness", "little">>,
  llvm.data_layout = "e-m:e-i8:8:32-i16:32-i64:64-n32:64-S128",
  llvm.target_triple = "aarch64-unknown-linux-gnu"
} {
  func.func @main() -> i32 {
    %route = arith.constant -1 : i32
    %c0_i32 = arith.constant 0 : i32

    arts.edt <task> <intranode> route(%route) {
      arts.yield
    }

    func.call @tail() : () -> ()
    return %c0_i32 : i32
  }

  func.func private @tail()
}
