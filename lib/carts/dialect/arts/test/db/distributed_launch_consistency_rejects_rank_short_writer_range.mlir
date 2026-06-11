// RUN: not %carts-compile %s --pass-pipeline='builtin.module(distributed-launch-consistency)' 2>&1 | %FileCheck %s

// Missing owner-dim size evidence is not owner-local. A rank-short writer
// acquire may cover multiple owner blocks and must fail closed.

// CHECK: writes a distributed DB range that may span multiple owners

module attributes {arts.runtime_total_nodes = 2 : i64, arts.runtime_total_workers = 8 : i64} {
  func.func @rank_short_writer_range_rejected() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index
    %route = arith.constant -1 : i32
    %value = arith.constant 1.0 : f64

    %guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <block>] route(%route : i32) sizes[%c4, %c4] elementType(f64) elementSizes[%c4, %c4] {distributed, db_memory_placement = #arts.db_memory_placement<owner_scattered>, owner_block_shape = [4, 4], owner_map_dims = [0, 1], owner_map_kind = #arts.owner_map_kind<owner_dim_contiguous>, owner_map_version = 1 : i32, planOwnerDims = [0, 1], planPhysicalBlockShape = [4, 4]} : (memref<?x?xi64>, memref<?x?xmemref<?x?xf64>>)
    %acq_guid, %acq_ptr = arts.db_acquire[<out>] (%guid : memref<?x?xi64>, %ptr : memref<?x?xmemref<?x?xf64>>) partitioning(<block>), indices[%c0], offsets[%c0], sizes[%c1] -> (memref<?xi64>, memref<?xmemref<?x?xf64>>)

    arts.edt <task> <intranode> route(%route) (%acq_ptr) : memref<?xmemref<?x?xf64>> {
    ^bb0(%dep: memref<?xmemref<?x?xf64>>):
      %payload = arts.db_ref %dep[%c0] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
      memref.store %value, %payload[%c0, %c0] : memref<?x?xf64>
      arts.db_release(%dep) : memref<?xmemref<?x?xf64>>
      arts.yield
    }

    arts.db_release(%acq_ptr) : memref<?xmemref<?x?xf64>>
    return
  }
}
