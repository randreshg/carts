// RUN: %carts-compile %s --arts-config %S/../examples/arts.cfg --arts-epoch-finish-continuation --start-from=epochs --pipeline=epochs | %FileCheck %s

// Baseline: a nested irregular inner loop (no epoch) is NOT converted
// to an async CPS chain. Only the outer timestep loop gets CPS conversion.
// Verifies:
//   1. Outer timestep loop produces CPS chain
//   2. Inner loop does NOT get cps_chain_id

// The outer loop gets a CPS chain.
// CHECK: arts.cps_chain_id
// The inner loop remains as a regular scf.for (not converted to CPS).
// CHECK: scf.for
// Only one cps_chain_id should exist (for the outer loop).
// CHECK-NOT: arts.cps_chain_id

module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f128, dense<128> : vector<2xi64>>, #dlti.dl_entry<f64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i8, dense<[8, 32]> : vector<2xi64>>, #dlti.dl_entry<i64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i16, dense<[16, 32]> : vector<2xi64>>, #dlti.dl_entry<i128, dense<128> : vector<2xi64>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi64>>, #dlti.dl_entry<i32, dense<32> : vector<2xi64>>, #dlti.dl_entry<i1, dense<8> : vector<2xi64>>, #dlti.dl_entry<f16, dense<16> : vector<2xi64>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i64>>, llvm.data_layout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128", llvm.target_triple = "aarch64-unknown-linux-gnu"} {

  func.func private @irregular_work(index) -> ()

  func.func @test_persistent_region_nested_no_async() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c50 = arith.constant 50 : index
    %c10 = arith.constant 10 : index
    %c0_i32 = arith.constant 0 : i32
    scf.for %t = %c0 to %c50 step %c1 {
      %e = arts.epoch {
        arts.edt <task> <intranode> route(%c0_i32) {
        ^bb0:
          scf.for %i = %c0 to %c10 step %c1 {
            func.call @irregular_work(%i) : (index) -> ()
          }
          arts.yield
        }
        arts.yield
      } : i64
    }
    return
  }
}
