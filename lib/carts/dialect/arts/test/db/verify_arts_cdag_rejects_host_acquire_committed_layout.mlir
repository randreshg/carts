// RUN: not %carts-compile %s --pass-pipeline='builtin.module(verify-arts-cdag)' 2>&1 | %FileCheck %s

// A committed block-layout DB cannot be observed through a host-side acquire.
// The observation must be explicit ARTS graph work, or the verifier rejects it.

// CHECK: observes a committed SDE block-layout DB through a non-EDT/non-cleanup use

module {
  func.func @host_acquire_committed_layout_rejected() {
    %c0 = arith.constant 0 : index
    %c4 = arith.constant 4 : index
    %c16 = arith.constant 16 : index
    %route = arith.constant -1 : i32

    %guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <block>] route(%route : i32) sizes[%c4] elementType(f64) elementSizes[%c16] {distributed, db_memory_placement = #arts.db_memory_placement<owner_scattered>, owner_block_shape = [16], owner_map_dims = [0], owner_map_kind = #arts.owner_map_kind<owner_dim_contiguous>, owner_map_version = 1 : i32, planOwnerDims = [0], planPhysicalBlockShape = [16]} : (memref<?xi64>, memref<?xmemref<?xf64>>)
    %acq_guid, %acq_ptr = arts.db_acquire[<in>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<?xf64>>) partitioning(<block>), indices[], offsets[%c0], sizes[%c4] {runtime_db_mode = #arts.runtime_db_mode<ro>} -> (memref<?xi64>, memref<?xmemref<?xf64>>)
    %payload = arts.db_ref %acq_ptr[%c0] : memref<?xmemref<?xf64>> -> memref<?xf64>
    %value = memref.load %payload[%c0] : memref<?xf64>
    func.call @observe(%value) : (f64) -> ()
    arts.db_release(%acq_ptr) : memref<?xmemref<?xf64>>
    return
  }

  func.func private @observe(f64)
}
