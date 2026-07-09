// RUN: %carts-compile %s --pass-pipeline='builtin.module(db-lowering,db-distributed-runtime-init)' \
// RUN:   | %FileCheck %s --check-prefix=RT
// RUN: %carts-compile %s --arts-config %inputs_dir/arts_multinode.cfg --start-from=pre-lowering --pipeline=arts-rt-to-llvm \
// RUN:   | %FileCheck %s --check-prefix=LLVM

// Distributed DB init is realized from typed ARTS layout/placement facts before
// LLVM lowering as explicit ARTS-RT runtime ops. The LLVM conversion only
// lowers those ops to ABI calls.

module attributes {
  dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i32, dense<32> : vector<2xi64>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi64>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i64>>,
  llvm.data_layout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128",
  llvm.target_triple = "aarch64-unknown-linux-gnu"
} {
  func.func @distributed_owner_ranked_alloc_from_db_grid() {
    %route = arith.constant 0 : i32
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index
    %guid, %ptr = arts.db_alloc[<out>, <heap>, <write>, <block>] route(%route : i32) sizes[%c4] elementType(f64) elementSizes[%c1] {
      distributed,
      db_placement = #arts.db_placement<distributed>,
      block_layout = #arts.block_layout<
        owner_dims = [0],
        block_shape = [1],
        distribution_kind = <block>>
    } : (memref<?xi64>, memref<?xmemref<?xf64>>)
    return
  }
}

// RT-LABEL: func.func private @__carts_dist_alloc_{{[0-9]+}}_worker_init(
// RT: scf.if
// RT: arts_rt.db_create_with_guid_local
// RT-NOT: func.call @arts_db_create_with_guid(
// RT-LABEL: func.func private @__carts_dist_alloc_{{[0-9]+}}_reserve_init
// RT: arts_rt.db_guid_reserve
// RT: return
// RT-NOT: func.call @arts_guid_reserve(
// RT-NOT: func.call @arts_db_create_with_guid(

// LLVM-NOT: func.func private @arts_db_create_with_guid(
// LLVM-LABEL: func.func private @__carts_dist_alloc_{{[0-9]+}}_worker_init(
// LLVM: func.call @arts_db_create_with_guid_local(
// LLVM-NOT: func.call @arts_db_create_with_guid(
// LLVM-NOT: func.func private @arts_db_create_with_guid(
// LLVM-LABEL: func.func private @__carts_dist_alloc_{{[0-9]+}}_reserve_init
// LLVM: func.call @arts_guid_reserve(
// LLVM-NOT: func.call @arts_db_create_with_guid(
