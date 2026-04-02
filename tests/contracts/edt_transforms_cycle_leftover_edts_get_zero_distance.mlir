// RUN: %carts-compile %s --arts-config %S/../examples/arts.cfg --start-from=post-db-refinement --pipeline=post-db-refinement | %FileCheck %s

// ET-6 documents that cyclic leftover EDTs fall back to
// critical_path_distance = 0. This loop creates a loop-carried cycle between
// the same two task EDT ops via inout acquires on a shared DB, so neither EDT
// reaches Kahn's topo worklist.
//
// CHECK-LABEL: func.func @cyclic_leftover_edts_get_zero_distance
// CHECK-DAG: arts.lowering_contract({{.*}}) contract(<postDbRefined = true, criticalPathDistance = 0 : i64>)
// CHECK-DAG: arts.lowering_contract({{.*}}) contract(<postDbRefined = true, criticalPathDistance = 0 : i64>)
// CHECK-DAG: arts.edt <task> <intranode> route(%c0_i32) (%{{.*}}) : memref<?xmemref<?xf64>> attributes {arts.id = 1 : i64, {{.*}}critical_path_distance = 0 : i64
// CHECK-DAG: arts.edt <task> <intranode> route(%c0_i32) (%{{.*}}) : memref<?xmemref<?xf64>> attributes {arts.id = 2 : i64, {{.*}}critical_path_distance = 0 : i64

module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i8, dense<[8, 32]> : vector<2xi64>>, #dlti.dl_entry<i64, dense<64> : vector<2xi64>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi64>>, #dlti.dl_entry<i32, dense<32> : vector<2xi64>>, #dlti.dl_entry<i1, dense<8> : vector<2xi64>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i64>>, llvm.data_layout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128", llvm.target_triple = "aarch64-unknown-linux-gnu"} {
  func.func @cyclic_leftover_edts_get_zero_distance() {
    %c0_i32 = arith.constant 0 : i32
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c2 = arith.constant 2 : index
    %c4 = arith.constant 4 : index
    %c1_f64 = arith.constant 1.000000e+00 : f64

    %guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <coarse>, <uniform>] route(%c0_i32 : i32) sizes[%c4] elementType(f64) elementSizes[%c1] : (memref<?xi64>, memref<?xmemref<?xf64>>)

    scf.for %iv = %c0 to %c2 step %c1 {
      %dep_guid_0, %dep_ptr_0 = arts.db_acquire[<inout>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<?xf64>>), indices[%c0], offsets[%c0], sizes[%c1] -> (memref<?xi64>, memref<?xmemref<?xf64>>)
      arts.edt <task> <intranode> route(%c0_i32) (%dep_ptr_0) : memref<?xmemref<?xf64>> attributes {arts.id = 1 : i64, no_verify = #arts.no_verify} {
      ^bb0(%arg0: memref<?xmemref<?xf64>>):
        %view0 = arts.db_ref %arg0[%c0] : memref<?xmemref<?xf64>> -> memref<?xf64>
        %value0 = memref.load %view0[%c0] : memref<?xf64>
        %sum0 = arith.addf %value0, %c1_f64 : f64
        memref.store %sum0, %view0[%c0] : memref<?xf64>
        arts.db_release(%arg0) : memref<?xmemref<?xf64>>
        arts.yield
      }

      %dep_guid_1, %dep_ptr_1 = arts.db_acquire[<inout>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<?xf64>>), indices[%c0], offsets[%c0], sizes[%c1] -> (memref<?xi64>, memref<?xmemref<?xf64>>)
      arts.edt <task> <intranode> route(%c0_i32) (%dep_ptr_1) : memref<?xmemref<?xf64>> attributes {arts.id = 2 : i64, no_verify = #arts.no_verify} {
      ^bb0(%arg1: memref<?xmemref<?xf64>>):
        %view1 = arts.db_ref %arg1[%c0] : memref<?xmemref<?xf64>> -> memref<?xf64>
        %value1 = memref.load %view1[%c0] : memref<?xf64>
        %sum1 = arith.addf %value1, %c1_f64 : f64
        memref.store %sum1, %view1[%c0] : memref<?xf64>
        arts.db_release(%arg1) : memref<?xmemref<?xf64>>
        arts.yield
      }
    }

    arts.db_free(%guid) : memref<?xi64>
    arts.db_free(%ptr) : memref<?xmemref<?xf64>>
    return
  }
}
