// RUN: %carts-compile %s --arts-config %S/../examples/arts.cfg --arts-epoch-finish-continuation --start-from=epochs --pipeline=epochs | %FileCheck %s

// Test CPS chain with parity-conditional single-epoch loop (poisson-for pattern).
// The loop body has ONE scf.if with epochs in both then and else branches.
// Verifies:
//   1. Outer wrapping epoch
//   2. scf.if with epoch in then-branch marked for continuation
//   3. Continuation EDT with cps_chain_id
//   4. arts.cps_advance inside continuation
//   5. Advance region contains scf.if (both branches)
//   6. The original scf.for loop is eliminated

// Outer wrapping epoch.
// CHECK: arts.epoch
// Launcher EDT carries a compiler-generated async loop chain id.
// CHECK: arts.edt <task> <intranode> route(%c-1_i32) attributes {arts.cps_chain_id = "[[CHAIN:async_loop_[0-9]+]]"}
// First iteration: scf.if with conditional epoch.
// CHECK: scf.if
// CHECK: arts.continuation_for_epoch
// CPS advance in the then branch recreates the same chain.
// CHECK: arts.cps_advance(%c1_i64, %c10_i64, %true) target "[[CHAIN]]"
// Else branch also stays in the same continuation chain.
// CHECK: } else {
// CHECK: arts.continuation_for_epoch
// CHECK: arts.cps_advance(%c1_i64, %c10_i64, %false) target "[[CHAIN]]"
// No loop remains.
// CHECK-NOT: scf.for

module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f128, dense<128> : vector<2xi64>>, #dlti.dl_entry<f64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i8, dense<[8, 32]> : vector<2xi64>>, #dlti.dl_entry<i64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i16, dense<[16, 32]> : vector<2xi64>>, #dlti.dl_entry<i128, dense<128> : vector<2xi64>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi64>>, #dlti.dl_entry<i32, dense<32> : vector<2xi64>>, #dlti.dl_entry<i1, dense<8> : vector<2xi64>>, #dlti.dl_entry<f16, dense<16> : vector<2xi64>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i64>>, llvm.data_layout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128", llvm.target_triple = "aarch64-unknown-linux-gnu"} {

  func.func private @even_work() -> ()
  func.func private @odd_work() -> ()

  func.func @test_cps_chain_parity(%flag: i1) {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c10 = arith.constant 10 : index
    %c0_i32 = arith.constant 0 : i32
    scf.for %t = %c0 to %c10 step %c1 {
      scf.if %flag {
        %e = arts.epoch {
          arts.edt <task> <intranode> route(%c0_i32) {
          ^bb0:
            func.call @even_work() : () -> ()
            arts.yield
          }
          arts.yield
        } : i64
      } else {
        %e = arts.epoch {
          arts.edt <task> <intranode> route(%c0_i32) {
          ^bb0:
            func.call @odd_work() : () -> ()
            arts.yield
          }
          arts.yield
        } : i64
      }
    }
    return
  }
}
