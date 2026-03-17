// RUN: %carts-compile %s --arts-config %S/../examples/arts.cfg --start-from=arts-to-llvm --pipeline=arts-to-llvm | %FileCheck %s

// CHECK-LABEL: func.func @guid_range_db_alloc
// CHECK: call @arts_guid_reserve_range(
// CHECK: call @arts_guid_from_index(
// CHECK-NOT: call @arts_guid_reserve(
//
// CHECK-LABEL: func.func @guid_linear_fallback_db_alloc
// CHECK: %[[TOTAL:.*]] = arith.muli %arg0, %arg1 : index
// CHECK: scf.for %{{.*}} = %{{.*}} to %[[TOTAL]] step %{{.*}} {
// CHECK:   {{(func[.])?call}} @arts_guid_reserve(
// CHECK-NOT: scf.for
// CHECK: return

module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f128, dense<128> : vector<2xi32>>, #dlti.dl_entry<f64, dense<64> : vector<2xi32>>, #dlti.dl_entry<i8, dense<[8, 32]> : vector<2xi32>>, #dlti.dl_entry<i64, dense<64> : vector<2xi32>>, #dlti.dl_entry<i16, dense<[16, 32]> : vector<2xi32>>, #dlti.dl_entry<i128, dense<128> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi32>>, #dlti.dl_entry<i32, dense<32> : vector<2xi32>>, #dlti.dl_entry<i1, dense<8> : vector<2xi32>>, #dlti.dl_entry<f16, dense<16> : vector<2xi32>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i32>>, llvm.data_layout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128", llvm.target_triple = "aarch64-unknown-linux-gnu"} {
  func.func @guid_range_db_alloc() {
    %c0 = arith.constant 0 : index
    %c4 = arith.constant 4 : index
    %c16 = arith.constant 16 : index
    %c0_i32 = arith.constant 0 : i32
    %guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <block>, <uniform>] route(%c0_i32 : i32) sizes[%c4] elementType(f64) elementSizes[%c16] : (memref<?xi64>, memref<?xmemref<?xf64>>)
    %g0 = memref.load %guid[%c0] : memref<?xi64>
    %p0 = memref.load %ptr[%c0] : memref<?xmemref<?xf64>>
    %dim0 = memref.dim %p0, %c0 : memref<?xf64>
    %g0_idx = arith.index_cast %g0 : i64 to index
    %sum = arith.addi %dim0, %g0_idx : index
    arts.db_free(%guid) : memref<?xi64>
    arts.db_free(%ptr) : memref<?xmemref<?xf64>>
    %unused = arith.cmpi eq, %sum, %c0 : index
    return
  }

  func.func @guid_linear_fallback_db_alloc(%N: index, %M: index) {
    %c1 = arith.constant 1 : index
    %c1_i32 = arith.constant 1 : i32
    %guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <block>, <uniform>] route(%c1_i32 : i32) sizes[%N, %M] elementType(f64) elementSizes[%c1] : (memref<?xi64>, memref<?xmemref<?xf64>>)
    arts.db_free(%guid) : memref<?xi64>
    arts.db_free(%ptr) : memref<?xmemref<?xf64>>
    return
  }
}
