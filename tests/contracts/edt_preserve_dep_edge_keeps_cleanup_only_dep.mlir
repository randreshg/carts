// RUN: %carts-compile %s --arts-config %S/../examples/arts.cfg --start-from=concurrency-opt --pipeline=concurrency-opt | %FileCheck %s

// CHECK-LABEL: func.func @test_preserve_dep_edge_keeps_cleanup_only_dep
// CHECK: arts.edt <task> <intranode> route(%c0_i32) (%{{.*}}, %{{.*}})
// CHECK: ^bb0(%[[KEEP:.*]]: memref<?xmemref<?xf64>>, %[[LIVE:.*]]: memref<?xmemref<?xf64>>):
// CHECK: arts.db_ref %[[LIVE]][%c0]
// CHECK: arts.db_release(%[[KEEP]])

module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i8, dense<[8, 32]> : vector<2xi64>>, #dlti.dl_entry<i64, dense<64> : vector<2xi64>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi64>>, #dlti.dl_entry<i32, dense<32> : vector<2xi64>>, #dlti.dl_entry<i1, dense<8> : vector<2xi64>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i64>>, llvm.data_layout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128", llvm.target_triple = "aarch64-unknown-linux-gnu"} {
  func.func @test_preserve_dep_edge_keeps_cleanup_only_dep() {
    %c0_i32 = arith.constant 0 : i32
    %c0 = arith.constant 0 : index
    %c0_f64 = arith.constant 0.0 : f64
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index

    %guid_0, %ptr_0 = arts.db_alloc[<inout>, <heap>, <write>, <coarse>, <uniform>] route(%c0_i32 : i32) sizes[%c4] elementType(f64) elementSizes[%c1] : (memref<?xi64>, memref<?xmemref<?xf64>>)
    %guid_1, %ptr_1 = arts.db_alloc[<inout>, <heap>, <write>, <coarse>, <uniform>] route(%c0_i32 : i32) sizes[%c4] elementType(f64) elementSizes[%c1] : (memref<?xi64>, memref<?xmemref<?xf64>>)

    %dep_guid_0, %dep_ptr_0 = arts.db_acquire[<in>] (%guid_0 : memref<?xi64>, %ptr_0 : memref<?xmemref<?xf64>>), indices[%c0], offsets[%c0], sizes[%c1] {preserve_dep_edge = #arts.preserve_dep_edge} -> (memref<?xi64>, memref<?xmemref<?xf64>>)
    %dep_guid_1, %dep_ptr_1 = arts.db_acquire[<inout>] (%guid_1 : memref<?xi64>, %ptr_1 : memref<?xmemref<?xf64>>), indices[%c0], offsets[%c0], sizes[%c1] -> (memref<?xi64>, memref<?xmemref<?xf64>>)

    arts.edt <task> <intranode> route(%c0_i32) (%dep_ptr_0, %dep_ptr_1) : memref<?xmemref<?xf64>>, memref<?xmemref<?xf64>> attributes {arts.id = 101 : i64, no_verify = #arts.no_verify} {
    ^bb0(%arg0: memref<?xmemref<?xf64>>, %arg1: memref<?xmemref<?xf64>>):
      %dst = arts.db_ref %arg1[%c0] : memref<?xmemref<?xf64>> -> memref<?xf64>
      memref.store %c0_f64, %dst[%c0] : memref<?xf64>
      arts.db_release(%arg0) : memref<?xmemref<?xf64>>
      arts.db_release(%arg1) : memref<?xmemref<?xf64>>
      arts.yield
    }

    arts.db_free(%guid_1) : memref<?xi64>
    arts.db_free(%ptr_1) : memref<?xmemref<?xf64>>
    arts.db_free(%guid_0) : memref<?xi64>
    arts.db_free(%ptr_0) : memref<?xmemref<?xf64>>
    return
  }
}
