// RUN: %carts-compile %s --arts-config %S/../examples/arts.cfg --arts-epoch-finish-continuation --start-from=epochs --pipeline=pre-lowering | %FileCheck %s --check-prefix=CONT
// RUN: %carts-compile %s --arts-config %S/../examples/arts.cfg --arts-epoch-finish-continuation=false --start-from=epochs --pipeline=pre-lowering | %FileCheck %s --check-prefix=WAIT

// Test that --arts-epoch-finish-continuation replaces blocking epoch waits with
// finish-EDT continuation scheduling for eligible patterns.
//
// Input: arts.epoch with an EDT + tail code (func.call to extern void function).
// The tail is side-effect-only and the return value is defined before the epoch.
//
// With continuation: epoch gets finish operands, no wait_on_epoch.
// CONT: arts.create_epoch finish(
// CONT-NOT: arts.wait_on_epoch

// Without continuation: standard blocking wait path.
// WAIT: arts.create_epoch
// WAIT-NOT: finish(
// WAIT: arts.wait_on_epoch

module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f128, dense<128> : vector<2xi64>>, #dlti.dl_entry<f64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i8, dense<[8, 32]> : vector<2xi64>>, #dlti.dl_entry<i64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i16, dense<[16, 32]> : vector<2xi64>>, #dlti.dl_entry<i128, dense<128> : vector<2xi64>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi64>>, #dlti.dl_entry<i32, dense<32> : vector<2xi64>>, #dlti.dl_entry<i1, dense<8> : vector<2xi64>>, #dlti.dl_entry<f16, dense<16> : vector<2xi64>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i64>>, llvm.data_layout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128", llvm.target_triple = "aarch64-unknown-linux-gnu"} {

  func.func private @sink() -> ()

  func.func @test_continuation() {
    %c0_i32 = arith.constant 0 : i32
    %e = arts.epoch {
      arts.edt <task> <intranode> route(%c0_i32) {
      ^bb0:
        func.call @sink() : () -> ()
        arts.yield
      }
      arts.yield
    } : i64
    // Tail: side-effect-only call (no values flow to func.return).
    func.call @sink() : () -> ()
    return
  }
}
