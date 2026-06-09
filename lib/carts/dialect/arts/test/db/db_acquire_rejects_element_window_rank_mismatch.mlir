// RUN: not %carts-compile %s --pass-pipeline='builtin.module(verify-arts-cdag)' 2>&1 \
// RUN:   | %FileCheck %s

// CHECK: element window rank (1) must match source element rank (2)

module {
  func.func @element_window_rank_mismatch() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index
    %c8 = arith.constant 8 : index
    %c16 = arith.constant 16 : index
    %route = arith.constant 0 : i32

    %guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <stencil>] route(%route : i32) sizes[%c4, %c4] elementType(f64) elementSizes[%c8, %c16] : (memref<?x?xi64>, memref<?x?xmemref<?x?xf64>>)
    %acq_guid, %acq_ptr = arts.db_acquire[<in>] (%guid : memref<?x?xi64>, %ptr : memref<?x?xmemref<?x?xf64>>) partitioning(<stencil>), indices[%c0, %c0], offsets[%c0, %c0], sizes[%c1, %c1] element_offsets[%c0] element_sizes[%c1] {halo_slice = #arts.halo_slice<lower = [-1, 0], upper = [0, 0]>, runtime_db_mode = #arts.runtime_db_mode<ro>} -> (memref<?x?xi64>, memref<?x?xmemref<?x?xf64>>)

    arts.db_release(%acq_ptr) : memref<?x?xmemref<?x?xf64>>
    return
  }
}
