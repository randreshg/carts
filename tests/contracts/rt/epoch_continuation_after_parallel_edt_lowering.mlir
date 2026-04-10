// RUN: %carts-compile %s --arts-config %S/../../examples/arts.cfg --arts-epoch-finish-continuation --start-from=epochs --pipeline=pre-lowering | %FileCheck %s --check-prefix=ON
// RUN: %carts-compile %s --arts-config %S/../../examples/arts.cfg --arts-epoch-finish-continuation=false --start-from=epochs --pipeline=pre-lowering | %FileCheck %s --check-prefix=OFF

// Verifies that continuation prep also runs after ParallelEdtLowering.
// ParallelEdtLowering introduces a fresh arts.epoch here. With continuation
// enabled, the late-created epoch should still be converted to finish-EDT form.

// ON: arts_rt.create_epoch finish(
// ON-NOT: arts_rt.wait_on_epoch

// OFF: arts_rt.create_epoch
// OFF-NOT: finish(
// OFF: arts_rt.wait_on_epoch

module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f128, dense<128> : vector<2xi64>>, #dlti.dl_entry<f64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i8, dense<[8, 32]> : vector<2xi64>>, #dlti.dl_entry<i64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i16, dense<[16, 32]> : vector<2xi64>>, #dlti.dl_entry<i128, dense<128> : vector<2xi64>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi64>>, #dlti.dl_entry<i32, dense<32> : vector<2xi64>>, #dlti.dl_entry<i1, dense<8> : vector<2xi64>>, #dlti.dl_entry<f16, dense<16> : vector<2xi64>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i64>>, llvm.data_layout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128", llvm.target_triple = "aarch64-unknown-linux-gnu"} {

  func.func private @sink() -> ()

  func.func @test_parallel_edt_epoch_tail() {
    %c0_i32 = arith.constant 0 : i32
    arts.edt <parallel> <intranode> route(%c0_i32) {
      func.call @sink() : () -> ()
      arts.yield
    }
    func.call @sink() : () -> ()
    return
  }
}
