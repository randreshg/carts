// RUN: %carts-compile %s --arts-config %S/../../examples/arts.cfg --start-from=arts-to-llvm --pipeline=arts-to-llvm | %FileCheck %s

// total_workers stays runtime-adaptive and lowers to arts_get_total_workers().

// CHECK-LABEL: func.func @test_total_workers
// CHECK: %[[W:.*]] = call @arts_get_total_workers() : () -> i32
// CHECK: return %[[W]] : i32
// CHECK-NOT: arts.runtime_query

module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f128, dense<128> : vector<2xi64>>, #dlti.dl_entry<f64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i8, dense<[8, 32]> : vector<2xi64>>, #dlti.dl_entry<i64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i16, dense<[16, 32]> : vector<2xi64>>, #dlti.dl_entry<i128, dense<128> : vector<2xi64>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi64>>, #dlti.dl_entry<i32, dense<32> : vector<2xi64>>, #dlti.dl_entry<i1, dense<8> : vector<2xi64>>, #dlti.dl_entry<f16, dense<16> : vector<2xi64>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i64>>, llvm.data_layout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128", llvm.target_triple = "aarch64-unknown-linux-gnu"} {
  func.func @test_total_workers() -> i32 {
    %w = arts.runtime_query <total_workers> -> i32
    return %w : i32
  }
}
