// RUN: %carts-compile %s --arts-config %S/../../examples/arts.cfg --arts-epoch-finish-continuation --start-from=epochs --pipeline=epochs | %FileCheck %s

// Test CPS chain with conditional epoch (Case 2: jacobi2d-like pattern).
// Two epochs per iteration, second is conditional (inside scf.if).
// Verifies:
//   1. Outer wrapping epoch
//   2. Launcher with epoch_0 marked for continuation
//   3. cont_0 handles the conditional epoch_1 (scf.if with else-branch advance)
//   4. cont_1 (last) has cps_advance targeting chain_0
//   5. The original scf.for loop is eliminated

// Outer wrapping epoch.
// CHECK: arts.epoch
// Launcher contains epoch_0 marked for continuation.
// CHECK: arts.continuation_for_epoch
// First continuation (cont_0).
// CHECK: arts.cps_chain_id = "{{.*chain_0}}"
// Conditional epoch_1 inside scf.if, also marked for continuation.
// CHECK: scf.if
// CHECK: arts.continuation_for_epoch
// Last continuation (cont_1).
// CHECK: arts.cps_chain_id = "{{.*chain_1}}"
// CPS advance in cont_1.
// CHECK: arts.cps_advance
// The else-branch also has advance logic.
// CHECK: arts.cps_advance
// No loop remains.
// CHECK-NOT: scf.for

module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f128, dense<128> : vector<2xi64>>, #dlti.dl_entry<f64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i8, dense<[8, 32]> : vector<2xi64>>, #dlti.dl_entry<i64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i16, dense<[16, 32]> : vector<2xi64>>, #dlti.dl_entry<i128, dense<128> : vector<2xi64>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi64>>, #dlti.dl_entry<i32, dense<32> : vector<2xi64>>, #dlti.dl_entry<i1, dense<8> : vector<2xi64>>, #dlti.dl_entry<f16, dense<16> : vector<2xi64>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i64>>, llvm.data_layout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128", llvm.target_triple = "aarch64-unknown-linux-gnu"} {

  func.func private @compute() -> ()
  func.func private @converge_check() -> ()

  func.func @test_cps_chain_conditional(%flag: i1) {
    %c0 = arith.constant 0 : index
    %c2 = arith.constant 2 : index
    %c100 = arith.constant 100 : index
    %c0_i32 = arith.constant 0 : i32
    scf.for %t = %c0 to %c100 step %c2 {
      %e1 = arts.epoch {
        arts.edt <task> <intranode> route(%c0_i32) {
        ^bb0:
          func.call @compute() : () -> ()
          arts.yield
        }
        arts.yield
      } : i64
      scf.if %flag {
        %e2 = arts.epoch {
          arts.edt <task> <intranode> route(%c0_i32) {
          ^bb0:
            func.call @converge_check() : () -> ()
            arts.yield
          }
          arts.yield
        } : i64
      }
    }
    return
  }
}
