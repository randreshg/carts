// RUN: %carts-compile %s --arts-config %S/../examples/arts.cfg --arts-epoch-finish-continuation --start-from=epochs --pipeline=epochs | %FileCheck %s

// STREAM-like timed loops carry sequential sidecars between epochs. The CPS
// chain transform must preserve those sidecars by promoting loop-external
// memref storage to DB-backed views that continuations can capture safely.
//
// CHECK-LABEL: func.func @test_cps_chain_sidecar_memref_promotion
// CHECK: arts.db_alloc[<inout>, <heap>, <write>, <coarse>] route(%c0_i32 : i32) sizes[%c1] elementType(f64) elementSizes[%c4] : (memref<?xi64>, memref<?xmemref<4xf64>>)
// CHECK: arts.cps_chain_id = "chain_0"
// CHECK: arts.db_ref %{{.*}}[%c0] : memref<?xmemref<4xf64>> -> memref<4xf64>
// CHECK: arts.cps_advance
// CHECK-NOT: scf.for

module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f128, dense<128> : vector<2xi64>>, #dlti.dl_entry<f64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i8, dense<[8, 32]> : vector<2xi64>>, #dlti.dl_entry<i64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i16, dense<[16, 32]> : vector<2xi64>>, #dlti.dl_entry<i128, dense<128> : vector<2xi64>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi64>>, #dlti.dl_entry<i32, dense<32> : vector<2xi64>>, #dlti.dl_entry<i1, dense<8> : vector<2xi64>>, #dlti.dl_entry<f16, dense<16> : vector<2xi64>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i64>>, llvm.data_layout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128", llvm.target_triple = "aarch64-unknown-linux-gnu"} {

  func.func private @now() -> f64
  func.func private @sink_a() -> ()
  func.func private @sink_b() -> ()

  func.func @test_cps_chain_sidecar_memref_promotion() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index
    %c0_i32 = arith.constant 0 : i32
    %timings = memref.alloca() : memref<4xf64>

    scf.for %t = %c0 to %c4 step %c1 {
      %start = func.call @now() : () -> f64
      memref.store %start, %timings[%t] : memref<4xf64>

      %e0 = arts.epoch {
        arts.edt <task> <intranode> route(%c0_i32) {
        ^bb0:
          func.call @sink_a() : () -> ()
          arts.yield
        }
        arts.yield
      } : i64

      %e1 = arts.epoch {
        arts.edt <task> <intranode> route(%c0_i32) {
        ^bb0:
          func.call @sink_b() : () -> ()
          arts.yield
        }
        arts.yield
      } : i64

      %stop = func.call @now() : () -> f64
      %prev = memref.load %timings[%t] : memref<4xf64>
      %delta = arith.subf %stop, %prev : f64
      memref.store %delta, %timings[%t] : memref<4xf64>
    }

    return
  }
}
