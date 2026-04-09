// RUN: %carts-compile %s --arts-config %S/../examples/arts.cfg --start-from=epochs --pipeline=pre-lowering | %FileCheck %s

// Test that a non-CPS EDT (no timestep loop) compiles through pre-lowering
// without any CPS chain attributes. Verifies:
//   1. The EDT is lowered to edt_create at pre-lowering
//   2. No cps_chain_id appears on any operation
//   3. No cps_iter_counter_param_idx appears
//   4. No cps_advance is generated

// CHECK: arts_rt.edt_create
// CHECK-NOT: cps_chain_id
// CHECK-NOT: cps_iter_counter_param_idx
// CHECK-NOT: arts.cps_advance

module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f128, dense<128> : vector<2xi64>>, #dlti.dl_entry<f64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i8, dense<[8, 32]> : vector<2xi64>>, #dlti.dl_entry<i64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i16, dense<[16, 32]> : vector<2xi64>>, #dlti.dl_entry<i128, dense<128> : vector<2xi64>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi64>>, #dlti.dl_entry<i32, dense<32> : vector<2xi64>>, #dlti.dl_entry<i1, dense<8> : vector<2xi64>>, #dlti.dl_entry<f16, dense<16> : vector<2xi64>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i64>>, llvm.data_layout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128", llvm.target_triple = "aarch64-unknown-linux-gnu"} {

  func.func private @sink() -> ()

  func.func @test_non_cps_param_pack() {
    %c0_i32 = arith.constant 0 : i32
    %e = arts.epoch {
      arts.edt <task> <intranode> route(%c0_i32) {
      ^bb0:
        func.call @sink() : () -> ()
        arts.yield
      }
      arts.yield
    } : i64
    return
  }
}
