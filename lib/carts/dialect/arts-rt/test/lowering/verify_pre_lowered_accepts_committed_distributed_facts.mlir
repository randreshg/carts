// RUN: %carts-compile %s --pass-pipeline='builtin.module(verify-pre-lowered)' | %FileCheck %s

// When the committed owner-map plan and runtime DB mode are present, the gate
// passes the IR through unchanged: ARTS-RT consumes these facts, it does not
// recompute them.

// CHECK-LABEL: func.func @distributed_facts_committed
// CHECK: arts.db_alloc
// CHECK-SAME: distributed
// CHECK-SAME: owner_map_kind = #arts.owner_map_kind<owner_dim_contiguous>
// CHECK: arts.db_acquire
// CHECK-SAME: runtime_db_mode = #arts.runtime_db_mode<ew>

module {
  func.func @distributed_facts_committed() {
    %route = arith.constant 0 : i32
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index
    %guid, %ptr = arts.db_alloc[<out>, <heap>, <write>, <block>] route(%route : i32) sizes[%c4] elementType(f64) elementSizes[%c1] {distributed, owner_block_shape = [1], owner_map_dims = [0], owner_map_kind = #arts.owner_map_kind<owner_dim_contiguous>, owner_map_version = 1 : i32, planOwnerDims = [0], planPhysicalBlockShape = [1]} : (memref<?xi64>, memref<?xmemref<?xf64>>)
    %acq_guid, %acq_ptr = arts.db_acquire[<in>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<?xf64>>) partitioning(<block>), indices[], offsets[%c0], sizes[%c1] {runtime_db_mode = #arts.runtime_db_mode<ew>} -> (memref<?xi64>, memref<?xmemref<?xf64>>)
    return
  }
}
