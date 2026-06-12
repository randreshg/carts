// RUN: %carts-compile %s --pass-pipeline='builtin.module(db-commit-distributed-deps)' | %FileCheck %s

// A distributed acquire that carries the block-halo signal (stencil_supported_
// block_halo) and a concrete stencil access extent also gets a committed
// halo_slice, even when its partition mode is not literally `stencil`. The
// slice is diagnostic reach metadata; byte windows remain explicit operands.

// CHECK-LABEL: func.func @distributed_block_halo_acquire_commits_window
// CHECK: arts.db_acquire
// CHECK-SAME: halo_slice = #arts.halo_slice<lower = [-1], upper = [1]>

module {
  func.func @distributed_block_halo_acquire_commits_window() {
    %c0 = arith.constant 0 : index
    %c8 = arith.constant 8 : index
    %c16 = arith.constant 16 : index
    %route = arith.constant 0 : i32

    %guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <block>] route(%route : i32) sizes[%c8] elementType(f64) elementSizes[%c16] {distributed} : (memref<?xi64>, memref<?xmemref<?xf64>>)
    %acq_guid, %acq_ptr = arts.db_acquire[<in>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<?xf64>>) partitioning(<block>), indices[%c0] {stencil_supported_block_halo, stencil_min_offsets = [-1], stencil_max_offsets = [1]} -> (memref<?xi64>, memref<?xmemref<?xf64>>)

    arts.edt <task> <internode> route(%route) (%acq_ptr) : memref<?xmemref<?xf64>> {
    ^bb0(%dep: memref<?xmemref<?xf64>>):
      arts.db_release(%dep) : memref<?xmemref<?xf64>>
      arts.yield
    }
    arts.db_release(%acq_ptr) : memref<?xmemref<?xf64>>
    return
  }
}
