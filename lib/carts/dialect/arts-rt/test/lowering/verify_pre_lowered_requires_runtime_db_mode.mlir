// RUN: not %carts-compile %s --pass-pipeline='builtin.module(verify-pre-lowered)' 2>&1 | %FileCheck %s

// The RO/EW/RW verdict on a distributed acquire is an ARTS policy decision.
// ARTS-RT maps the committed verdict to a runtime constant and must reject an
// acquire of a distributed DB that arrives without one.

module {
  func.func @distributed_acquire_without_runtime_db_mode() {
    %route = arith.constant 0 : i32
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index
    %guid, %ptr = arts.db_alloc[<out>, <heap>, <write>, <block>] route(%route : i32) sizes[%c4] elementType(f64) elementSizes[%c1] {distributed, owner_block_shape = [1], owner_map_dims = [0], owner_map_kind = #arts.owner_map_kind<owner_dim_contiguous>, owner_map_version = 1 : i32, planOwnerDims = [0], planPhysicalBlockShape = [1]} : (memref<?xi64>, memref<?xmemref<?xf64>>)
    // CHECK: error: 'arts.db_acquire' op distributed DB acquire reached ABI lowering without a committed runtime DB mode; ARTS-RT must not infer it
    %acq_guid, %acq_ptr = arts.db_acquire[<in>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<?xf64>>) partitioning(<block>), indices[], offsets[%c0], sizes[%c1] -> (memref<?xi64>, memref<?xmemref<?xf64>>)
    return
  }
}
