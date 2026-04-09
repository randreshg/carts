// RUN: %carts-compile %s --arts-config %S/../examples/arts.cfg --emit-llvm --pipeline=complete | %FileCheck %s

// Verify that ESD deps with dep_flags emit arts_add_dependence_at_ex (7-arg variant).
// CHECK-DAG: declare void @arts_add_dependence_at_ex(i64, i64, i32 zeroext, i32 signext, i64, i64, i32 zeroext)
// CHECK: call void @arts_add_dependence_at_ex({{.*}}i32 1)

module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f128, dense<128> : vector<2xi64>>, #dlti.dl_entry<f64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i8, dense<[8, 32]> : vector<2xi64>>, #dlti.dl_entry<i64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i16, dense<[16, 32]> : vector<2xi64>>, #dlti.dl_entry<i128, dense<128> : vector<2xi64>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi64>>, #dlti.dl_entry<i32, dense<32> : vector<2xi64>>, #dlti.dl_entry<i1, dense<8> : vector<2xi64>>, #dlti.dl_entry<f16, dense<16> : vector<2xi64>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i64>>, llvm.data_layout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128", llvm.target_triple = "aarch64-unknown-linux-gnu"} {
  func.func @test() {
    %c0_i32 = arith.constant 0 : i32
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c2 = arith.constant 2 : index
    %c64 = arith.constant 64 : index
    %edt = arith.constant 7 : i64
    %guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <block>, <uniform>] route(%c0_i32 : i32) sizes[%c2] elementType(f64) elementSizes[%c1] {distributed, arts.read_only_after_init} : (memref<?xi64>, memref<?xmemref<?xf64>>)
    %dep_guid, %dep_ptr = arts.db_acquire[<in>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<?xf64>>), indices[%c0], offsets[%c0], sizes[%c1] -> (memref<?xi64>, memref<?xmemref<?xf64>>)
    arts_rt.rec_dep %edt(%dep_guid : memref<?xi64>) byte_offsets(%c0) byte_sizes(%c64) {acquire_modes = array<i32: 1>, dep_flags = array<i32: 1>}
    return
  }
}
