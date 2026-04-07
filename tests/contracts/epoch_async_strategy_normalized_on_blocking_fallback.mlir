// RUN: %carts-compile %s --arts-config %S/../examples/arts.cfg --arts-epoch-finish-continuation --start-from=epochs --pipeline=epochs | %FileCheck %s

// Test that stale structured-plan async-strategy attrs are normalized back to
// blocking when EpochOpt declines CPS-chain formation.
//
// This loop looks like a full-timestep kernel with zero expected local work,
// but it also has sequential tail work after the epoch. The heuristics now
// keep it in blocking form, so any pre-stamped `cps_chain` attrs must be
// rewritten to `blocking` on the loop and the nested epoch/task ops.

// CHECK: arts.plan.async_strategy = "blocking"
// CHECK-NOT: arts.plan.async_strategy = "cps_chain"
// CHECK-NOT: arts.cps_chain_id
// CHECK-NOT: arts.cps_advance

module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f128, dense<128> : vector<2xi64>>, #dlti.dl_entry<f64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i8, dense<[8, 32]> : vector<2xi64>>, #dlti.dl_entry<i64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i16, dense<[16, 32]> : vector<2xi64>>, #dlti.dl_entry<i128, dense<128> : vector<2xi64>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi64>>, #dlti.dl_entry<i32, dense<32> : vector<2xi64>>, #dlti.dl_entry<i1, dense<8> : vector<2xi64>>, #dlti.dl_entry<f16, dense<16> : vector<2xi64>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i64>>, llvm.data_layout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128", llvm.target_triple = "aarch64-unknown-linux-gnu"} {

  func.func private @sidecar() -> ()

  func.func @test_async_strategy_normalized_to_blocking() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c8 = arith.constant 8 : index
    %c0_i32 = arith.constant 0 : i32
    scf.for %t = %c0 to %c8 step %c1 {
      %e = arts.epoch attributes {arts.plan.async_strategy = "cps_chain", arts.plan.cost.expected_local_work = 0 : i64, arts.plan.repetition_structure = "full_timestep"} {
        arts.edt <task> <intranode> route(%c0_i32) attributes {arts.plan.async_strategy = "cps_chain"} {
        ^bb0:
          func.call @sidecar() : () -> ()
          arts.yield
        }
        arts.yield
      } : i64
      func.call @sidecar() : () -> ()
    }
    return
  }
}
