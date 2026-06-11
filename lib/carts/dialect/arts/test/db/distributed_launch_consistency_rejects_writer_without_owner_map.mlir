// RUN: not %carts-compile %s --pass-pipeline='builtin.module(distributed-launch-consistency)' 2>&1 | %FileCheck %s

// A distributed writer with ambiguous partition evidence cannot be treated as
// "no distributed writer"; ARTS must fail closed instead of leaving the route.

// CHECK: writes distributed DBs whose owner route cannot be derived from committed DB owner-map facts

module attributes {arts.runtime_total_nodes = 2 : i64, arts.runtime_total_workers = 8 : i64} {
  func.func @unroutable_writer_partition_rejected() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index
    %route = arith.constant -1 : i32
    %value = arith.constant 1.0 : f64

    %guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <block>] route(%route : i32) sizes[%c4] elementType(f64) elementSizes[%c4] {distributed, db_memory_placement = #arts.db_memory_placement<owner_scattered>, owner_block_shape = [4], owner_map_dims = [0], owner_map_kind = #arts.owner_map_kind<owner_dim_contiguous>, owner_map_version = 1 : i32, planOwnerDims = [0], planPhysicalBlockShape = [4]} : (memref<?xi64>, memref<?xmemref<?xf64>>)
    %acq_guid, %acq_ptr = arts.db_acquire[<out>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<?xf64>>) partitioning(<block>, indices[], offsets[%c0, %c4], sizes[%c1, %c1]), indices[], offsets[], sizes[%c1] {partition_offsets_segments = array<i32: 1, 1>, partition_sizes_segments = array<i32: 1, 1>} -> (memref<?xi64>, memref<?xmemref<?xf64>>)

    arts.edt <task> <intranode> route(%route) (%acq_ptr) : memref<?xmemref<?xf64>> {
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
