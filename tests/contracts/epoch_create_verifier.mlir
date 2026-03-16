// RUN: not %carts-compile %s --arts-config %S/../examples/arts.cfg --start-from=arts-to-llvm --stop-at=arts-to-llvm 2>&1 | %FileCheck %s

// Verify that CreateEpochOp rejects mismatched finish operands:
// providing finishEdtGuid without finishSlot must fail verification.

// CHECK: 'arts.create_epoch' op finishEdtGuid and finishSlot must both be present or both be absent

module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f128, dense<128> : vector<2xi32>>, #dlti.dl_entry<f64, dense<64> : vector<2xi32>>, #dlti.dl_entry<i8, dense<[8, 32]> : vector<2xi32>>, #dlti.dl_entry<i64, dense<64> : vector<2xi32>>, #dlti.dl_entry<i16, dense<[16, 32]> : vector<2xi32>>, #dlti.dl_entry<i128, dense<128> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi32>>, #dlti.dl_entry<i32, dense<32> : vector<2xi32>>, #dlti.dl_entry<i1, dense<8> : vector<2xi32>>, #dlti.dl_entry<f16, dense<16> : vector<2xi32>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i32>>, llvm.data_layout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128", llvm.target_triple = "aarch64-unknown-linux-gnu"} {

  // finishEdtGuid present but finishSlot absent -- must fail verification.
  func.func @test_invalid_finish_mismatch(%guid: i64) -> i64 {
    %e = "arts.create_epoch"(%guid) {operandSegmentSizes = array<i32: 1, 0>} : (i64) -> i64
    return %e : i64
  }
}
