// RUN: not %carts-compile %s --pass-pipeline='builtin.module(verify-arts-cdag)' \
// RUN:   2>&1 | %FileCheck %s --check-prefix=BEFORE
// RUN: %carts-compile %s --pass-pipeline='builtin.module(db-commit-distributed-deps,verify-arts-cdag)' \
// RUN:   | %FileCheck %s --check-prefix=AFTER

// ARTS -> ARTS-RT boundary rule for partial acquires: a distributed partial
// halo acquire may only reach ARTS-RT once ARTS has committed an authoritative
// DB-space window for it. ARTS-RT must copy that window, never reconstruct the
// halo face slice.
//
// Without the committed window the acquire carries only the stencil access
// extent. verify-arts-cdag then fails closed, so the acquire cannot cross the
// boundary in that state (the first RUN line).
//
// db-commit-distributed-deps authors the per-slot halo_slice from the committed
// stencil access window. verify-arts-cdag then accepts the acquire and the
// committed window crosses the boundary as a fact ARTS-RT consumes (second RUN).

// BEFORE: acquires a partial halo window of a distributed DB without a committed DB-space window

// AFTER-LABEL: func.func @partial_acquire_commits_db_space_window
// AFTER: arts.db_acquire
// AFTER-SAME: halo_slice = #arts.halo_slice<lower = [-1], upper = [1]>

module {
  func.func @partial_acquire_commits_db_space_window() {
    %c0 = arith.constant 0 : index
    %c8 = arith.constant 8 : index
    %c16 = arith.constant 16 : index
    %route = arith.constant 0 : i32

    %guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <stencil>] route(%route : i32) sizes[%c8] elementType(f64) elementSizes[%c16] {distributed, db_memory_placement = #arts.db_memory_placement<owner_scattered>, owner_block_shape = [16], owner_map_dims = [0], owner_map_kind = #arts.owner_map_kind<owner_dim_contiguous>, owner_map_version = 1 : i32, planOwnerDims = [0], planPhysicalBlockShape = [16]} : (memref<?xi64>, memref<?xmemref<?xf64>>)
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
