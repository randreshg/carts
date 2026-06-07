// RUN: %carts-compile %s --pass-pipeline='builtin.module(db-commit-distributed-deps,verify-arts-cdag)' | %FileCheck %s

// Every distributed acquire must carry a committed runtime DB mode. The commit
// pass records the ordered mode on a distributed acquire that has none, and
// verify-arts-cdag then accepts it.

// CHECK-LABEL: func.func @distributed_acquire_gets_committed_mode
// CHECK: arts.db_acquire
// CHECK-SAME: runtime_db_mode = #arts.runtime_db_mode<ew>

module {
  func.func @distributed_acquire_gets_committed_mode() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c8 = arith.constant 8 : index
    %c16 = arith.constant 16 : index
    %route = arith.constant 0 : i32

    %guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <block>] route(%route : i32) sizes[%c8] elementType(f64) elementSizes[%c16] {distributed, db_memory_placement = #arts.db_memory_placement<owner_scattered>, distribution_kind = #arts.distribution_kind<block>, owner_block_shape = [16], owner_map_dims = [0], owner_map_kind = #arts.owner_map_kind<owner_dim_contiguous>, owner_map_version = 1 : i32, planLogicalWorkerSlice = [16], planOwnerDims = [0], planPhysicalBlockShape = [16]} : (memref<?xi64>, memref<?xmemref<?xf64>>)
    %acq_guid, %acq_ptr = arts.db_acquire[<inout>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<?xf64>>) partitioning(<block>), indices[%c0] -> (memref<?xi64>, memref<?xmemref<?xf64>>)

    arts.edt <task> <internode> route(%route) (%acq_ptr) : memref<?xmemref<?xf64>> {
    ^bb0(%dep: memref<?xmemref<?xf64>>):
      arts.db_release(%dep) : memref<?xmemref<?xf64>>
      arts.yield
    }
    arts.db_release(%acq_ptr) : memref<?xmemref<?xf64>>
    return
  }
}
