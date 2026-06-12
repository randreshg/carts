// RUN: not %carts-compile %s --pass-pipeline='builtin.module(verify-arts-cdag)' \
// RUN:   2>&1 | %FileCheck %s --check-prefix=BEFORE
// RUN: not %carts-compile %s --pass-pipeline='builtin.module(db-commit-distributed-deps,verify-arts-cdag)' \
// RUN:   2>&1 | %FileCheck %s --check-prefix=AFTER

// ARTS -> ARTS-RT boundary rule for partial acquires: a distributed partial
// halo acquire may only reach ARTS-RT with explicit element_offsets and
// element_sizes. ARTS-RT must copy that byte window, never reconstruct the halo
// face slice from metadata.
//
// Without explicit element windows the acquire carries only the stencil access
// extent. verify-arts-cdag fails closed, so the acquire cannot cross the
// boundary in that state.
//
// db-commit-distributed-deps authors the per-slot halo_slice from the committed
// stencil access window, but halo_slice is only diagnostic stencil-reach
// metadata. It is still rejected without element_offsets/element_sizes.

// BEFORE: acquires a partial halo window of a distributed DB without explicit element_offsets/element_sizes

// AFTER: acquires a partial halo window of a distributed DB without explicit element_offsets/element_sizes
// AFTER-LABEL: func.func @partial_acquire_commits_db_space_window
// AFTER: arts.db_acquire
// AFTER-SAME: halo_slice = #arts.halo_slice<lower = [-1], upper = [1]>

module {
  func.func @partial_acquire_commits_db_space_window() {
    %c0 = arith.constant 0 : index
    %c8 = arith.constant 8 : index
    %c16 = arith.constant 16 : index
    %route = arith.constant 0 : i32

    %guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <stencil>] route(%route : i32) sizes[%c8] elementType(f64) elementSizes[%c16] {distributed} : (memref<?xi64>, memref<?xmemref<?xf64>>)
    %acq_guid, %acq_ptr = arts.db_acquire[<in>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<?xf64>>) partitioning(<stencil>), indices[%c0] {runtime_db_mode = #arts.runtime_db_mode<ro>, stencil_min_offsets = [-1], stencil_max_offsets = [1]} -> (memref<?xi64>, memref<?xmemref<?xf64>>)

    arts.edt <task> <internode> route(%route) (%acq_ptr) : memref<?xmemref<?xf64>> {
    ^bb0(%dep: memref<?xmemref<?xf64>>):
      arts.db_release(%dep) : memref<?xmemref<?xf64>>
      arts.yield
    }
    arts.db_release(%acq_ptr) : memref<?xmemref<?xf64>>
    return
  }
}
