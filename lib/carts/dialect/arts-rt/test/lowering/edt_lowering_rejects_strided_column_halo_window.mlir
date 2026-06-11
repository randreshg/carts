// RUN: not %carts-compile %s --pass-pipeline='builtin.module(edt-lowering)' 2>&1 \
// RUN:   | %FileCheck %s

// CHECK: per-block halo read dependency has an explicit element window that is not representable as a contiguous byte slice

module attributes {
  arts.runtime_total_nodes = 4 : i64,
  arts.runtime_total_workers = 16 : i64,
  dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i32, dense<32> : vector<2xi64>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi64>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i64>>,
  llvm.data_layout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128",
  llvm.target_triple = "aarch64-unknown-linux-gnu"
} {
  func.func @edt_lowering_rejects_strided_column_halo_window() {
    %route = arith.constant 0 : i32
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index
    %c32 = arith.constant 32 : index

    %guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <block>] route(%route : i32) sizes[%c4, %c4] elementType(f64) elementSizes[%c1, %c1, %c32, %c32] {distributed, db_memory_placement = #arts.db_memory_placement<owner_scattered>, owner_block_shape = [1, 1], owner_map_dims = [0, 1], owner_map_kind = #arts.owner_map_kind<owner_dim_contiguous>, owner_map_version = 1 : i32, planOwnerDims = [0, 1], planPhysicalBlockShape = [1, 1, 32, 32]} : (memref<?x?xi64>, memref<?x?xmemref<?x?x?x?xf64>>)
    %acq_guid, %acq_ptr = arts.db_acquire[<in>] (%guid : memref<?x?xi64>, %ptr : memref<?x?xmemref<?x?x?x?xf64>>) partitioning(<block>), indices[], offsets[%c0, %c0], sizes[%c1, %c1] element_offsets[%c0, %c0, %c0, %c0] element_sizes[%c1, %c1, %c32, %c1] {haloViewDependency, runtime_db_mode = #arts.runtime_db_mode<ro>, stencil_max_offsets = [0, 1], stencil_min_offsets = [0, 0], stencil_owner_dims = [0, 1], stencil_spatial_dims = [0, 1], stencil_supported_block_halo} -> (memref<?x?xi64>, memref<?x?xmemref<?x?x?x?xf64>>)

    arts.edt <task> <internode> route(%route) (%acq_ptr) : memref<?x?xmemref<?x?x?x?xf64>> {
    ^bb0(%dep: memref<?x?xmemref<?x?x?x?xf64>>):
      arts.db_release(%dep) : memref<?x?xmemref<?x?x?x?xf64>>
      arts.yield
    }

    arts.db_release(%acq_ptr) : memref<?x?xmemref<?x?x?x?xf64>>
    return
  }
}
