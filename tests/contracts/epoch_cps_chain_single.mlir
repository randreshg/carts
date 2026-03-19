// RUN: %carts-compile %s --arts-config %S/../examples/arts.cfg --arts-epoch-finish-continuation --start-from=epochs --pipeline=epochs | %FileCheck %s

// Test that CPS chain transform converts a single-epoch time-step loop
// into a continuation chain. Verifies:
//   1. An outer wrapping epoch
//   2. A continuation EDT with cps_chain_id and continuation attributes
//   3. arts.cps_advance placeholder
//   4. A launcher EDT with first iteration's epoch marked for continuation
//   5. The original scf.for loop is eliminated

// CHECK: arts.epoch
// Continuation EDT with CPS chain attributes.
// CHECK: arts.cps_chain_id = "chain_0"
// CPS advance placeholder.
// CHECK: arts.cps_advance
// Launcher EDT creates first iteration's epoch for continuation.
// CHECK: arts.continuation_for_epoch
// No loop remains.
// CHECK-NOT: scf.for

module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f128, dense<128> : vector<2xi32>>, #dlti.dl_entry<f64, dense<64> : vector<2xi32>>, #dlti.dl_entry<i8, dense<[8, 32]> : vector<2xi32>>, #dlti.dl_entry<i64, dense<64> : vector<2xi32>>, #dlti.dl_entry<i16, dense<[16, 32]> : vector<2xi32>>, #dlti.dl_entry<i128, dense<128> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi32>>, #dlti.dl_entry<i32, dense<32> : vector<2xi32>>, #dlti.dl_entry<i1, dense<8> : vector<2xi32>>, #dlti.dl_entry<f16, dense<16> : vector<2xi32>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i32>>, llvm.data_layout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128", llvm.target_triple = "aarch64-unknown-linux-gnu"} {

  func.func private @sink() -> ()

  func.func @test_cps_chain_single_epoch() {
    %c0 = arith.constant 0 : index
    %c2 = arith.constant 2 : index
    %c100 = arith.constant 100 : index
    %c0_i32 = arith.constant 0 : i32
    scf.for %t = %c0 to %c100 step %c2 {
      %e = arts.epoch {
        arts.edt <task> <intranode> route(%c0_i32) {
        ^bb0:
          func.call @sink() : () -> ()
          arts.yield
        }
        arts.yield
      } : i64
    }
    return
  }
}
