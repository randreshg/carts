// RUN: %carts-compile %s --arts-config %S/../examples/arts.cfg --arts-epoch-finish-continuation --start-from=epochs --pipeline=epochs | %FileCheck %s

// Test that CPS dep routing attribute is emitted on arts.cps_advance when
// DB dependencies are captured in the CPS chain loop. Verifies:
//   1. A db_alloc is created for the sidecar memref
//   2. Continuation EDT captures the DB pointer
//   3. arts.cps_dep_routing array appears on the cps_advance op
//   4. The original scf.for loop is eliminated

// CHECK: arts.db_alloc
// CHECK: arts.cps_chain_id = "chain_0"
// CHECK: arts.db_ref
// CHECK: arts.cps_advance
// CHECK: arts.cps_dep_routing = array<i64:
// CHECK-NOT: scf.for

module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f128, dense<128> : vector<2xi64>>, #dlti.dl_entry<f64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i8, dense<[8, 32]> : vector<2xi64>>, #dlti.dl_entry<i64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i16, dense<[16, 32]> : vector<2xi64>>, #dlti.dl_entry<i128, dense<128> : vector<2xi64>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi64>>, #dlti.dl_entry<i32, dense<32> : vector<2xi64>>, #dlti.dl_entry<i1, dense<8> : vector<2xi64>>, #dlti.dl_entry<f16, dense<16> : vector<2xi64>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i64>>, llvm.data_layout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128", llvm.target_triple = "aarch64-unknown-linux-gnu"} {

  func.func private @timer() -> f64
  func.func private @compute() -> ()

  func.func @test_cps_dep_routing_format() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c8 = arith.constant 8 : index
    %c0_i32 = arith.constant 0 : i32
    %timings = memref.alloca() : memref<8xf64>

    scf.for %t = %c0 to %c8 step %c1 {
      %start = func.call @timer() : () -> f64
      memref.store %start, %timings[%t] : memref<8xf64>

      %e = arts.epoch {
        arts.edt <task> <intranode> route(%c0_i32) {
        ^bb0:
          func.call @compute() : () -> ()
          arts.yield
        }
        arts.yield
      } : i64

      %stop = func.call @timer() : () -> f64
      %prev = memref.load %timings[%t] : memref<8xf64>
      %delta = arith.subf %stop, %prev : f64
      memref.store %delta, %timings[%t] : memref<8xf64>
    }

    return
  }
}
