// RUN: %carts-compile %s --arts-config %S/../examples/arts.cfg --start-from=epochs --pipeline=epochs | %FileCheck %s --check-prefix=ON
// RUN: %carts-compile %s --arts-config %S/../examples/arts.cfg --arts-epoch-finish-continuation=false --start-from=epochs --pipeline=epochs | %FileCheck %s --check-prefix=OFF

// Two consecutive independent epochs are fusible by access alone. When the
// second epoch is a profitable continuation candidate, the default-on
// continuation policy should preserve that boundary. If continuation is
// explicitly disabled, the same pair should fuse.
//
// ON-LABEL: func.func @test_epoch_fusion_respects_continuation_candidate
// ON-COUNT-2: arts.epoch
// ON: arts.continuation_for_epoch
//
// OFF-LABEL: func.func @test_epoch_fusion_respects_continuation_candidate
// OFF-COUNT-1: arts.epoch
// OFF-NOT: arts.continuation_for_epoch

module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f128, dense<128> : vector<2xi64>>, #dlti.dl_entry<f64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i8, dense<[8, 32]> : vector<2xi64>>, #dlti.dl_entry<i64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i16, dense<[16, 32]> : vector<2xi64>>, #dlti.dl_entry<i128, dense<128> : vector<2xi64>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi64>>, #dlti.dl_entry<i32, dense<32> : vector<2xi64>>, #dlti.dl_entry<i1, dense<8> : vector<2xi64>>, #dlti.dl_entry<f16, dense<16> : vector<2xi64>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i64>>, llvm.data_layout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128", llvm.target_triple = "aarch64-unknown-linux-gnu"} {

  func.func private @first() -> ()
  func.func private @second() -> ()
  func.func private @tail() -> ()

  func.func @test_epoch_fusion_respects_continuation_candidate() {
    %c0_i32 = arith.constant 0 : i32

    %e0 = arts.epoch {
      arts.edt <task> <intranode> route(%c0_i32) {
      ^bb0:
        func.call @first() : () -> ()
        arts.yield
      }
      arts.yield
    } : i64

    %e1 = arts.epoch {
      arts.edt <task> <intranode> route(%c0_i32) {
      ^bb0:
        func.call @second() : () -> ()
        arts.yield
      }
      arts.yield
    } : i64

    func.call @tail() : () -> ()
    return
  }
}
