// RUN: %carts-compile %s --pass-pipeline='builtin.module(arts-epoch-tail-continuation)' | %FileCheck %s

// The pass may inline a final scf.if only when the condition is proven true at
// the use site. A true store that does not dominate the load is not enough.

// CHECK-LABEL: func.func @main
// CHECK: scf.if
// CHECK: arts.epoch
// CHECK-NOT: arts.edt
// CHECK: return %{{.*}} : i32

module attributes {
  arts.runtime_total_nodes = 1 : i64,
  arts.runtime_total_workers = 4 : i64,
  dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<i1, dense<8> : vector<2xi64>>, #dlti.dl_entry<i32, dense<32> : vector<2xi64>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi64>>, #dlti.dl_entry<"dlti.endianness", "little">>,
  llvm.data_layout = "e-m:e-i8:8:32-i16:32-i64:64-n32:64-S128",
  llvm.target_triple = "aarch64-unknown-linux-gnu"
} {
  func.func @main() -> i32 {
    %c0_i32 = arith.constant 0 : i32
    %true = arith.constant true
    %flag = memref.alloca() : memref<i1>
    %cond = memref.load %flag[] : memref<i1>

    scf.if %cond {
      arts.epoch {
        arts.yield
      } : i64
    }

    memref.store %true, %flag[] : memref<i1>
    func.call @tail() : () -> ()
    return %c0_i32 : i32
  }

  func.func private @tail()
}
