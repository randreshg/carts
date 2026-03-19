// RUN: %carts-compile %s --arts-config %S/../examples/arts.cfg --start-from=arts-to-llvm --pipeline=arts-to-llvm | %FileCheck %s --check-prefix=NO-FINISH
// RUN: %carts-compile %s --arts-config %S/../examples/arts.cfg --start-from=arts-to-llvm --pipeline=arts-to-llvm | %FileCheck %s --check-prefix=WITH-FINISH

// Verify CreateEpochOp lowering to arts_initialize_and_start_epoch runtime call
// in both modes: without finish target (blocking epoch) and with finish target
// (continuation-style epoch).

// --- Without finish operands: hardcoded constants (guid=0, slot=0) ---
// NO-FINISH-LABEL: func @test_no_finish
// NO-FINISH:         %[[GUID:.*]] = arith.constant 0 : i64
// NO-FINISH:         %[[SLOT:.*]] = arith.constant 0 : i32
// NO-FINISH:         call @arts_initialize_and_start_epoch(%[[GUID]], %[[SLOT]])
// NO-FINISH:         call @arts_wait_on_handle

// --- With finish operands: function args passed through ---
// WITH-FINISH-LABEL: func @test_with_finish
// WITH-FINISH-SAME:  (%[[ARG_GUID:.*]]: i64, %[[ARG_SLOT:.*]]: i32)
// WITH-FINISH:         call @arts_initialize_and_start_epoch(%[[ARG_GUID]], %[[ARG_SLOT]])
// WITH-FINISH-NOT:     call @arts_wait_on_handle

module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f128, dense<128> : vector<2xi64>>, #dlti.dl_entry<f64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i8, dense<[8, 32]> : vector<2xi64>>, #dlti.dl_entry<i64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i16, dense<[16, 32]> : vector<2xi64>>, #dlti.dl_entry<i128, dense<128> : vector<2xi64>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi64>>, #dlti.dl_entry<i32, dense<32> : vector<2xi64>>, #dlti.dl_entry<i1, dense<8> : vector<2xi64>>, #dlti.dl_entry<f16, dense<16> : vector<2xi64>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i64>>, llvm.data_layout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128", llvm.target_triple = "aarch64-unknown-linux-gnu"} {

  // Test 1: CreateEpochOp without finish target (regression test).
  // Should lower to arts_initialize_and_start_epoch(0, 0).
  func.func @test_no_finish() -> i64 {
    %e = arts.create_epoch : i64
    arts.wait_on_epoch %e : i64
    return %e : i64
  }

  // Test 2: CreateEpochOp with finish target.
  // Should lower to arts_initialize_and_start_epoch(guid, slot).
  func.func @test_with_finish(%guid: i64, %slot: i32) -> i64 {
    %e = arts.create_epoch finish(%guid : i64, %slot : i32) : i64
    return %e : i64
  }
}
