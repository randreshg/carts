// RUN: %carts-compile %s --pass-pipeline='builtin.module(db-lowering,db-distributed-runtime-init)' \
// RUN:   | %FileCheck %s

// Distributed DB init is realized before LLVM lowering as explicit ARTS-RT
// runtime ops. The LLVM conversion only lowers those ops to ABI calls.

module attributes {
  dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i32, dense<32> : vector<2xi64>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi64>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i64>>,
  llvm.data_layout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128",
  llvm.target_triple = "aarch64-unknown-linux-gnu"
} {
  func.func @distributed_owner_ranked_alloc_from_db_grid() {
    %route = arith.constant 0 : i32
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index
    %guid, %ptr = arts.db_alloc[<out>, <heap>, <write>, <block>] route(%route : i32) sizes[%c4] elementType(f64) elementSizes[%c1] {distributed} : (memref<?xi64>, memref<?xmemref<?xf64>>)
    return
  }
}

// CHECK-LABEL: func.func private @__carts_dist_alloc_{{[0-9]+}}_worker_init(
// CHECK: scf.if
// CHECK: arts_rt.db_create_with_guid_local
// CHECK-NOT: func.call @arts_db_create_with_guid
// CHECK-LABEL: func.func private @__carts_dist_alloc_{{[0-9]+}}_reserve_init
// CHECK: arts_rt.db_guid_reserve
// CHECK: return
// CHECK-NOT: func.call @arts_guid_reserve
// CHECK-NOT: func.call @arts_db_create_with_guid
