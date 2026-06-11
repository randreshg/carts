// RUN: not %carts-compile %s --pass-pipeline='builtin.module(realize-edt-distribution-plan)' 2>&1 | %FileCheck %s

// CHECK: realizes a per-block halo dependency without explicit element_offsets/element_sizes

module {
  func.func @rejects_missing_halo_window() {
    %route = arith.constant -1 : i32
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index
    %c16 = arith.constant 16 : index
    %value = arith.constant 0.0 : f64

    %guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <block>] route(%route : i32) sizes[%c4] elementType(f64) elementSizes[%c16] {planOwnerDims = [0], planPhysicalBlockShape = [16]} : (memref<?xi64>, memref<?xmemref<?xf64>>)
    %acq_guid, %acq_ptr = arts.db_acquire[<in>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<?xf64>>) partitioning(<block>), indices[], offsets[%c0], sizes[%c1] {depPattern = #arts.dep_pattern<stencil>, distribution_pattern = #arts.distribution_pattern<stencil>, stencil_max_offsets = [1], stencil_min_offsets = [-1], stencil_supported_block_halo} -> (memref<?xi64>, memref<?xmemref<?xf64>>)

    arts.edt <task> <intranode> route(%route) (%acq_ptr) : memref<?xmemref<?xf64>> attributes {depPattern = #arts.dep_pattern<stencil>, planHaloShape = [1], stencil_max_offsets = [1], stencil_min_offsets = [-1]} {
    ^bb0(%dep: memref<?xmemref<?xf64>>):
      %payload = arts.db_ref %dep[%c0] : memref<?xmemref<?xf64>> -> memref<?xf64>
      memref.store %value, %payload[%c0] : memref<?xf64>
      arts.db_release(%dep) : memref<?xmemref<?xf64>>
      arts.yield
    }

    arts.db_release(%acq_ptr) : memref<?xmemref<?xf64>>
    return
  }
}
