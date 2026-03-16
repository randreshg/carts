// RUN: %carts-compile %s --arts-config %S/../examples/arts.cfg --arts-epoch-finish-continuation --start-from=epochs --stop-at=pre-lowering | %FileCheck %s

// Test that --arts-epoch-finish-continuation falls back to blocking wait
// when the tail of an epoch contains another epoch (Rule 6 rejects this).

// CHECK: arts.wait_on_epoch
// CHECK: arts.wait_on_epoch
// CHECK-NOT: finish(

module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f128, dense<128> : vector<2xi32>>, #dlti.dl_entry<f64, dense<64> : vector<2xi32>>, #dlti.dl_entry<i8, dense<[8, 32]> : vector<2xi32>>, #dlti.dl_entry<i64, dense<64> : vector<2xi32>>, #dlti.dl_entry<i16, dense<[16, 32]> : vector<2xi32>>, #dlti.dl_entry<i128, dense<128> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi32>>, #dlti.dl_entry<i32, dense<32> : vector<2xi32>>, #dlti.dl_entry<i1, dense<8> : vector<2xi32>>, #dlti.dl_entry<f16, dense<16> : vector<2xi32>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i32>>, llvm.data_layout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128", llvm.target_triple = "aarch64-unknown-linux-gnu"} {

  func.func private @sink() -> ()

  func.func @test_nested_fallback() {
    %c0_i32 = arith.constant 0 : i32
    %e1 = arts.epoch {
      arts.edt <task> <intranode> route(%c0_i32) {
      ^bb0:
        func.call @sink() : () -> ()
        arts.yield
      }
      arts.yield
    } : i64
    // Tail contains another epoch -- Rule 6 rejects this
    %e2 = arts.epoch {
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
