// RUN: %carts-compile %s --arts-config %inputs_dir/arts_multinode.cfg --distributed-db --start-from arts-rt-to-llvm --pipeline arts-rt-to-llvm \
// RUN:   | %FileCheck %s

// Rank-2 owner_dim_contiguous maps must lower to the same owner-ranked
// reserve/create discipline as rank-1 maps. The lowered route is computed from
// the 2-D block coordinates, not by falling back to route(-1), node zero, or a
// local-only coarse allocation.

module attributes {
  dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i32, dense<32> : vector<2xi64>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi64>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i64>>,
  llvm.data_layout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128",
  llvm.target_triple = "aarch64-unknown-linux-gnu"
} {
  func.func @distributed_owner_ranked_alloc_2d() {
    %route = arith.constant 0 : i32
    %c1 = arith.constant 1 : index
    %c2 = arith.constant 2 : index
    %c4 = arith.constant 4 : index
    %c8 = arith.constant 8 : index
    %c16 = arith.constant 16 : index
    %guid, %ptr = arts.db_alloc[<out>, <heap>, <write>, <block>] route(%route : i32) sizes[%c2, %c4] elementType(f64) elementSizes[%c8, %c16] {distributed, owner_block_shape = [8, 16], owner_map_dims = [0, 1], owner_map_kind = #arts.owner_map_kind<owner_dim_contiguous>, owner_map_version = 1 : i32, planOwnerDims = [0, 1], planPhysicalBlockShape = [8, 16]} : (memref<?x?xi64>, memref<?x?xmemref<?x?xf64>>)
    return
  }
}

// CHECK-LABEL: func.func private @__carts_dist_alloc_{{[0-9]+}}_worker_init(
// CHECK-SAME: %[[NODE:arg[0-9]+]]: i32, %[[LOCAL:arg[0-9]+]]: i32
// CHECK: %[[ZERO_I32:.*]] = arith.constant 0 : i32
// CHECK: %[[PRIMARY:.*]] = arith.cmpi eq, %[[LOCAL]], %[[ZERO_I32]] : i32
// CHECK: scf.if %[[PRIMARY]]
// CHECK: scf.for %[[BLOCK:arg[0-9]+]] =
// CHECK: %[[ROW:.*]] = arith.divui %[[BLOCK]],
// CHECK: %[[COL:.*]] = arith.remui %[[BLOCK]],
// CHECK: %[[ROW_STRIDE:.*]] = arith.muli %[[ROW]], %{{.*}} : index
// CHECK: %[[OWNER_LINEAR:.*]] = arith.addi %[[ROW_STRIDE]], %[[COL]] : index
// CHECK: %[[SCALED:.*]] = arith.muli %[[OWNER_LINEAR]], %{{.*}} : index
// CHECK: %[[OWNER_IDX:.*]] = arith.divui %[[SCALED]], %{{.*}} : index
// CHECK: %[[OWNER_ROUTE:.*]] = arith.index_cast %[[OWNER_IDX]] : index to i32
// CHECK: %[[OWNS_BLOCK:.*]] = arith.cmpi eq, %[[OWNER_ROUTE]], %[[NODE]] : i32
// CHECK: scf.if %[[OWNS_BLOCK]] {
// CHECK-NOT: }
// CHECK: func.call @arts_db_create_with_guid

// CHECK-LABEL: func.func private @__carts_dist_alloc_{{[0-9]+}}_reserve_init
// CHECK-NOT: arith.constant -1 : i32
// CHECK: scf.for %[[RESERVE_BLOCK:arg[0-9]+]] =
// CHECK: %[[RESERVE_ROW:.*]] = arith.divui %[[RESERVE_BLOCK]],
// CHECK: %[[RESERVE_COL:.*]] = arith.remui %[[RESERVE_BLOCK]],
// CHECK: %[[RESERVE_ROW_STRIDE:.*]] = arith.muli %[[RESERVE_ROW]], %{{.*}} : index
// CHECK: %[[RESERVE_OWNER_LINEAR:.*]] = arith.addi %[[RESERVE_ROW_STRIDE]], %[[RESERVE_COL]] : index
// CHECK: %[[RESERVE_SCALED:.*]] = arith.muli %[[RESERVE_OWNER_LINEAR]], %{{.*}} : index
// CHECK: %[[RESERVE_OWNER_IDX:.*]] = arith.divui %[[RESERVE_SCALED]], %{{.*}} : index
// CHECK: %[[RESERVE_OWNER_ROUTE:.*]] = arith.index_cast %[[RESERVE_OWNER_IDX]] : index to i32
// CHECK-NOT: arith.constant -1 : i32
// CHECK: func.call @arts_guid_reserve(%{{.*}}, %[[RESERVE_OWNER_ROUTE]]) : (i32, i32) -> i64
// CHECK-NOT: func.call @arts_db_create_with_guid
// CHECK: return
