// RUN: not %carts-compile %s --pass-pipeline='builtin.module(edt-lowering)' 2>&1 \
// RUN:   | %FileCheck %s

// CHECK: per-block halo read dependency requires an explicit provably nonzero byte window

module attributes {
  arts.runtime_total_nodes = 1 : i64,
  arts.runtime_total_workers = 1 : i64,
  dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i32, dense<32> : vector<2xi64>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi64>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i64>>,
  llvm.data_layout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128",
  llvm.target_triple = "aarch64-unknown-linux-gnu"
} {
  func.func @edt_lowering_rejects_zero_element_halo_window() {
    %route = arith.constant 0 : i32
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index
    %c16 = arith.constant 16 : index

    %guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <stencil>] route(%route : i32) sizes[%c4] elementType(f64) elementSizes[%c16] {db_memory_placement = #arts.db_memory_placement<owner_scattered>, owner_block_shape = [16], owner_map_dims = [0], owner_map_kind = #arts.owner_map_kind<owner_dim_contiguous>, owner_map_version = 1 : i32, planOwnerDims = [0], planPhysicalBlockShape = [16]} : (memref<?xi64>, memref<?xmemref<?xf64>>)
    %acq_guid, %acq_ptr = arts.db_acquire[<in>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<?xf64>>) partitioning(<stencil>), indices[%c0], offsets[%c0], sizes[%c1] element_offsets[%c0] element_sizes[%c0] {haloViewDependency, halo_slice = #arts.halo_slice<lower = [-1], upper = [0]>, runtime_db_mode = #arts.runtime_db_mode<ro>, stencil_max_offsets = [0], stencil_min_offsets = [-1], stencil_supported_block_halo} -> (memref<?xi64>, memref<?xmemref<?xf64>>)

    arts.edt <task> <intranode> route(%route) (%acq_ptr) : memref<?xmemref<?xf64>> {
    ^bb0(%dep: memref<?xmemref<?xf64>>):
      arts.db_release(%dep) : memref<?xmemref<?xf64>>
      arts.yield
    }

    arts.db_release(%acq_ptr) : memref<?xmemref<?xf64>>
    return
  }
}
