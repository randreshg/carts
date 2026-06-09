// RUN: not %carts-compile %s --pass-pipeline='builtin.module(verify-arts-cdag)' 2>&1 \
// RUN:   | %FileCheck %s

// CHECK: element_offsets requires matching element_sizes

module {
  func.func @missing_element_sizes() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index
    %c16 = arith.constant 16 : index
    %route = arith.constant 0 : i32

    %guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <stencil>] route(%route : i32) sizes[%c4] elementType(f64) elementSizes[%c16] : (memref<?xi64>, memref<?xmemref<?xf64>>)
    %acq_guid, %acq_ptr = arts.db_acquire[<in>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<?xf64>>) partitioning(<stencil>), indices[%c0], offsets[%c0], sizes[%c1] element_offsets[%c0] {halo_slice = #arts.halo_slice<lower = [-1], upper = [0]>, runtime_db_mode = #arts.runtime_db_mode<ro>} -> (memref<?xi64>, memref<?xmemref<?xf64>>)

    arts.db_release(%acq_ptr) : memref<?xmemref<?xf64>>
    return
  }
}
