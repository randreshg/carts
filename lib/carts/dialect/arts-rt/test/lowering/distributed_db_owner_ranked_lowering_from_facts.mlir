// RUN: %carts-compile %s --arts-config %inputs_dir/arts_multinode.cfg --start-from arts-rt-to-llvm --pipeline arts-rt-to-llvm \
// RUN:   | %FileCheck %s

// ARTS-RT lowering consumes the upstream distributed owner-map facts on the DB
// op. It must not require the CLI flag to be threaded back into this stage, and
// it must not fall back to single-node DB creation when the runtime config is
// multi-node.

module attributes {
  dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i32, dense<32> : vector<2xi64>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi64>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i64>>,
  llvm.data_layout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128",
  llvm.target_triple = "aarch64-unknown-linux-gnu"
} {
  func.func @distributed_owner_ranked_alloc_from_facts() {
    %route = arith.constant 0 : i32
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index
    %guid, %ptr = arts.db_alloc[<out>, <heap>, <write>, <block>] route(%route : i32) sizes[%c4] elementType(f64) elementSizes[%c1] {distributed, owner_block_shape = [1], owner_map_dims = [0], owner_map_kind = #arts.owner_map_kind<owner_dim_contiguous>, owner_map_version = 1 : i32, planOwnerDims = [0], planPhysicalBlockShape = [1]} : (memref<?xi64>, memref<?xmemref<?xf64>>)
    return
  }
}

// CHECK-LABEL: func.func private @__carts_dist_alloc_{{[0-9]+}}_worker_init(
// CHECK: scf.if
// CHECK: func.call @arts_db_create_with_guid

// CHECK-LABEL: func.func private @__carts_dist_alloc_{{[0-9]+}}_reserve_init
// CHECK: func.call @arts_guid_reserve
// CHECK-NOT: func.call @arts_db_create_with_guid
// CHECK: return
