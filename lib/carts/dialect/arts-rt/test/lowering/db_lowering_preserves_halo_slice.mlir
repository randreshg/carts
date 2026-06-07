// RUN: %carts-compile %s --pass-pipeline='builtin.module(db-lowering)' | %FileCheck %s

// halo_slice is the committed per-slot window ARTS-RT consumes. DB lowering
// rebuilds acquires in opaque-pointer form and must preserve that fact.

// CHECK-LABEL: func.func @db_lowering_keeps_halo_slice
// CHECK: arts.db_acquire
// CHECK-SAME: halo_slice = #arts.halo_slice<lower = [-1], upper = [1]>
// CHECK-SAME: memref<?x!llvm.ptr>

module attributes {
  arts.runtime_total_nodes = 4 : i64,
  arts.runtime_total_workers = 16 : i64,
  dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i32, dense<32> : vector<2xi64>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi64>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i64>>,
  llvm.data_layout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128",
  llvm.target_triple = "aarch64-unknown-linux-gnu"
} {
  func.func @db_lowering_keeps_halo_slice() {
    %route = arith.constant 0 : i32
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index
    %c16 = arith.constant 16 : index
    %guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <stencil>] route(%route : i32) sizes[%c4] elementType(f64) elementSizes[%c16] {distributed, db_memory_placement = #arts.db_memory_placement<owner_scattered>, owner_block_shape = [16], owner_map_dims = [0], owner_map_kind = #arts.owner_map_kind<owner_dim_contiguous>, owner_map_version = 1 : i32, planOwnerDims = [0], planPhysicalBlockShape = [16]} : (memref<?xi64>, memref<?xmemref<?xf64>>)
    %acq_guid, %acq_ptr = arts.db_acquire[<in>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<?xf64>>) partitioning(<stencil>), indices[%c0], offsets[%c0], sizes[%c1] {halo_slice = #arts.halo_slice<lower = [-1], upper = [1]>, runtime_db_mode = #arts.runtime_db_mode<ro>} -> (memref<?xi64>, memref<?xmemref<?xf64>>)
    return
  }
}
