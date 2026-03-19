// RUN: %carts-compile %s --arts-config %S/../examples/arts.cfg --emit-llvm --pipeline=complete | %FileCheck %s

// CHECK-DAG: declare {{(zeroext )?}}i32 @arts_get_total_workers() #[[RTATTR:[0-9]+]]
// CHECK-DAG: declare noalias ptr @arts_db_create_with_guid(
// CHECK: attributes #[[RTATTR]] = { nofree nosync nounwind willreturn memory(none) }

module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f128, dense<128> : vector<2xi64>>, #dlti.dl_entry<f64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i8, dense<[8, 32]> : vector<2xi64>>, #dlti.dl_entry<i64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i16, dense<[16, 32]> : vector<2xi64>>, #dlti.dl_entry<i128, dense<128> : vector<2xi64>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi64>>, #dlti.dl_entry<i32, dense<32> : vector<2xi64>>, #dlti.dl_entry<i1, dense<8> : vector<2xi64>>, #dlti.dl_entry<f16, dense<16> : vector<2xi64>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i64>>, llvm.data_layout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128", llvm.target_triple = "aarch64-unknown-linux-gnu"} {
  func.func @test_total_workers() -> i32 {
    %w = arts.runtime_query <total_workers> -> i32
    return %w : i32
  }

  func.func @test_db_create_attrs() {
    %c4 = arith.constant 4 : index
    %c16 = arith.constant 16 : index
    %c0_i32 = arith.constant 0 : i32
    %guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <block>, <uniform>] route(%c0_i32 : i32) sizes[%c4] elementType(f64) elementSizes[%c16] : (memref<?xi64>, memref<?xmemref<?xf64>>)
    arts.db_free(%guid) : memref<?xi64>
    arts.db_free(%ptr) : memref<?xmemref<?xf64>>
    return
  }
}
