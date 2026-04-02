// RUN: %carts-compile %s --arts-config %S/../examples/arts.cfg --start-from=post-db-refinement --pipeline=post-db-refinement | %FileCheck %s

// CHECK-LABEL: func.func @test_release_only_dep_elimination
// CHECK-NOT: arts.edt <task> <intranode> route(%c0_i32) (%{{.*}}, %{{.*}}, %{{.*}})
// CHECK: arts.edt <task> <intranode> route(%c0_i32) (%{{.*}}, %{{.*}}) : memref<?xmemref<?xf64>>, memref<?xmemref<?xf64>>
// CHECK: ^bb0(%[[IN_ARG:[A-Za-z0-9_]+]]: memref<?xmemref<?xf64>>, %[[OUT_ARG:[A-Za-z0-9_]+]]: memref<?xmemref<?xf64>>):
// CHECK: arts.db_ref %[[IN_ARG]][%c0]
// CHECK: arts.db_ref %[[OUT_ARG]][%c0]

module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i8, dense<[8, 32]> : vector<2xi64>>, #dlti.dl_entry<i64, dense<64> : vector<2xi64>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi64>>, #dlti.dl_entry<i32, dense<32> : vector<2xi64>>, #dlti.dl_entry<i1, dense<8> : vector<2xi64>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i64>>, llvm.data_layout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128", llvm.target_triple = "aarch64-unknown-linux-gnu"} {
  func.func @test_release_only_dep_elimination() {
    %c0_i32 = arith.constant 0 : i32
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index

    %guid_0, %ptr_0 = arts.db_alloc[<inout>, <heap>, <write>, <coarse>, <uniform>] route(%c0_i32 : i32) sizes[%c4] elementType(f64) elementSizes[%c1] : (memref<?xi64>, memref<?xmemref<?xf64>>)
    %guid_1, %ptr_1 = arts.db_alloc[<inout>, <heap>, <write>, <coarse>, <uniform>] route(%c0_i32 : i32) sizes[%c4] elementType(f64) elementSizes[%c1] : (memref<?xi64>, memref<?xmemref<?xf64>>)
    %guid_2, %ptr_2 = arts.db_alloc[<inout>, <heap>, <write>, <coarse>, <uniform>] route(%c0_i32 : i32) sizes[%c4] elementType(f64) elementSizes[%c1] : (memref<?xi64>, memref<?xmemref<?xf64>>)

    %dep_guid_0, %dep_ptr_0 = arts.db_acquire[<in>] (%guid_0 : memref<?xi64>, %ptr_0 : memref<?xmemref<?xf64>>), indices[%c0], offsets[%c0], sizes[%c1] -> (memref<?xi64>, memref<?xmemref<?xf64>>)
    %dep_guid_1, %dep_ptr_1 = arts.db_acquire[<in>] (%guid_1 : memref<?xi64>, %ptr_1 : memref<?xmemref<?xf64>>), indices[%c0], offsets[%c0], sizes[%c1] -> (memref<?xi64>, memref<?xmemref<?xf64>>)
    %dep_guid_2, %dep_ptr_2 = arts.db_acquire[<inout>] (%guid_2 : memref<?xi64>, %ptr_2 : memref<?xmemref<?xf64>>), indices[%c0], offsets[%c0], sizes[%c1] -> (memref<?xi64>, memref<?xmemref<?xf64>>)

    arts.edt <task> <intranode> route(%c0_i32) (%dep_ptr_0, %dep_ptr_1, %dep_ptr_2) : memref<?xmemref<?xf64>>, memref<?xmemref<?xf64>>, memref<?xmemref<?xf64>> attributes {arts.id = 100 : i64, no_verify = #arts.no_verify} {
    ^bb0(%arg0: memref<?xmemref<?xf64>>, %arg1: memref<?xmemref<?xf64>>, %arg2: memref<?xmemref<?xf64>>):
      %src = arts.db_ref %arg0[%c0] : memref<?xmemref<?xf64>> -> memref<?xf64>
      %dead = arts.db_ref %arg1[%c0] : memref<?xmemref<?xf64>> -> memref<?xf64>
      %dst = arts.db_ref %arg2[%c0] : memref<?xmemref<?xf64>> -> memref<?xf64>
      %value = memref.load %src[%c0] : memref<?xf64>
      memref.store %value, %dst[%c0] : memref<?xf64>
      arts.db_release(%arg0) : memref<?xmemref<?xf64>>
      arts.db_release(%arg1) : memref<?xmemref<?xf64>>
      arts.db_release(%arg2) : memref<?xmemref<?xf64>>
      arts.yield
    }

    arts.db_free(%guid_2) : memref<?xi64>
    arts.db_free(%ptr_2) : memref<?xmemref<?xf64>>
    arts.db_free(%guid_1) : memref<?xi64>
    arts.db_free(%ptr_1) : memref<?xmemref<?xf64>>
    arts.db_free(%guid_0) : memref<?xi64>
    arts.db_free(%ptr_0) : memref<?xmemref<?xf64>>
    return
  }
}
