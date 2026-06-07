// RUN: %carts-compile %s --pipeline post-db-refinement --start-from post-db-refinement --arts-config %inputs_dir/arts_multinode.cfg | %FileCheck %s

// A block-planned DB used by an internode task is partitioned SDE structure.
// ARTS must realize it through explicit owner-map and memory-placement facts:
// the owner map (kind/dims/block shape) plus an owner_scattered home. The block
// grain itself stays readable as the DB shape (sizes = block grid,
// owner_block_shape = per-block extent); none of it is left implicit.

// Attributes print in alphabetical order, so the checks follow that order.
// CHECK-LABEL: func.func @block_planned_db_realizes_owner_map_and_placement
// CHECK: arts.db_alloc
// CHECK-SAME: <block>
// CHECK-SAME: db_memory_placement = #arts.db_memory_placement<owner_scattered>
// CHECK-SAME: distributed
// CHECK-SAME: owner_block_shape = [16]
// CHECK-SAME: owner_map_dims = [0]
// CHECK-SAME: owner_map_kind = #arts.owner_map_kind<owner_dim_contiguous>
// CHECK-SAME: owner_map_version = 1 : i32
// CHECK: arts.edt <task> <internode>

module {
  func.func @block_planned_db_realizes_owner_map_and_placement() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c8 = arith.constant 8 : index
    %c16 = arith.constant 16 : index
    %route = arith.constant 0 : i32
    %value = arith.constant 1.0 : f64

    %guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <block>] route(%route : i32) sizes[%c8] elementType(f64) elementSizes[%c16] {distribution_kind = #arts.distribution_kind<block>, planLogicalWorkerSlice = [16], planOwnerDims = [0], planPhysicalBlockShape = [16]} : (memref<?xi64>, memref<?xmemref<?xf64>>)
    %acq_guid, %acq_ptr = arts.db_acquire[<inout>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<?xf64>>) partitioning(<block>), indices[], offsets[%c0], sizes[%c1] -> (memref<?xi64>, memref<?xmemref<?xf64>>)

    arts.edt <task> <internode> route(%route) (%acq_ptr) : memref<?xmemref<?xf64>> attributes {distribution_kind = #arts.distribution_kind<block>, planLogicalWorkerSlice = [16], planOwnerDims = [0], planPhysicalBlockShape = [16]} {
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
