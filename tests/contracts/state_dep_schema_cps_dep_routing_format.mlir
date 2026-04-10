// RUN: %carts-compile %s --arts-config %S/../examples/arts.cfg --arts-epoch-finish-continuation --start-from=epochs --pipeline=epochs | %FileCheck %s

// Loops with host-call sidecars currently stay blocking in the epochs
// pipeline, so no CPS dep-routing metadata or sidecar DB promotion is emitted.
//
// CHECK-LABEL: func.func @test_cps_dep_routing_format
// CHECK-NOT: arts.db_alloc
// CHECK-NOT: arts.cps_chain_id
// CHECK-NOT: arts.cps_dep_routing
// CHECK: scf.for %{{.+}} = %c0 to %c8 step %c1
// CHECK: func.call @timer() : () -> f64
// CHECK: arts.epoch
// CHECK: arts.edt <task> <intranode> route(%c0_i32)
// CHECK: func.call @timer() : () -> f64

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
