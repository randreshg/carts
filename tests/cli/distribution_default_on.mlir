// Distribution is default-on for multinode configs: staged -O3 compilation
// realizes owner-ranked single-writer DBs with no opt-in flag. The only
// distribution knob is --no-distributed-db, which forces the origin-node
// baseline for debug/comparison.

// RUN: %carts-compile %s --pipeline post-db-refinement --start-from post-db-refinement --arts-config %inputs_dir/arts_multinode.cfg \
// RUN:   | %FileCheck %s --check-prefix=DEFAULT
// RUN: %carts-compile %s --pipeline post-db-refinement --start-from post-db-refinement --arts-config %inputs_dir/arts_multinode.cfg --no-distributed-db \
// RUN:   | %FileCheck %s --check-prefix=BASELINE --implicit-check-not=owner_map_kind --implicit-check-not=owner_scattered

// Default: the block-planned DB is realized as distributed storage with an
// explicit owner map and an owner-scattered home. No flag was passed.
// DEFAULT: arts.db_alloc
// DEFAULT-SAME: db_memory_placement = #arts.db_memory_placement<owner_scattered>
// DEFAULT-SAME: distributed
// DEFAULT-SAME: owner_map_kind = #arts.owner_map_kind<owner_dim_contiguous>
// DEFAULT: arts.edt <task> <internode>

// Baseline: --no-distributed-db keeps the DB on its origin node; the owner map
// is not realized (owner_map_kind / owner_scattered checked absent above).
// BASELINE: arts.db_alloc
// BASELINE-SAME: local_only

module {
  func.func @block_planned_db() {
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
