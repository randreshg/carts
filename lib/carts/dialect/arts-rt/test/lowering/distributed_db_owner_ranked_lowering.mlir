// RUN: %carts-compile %s --arts-config %inputs_dir/arts_multinode.cfg --start-from arts-rt-to-llvm --pipeline arts-rt-to-llvm \
// RUN:   | %FileCheck %s

// Canonical must lower distributed DB allocation as
// owner-ranked storage: reserve every GUID on its computed owner rank, and let
// worker init create only the DBs owned by the local node.

module attributes {
  dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i32, dense<32> : vector<2xi64>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi64>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i64>>,
  llvm.data_layout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128",
  llvm.target_triple = "aarch64-unknown-linux-gnu"
} {
  func.func @distributed_owner_ranked_alloc() {
    %route = arith.constant 0 : i32
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index
    %guid, %ptr = arts.db_alloc[<out>, <heap>, <write>, <block>] route(%route : i32) sizes[%c4] elementType(f64) elementSizes[%c1] {distributed, owner_block_shape = [1], owner_map_dims = [0], owner_map_kind = #arts.owner_map_kind<owner_dim_contiguous>, owner_map_version = 1 : i32, planOwnerDims = [0], planPhysicalBlockShape = [1]} : (memref<?xi64>, memref<?xmemref<?xf64>>)
    return
  }
}

// CHECK-LABEL: func.func private @__carts_dist_alloc_{{[0-9]+}}_worker_init(
// CHECK-SAME: %[[NODE:arg[0-9]+]]: i32, %[[LOCAL:arg[0-9]+]]: i32
// CHECK: %[[ZERO_I32:.*]] = arith.constant 0 : i32
// CHECK: %[[PRIMARY:.*]] = arith.cmpi eq, %[[LOCAL]], %[[ZERO_I32]] : i32
// CHECK: scf.if %[[PRIMARY]]
// CHECK: scf.for %[[BLOCK:arg[0-9]+]] =
// CHECK: %[[SCALED:.*]] = arith.muli %[[BLOCK]], %{{.*}} : index
// CHECK: %[[OWNER_IDX:.*]] = arith.divui %[[SCALED]], %{{.*}} : index
// CHECK: %[[OWNER_ROUTE:.*]] = arith.index_cast %[[OWNER_IDX]] : index to i32
// CHECK: %[[OWNS_BLOCK:.*]] = arith.cmpi eq, %[[OWNER_ROUTE]], %[[NODE]] : i32
// CHECK: scf.if %[[OWNS_BLOCK]] {
// CHECK-NOT: }
// CHECK: func.call @arts_db_create_with_guid

// CHECK-LABEL: func.func private @__carts_dist_alloc_{{[0-9]+}}_reserve_init
// CHECK-NOT: arith.constant -1 : i32
// CHECK: scf.for %[[RESERVE_BLOCK:arg[0-9]+]] =
// CHECK: %[[RESERVE_SCALED:.*]] = arith.muli %[[RESERVE_BLOCK]], %{{.*}} : index
// CHECK: %[[RESERVE_OWNER_IDX:.*]] = arith.divui %[[RESERVE_SCALED]], %{{.*}} : index
// CHECK: %[[RESERVE_OWNER_ROUTE:.*]] = arith.index_cast %[[RESERVE_OWNER_IDX]] : index to i32
// CHECK-NOT: arith.constant -1 : i32
// CHECK: func.call @arts_guid_reserve(%{{.*}}, %[[RESERVE_OWNER_ROUTE]]) : (i32, i32) -> i64
// CHECK-NOT: func.call @arts_db_create_with_guid
// CHECK: return
