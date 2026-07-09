// RUN: %carts-compile %s --pass-pipeline='builtin.module(db-lowering,db-distributed-runtime-init)' \
// RUN:   | %FileCheck %s

// ARTS-RT consumes typed ARTS ABI facts. The typed block_layout carries the
// block-cyclic owner-route policy; the op intentionally omits the legacy
// `distributed` unit and legacy `distribution_kind` attr.

module attributes {
  arts.runtime_total_nodes = 4 : i64,
  arts.runtime_total_workers = 16 : i64,
  dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i32, dense<32> : vector<2xi64>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi64>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i64>>,
  llvm.data_layout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128",
  llvm.target_triple = "aarch64-unknown-linux-gnu"
} {
  func.func @distributed_runtime_init_consumes_typed_facts() {
    %route = arith.constant 0 : i32
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index
    %guid, %ptr = arts.db_alloc[<out>, <heap>, <write>, <block>]
        route(%route : i32) sizes[%c4] elementType(f64) elementSizes[%c1]
        {
          db_placement = #arts.db_placement<distributed>,
          block_layout = #arts.block_layout<
            owner_dims = [0],
            block_shape = [1],
            distribution_kind = <block_cyclic>>
        }
        : (memref<?xi64>, memref<?xmemref<?xf64>>)
    return
  }
}

// CHECK-LABEL: func.func private @__carts_dist_alloc_{{[0-9]+}}_worker_init(
// CHECK: %[[TOTAL_NODES:.*]] = arts.runtime_query <total_nodes>
// CHECK: %[[TOTAL_NODES_INDEX:.*]] = arith.index_cast %[[TOTAL_NODES]]
// CHECK: scf.for {{.*}} step %[[TOTAL_NODES_INDEX]]
// CHECK: arts_rt.db_create_with_guid_local
// CHECK-LABEL: func.func private @__carts_dist_alloc_{{[0-9]+}}_reserve_init
// CHECK: arts_rt.db_guid_reserve
