// RUN: not %carts-compile %s --pass-pipeline='builtin.module(verify-arts-cdag)' 2>&1 | %FileCheck %s

// Two distinct EDTs writing the same distributed DB block inside one epoch are
// concurrent writers of a single-writer block grain: a SWMR violation.

// CHECK: is a second concurrent writer of distributed DB block 0,

module {
  func.func @swmr_violation_two_writers_one_block() {
    %c0 = arith.constant 0 : index
    %c8 = arith.constant 8 : index
    %c16 = arith.constant 16 : index
    %route = arith.constant 0 : i32

    %guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <block>] route(%route : i32) sizes[%c8] elementType(f64) elementSizes[%c16] {distributed, db_memory_placement = #arts.db_memory_placement<owner_scattered>, owner_block_shape = [16], owner_map_dims = [0], owner_map_kind = #arts.owner_map_kind<owner_dim_contiguous>, owner_map_version = 1 : i32, planOwnerDims = [0], planPhysicalBlockShape = [16]} : (memref<?xi64>, memref<?xmemref<?xf64>>)
    arts.epoch {
      %acq_guid0, %acq_ptr0 = arts.db_acquire[<inout>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<?xf64>>) partitioning(<block>), indices[%c0] {runtime_db_mode = #arts.runtime_db_mode<ew>} -> (memref<?xi64>, memref<?xmemref<?xf64>>)
      arts.edt <task> <internode> route(%route) (%acq_ptr0) : memref<?xmemref<?xf64>> {
      ^bb0(%dep0: memref<?xmemref<?xf64>>):
        arts.db_release(%dep0) : memref<?xmemref<?xf64>>
        arts.yield
      }
      %acq_guid1, %acq_ptr1 = arts.db_acquire[<inout>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<?xf64>>) partitioning(<block>), indices[%c0] {runtime_db_mode = #arts.runtime_db_mode<ew>} -> (memref<?xi64>, memref<?xmemref<?xf64>>)
      arts.edt <task> <internode> route(%route) (%acq_ptr1) : memref<?xmemref<?xf64>> {
      ^bb0(%dep1: memref<?xmemref<?xf64>>):
        arts.db_release(%dep1) : memref<?xmemref<?xf64>>
        arts.yield
      }
      arts.yield
    } : i64
    return
  }
}
