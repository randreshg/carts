// RUN: %carts-compile %s --pass-pipeline='builtin.module(reduction-planning,storage-planning)' \
// RUN:   | %FileCheck %s

// First-class CODIR collective dispatch: StoragePlanning stamps a
// per-dep `dep_collectives` array via `chooseCollective`, the name-free
// extraction of the historical all-gather / cross-owner-reduce gate bodies.
//   - the producer whose output is consumed by a contraction-axis
//     `replicated_read` sibling selects `all_gather`;
//   - the cross-owner transpose-reduce step (atax/bicg A^T) selects
//     `reduce_scatter`;
//   - an iterative full-timestep stencil write selects `halo`;
//   - an in-place safe readwrite halo stencil selects `halo` and block storage;
//   - owner-aligned deps select `none`.

module {
  // all_gather: %F is written by the first codelet and read as
  // `replicated_read` by the sibling codelet in the same module (shared root).
  func.func @all_gather(%C: memref<1024x1024xf64>, %D: memref<1024x1024xf64>,
                        %E: memref<1024x1024xf64>, %F: memref<1024x1024xf64>,
                        %G: memref<1024x1024xf64>, %base: index) {
    %c4 = arith.constant 4 : index
    %c1024 = arith.constant 1024 : index
    scf.for %i = %base to %c1024 step %c4 {
      codir.codelet deps(%F, %C, %D : memref<1024x1024xf64>, memref<1024x1024xf64>, memref<1024x1024xf64>)
          params(%i : index)
          attributes {dep_modes = [#codir.access_mode<readwrite>, #codir.access_mode<read>, #codir.access_mode<read>],
                      dep_storage_views = [#codir.storage_view<compute_block>, #codir.storage_view<compute_block>, #codir.storage_view<replicated_read>],
                      pattern = #codir.pattern<matmul>,
                      tile_owner_dims = [0],
                      tile_shape = [4, 1024]} {
      ^bb0(%f: memref<1024x1024xf64>, %c: memref<1024x1024xf64>, %d: memref<1024x1024xf64>, %owner: index):
        codir.yield
      }
    }
    scf.for %i = %base to %c1024 step %c4 {
      codir.codelet deps(%G, %E, %F : memref<1024x1024xf64>, memref<1024x1024xf64>, memref<1024x1024xf64>)
          params(%i : index)
          attributes {dep_modes = [#codir.access_mode<readwrite>, #codir.access_mode<read>, #codir.access_mode<read>],
                      dep_storage_views = [#codir.storage_view<compute_block>, #codir.storage_view<compute_block>, #codir.storage_view<replicated_read>],
                      pattern = #codir.pattern<matmul>,
                      tile_owner_dims = [0],
                      tile_shape = [4, 1024]} {
      ^bb0(%g: memref<1024x1024xf64>, %e: memref<1024x1024xf64>, %f: memref<1024x1024xf64>, %owner: index):
        codir.yield
      }
    }
    return
  }

  // reduce_scatter: cross-owner transpose-reduce (atax y = A^T(Ax)); the matrix
  // dep is mapped `[-1, ownerDim]` (leading row dim reduces, trailing dim owns).
  func.func @reduce_scatter(%y: memref<32768xf32>, %A: memref<1024x32768xf32>,
                            %base: index) {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c16 = arith.constant 16 : index
    %c1024 = arith.constant 1024 : index
    %c32768 = arith.constant 32768 : index
    scf.for %i = %base to %c32768 step %c16 {
      codir.codelet deps(%y, %A : memref<32768xf32>, memref<1024x32768xf32>)
          params(%i : index)
          attributes {dep_modes = [#codir.access_mode<readwrite>, #codir.access_mode<read>],
                      dep_storage_views = [#codir.storage_view<compute_block>, #codir.storage_view<compute_block>],
                      partial_reduction,
                      partial_reduction_dims = [1],
                      partial_reduction_owner_dims = [0],
                      tile_owner_dims = [0],
                      tile_shape = [16]} {
      ^bb0(%arg0: memref<32768xf32>, %arg1: memref<1024x32768xf32>, %owner: index):
        %inner_c0 = arith.constant 0 : index
        %inner_c1 = arith.constant 1 : index
        %inner_c1024 = arith.constant 1024 : index
        %old = memref.load %arg0[%owner] : memref<32768xf32>
        scf.for %j = %inner_c0 to %inner_c1024 step %inner_c1 {
          %a = memref.load %arg1[%j, %owner] : memref<1024x32768xf32>
          %next = arith.addf %old, %a : f32
          memref.store %next, %arg0[%owner] : memref<32768xf32>
        }
        codir.yield
      }
    }
    return
  }

  // halo: an iterative full-timestep stencil producer writes the same
  // block-distributed buffer it reads with a halo. StoragePlanning stamps the
  // first-class halo carrier so ARTS can realize nearest-neighbor block exchange.
  func.func @halo_collective(%A: memref<1024x1024xf64>, %base: index) {
    %c16 = arith.constant 16 : index
    %c1023 = arith.constant 1023 : index
    scf.for %i = %base to %c1023 step %c16 {
      codir.codelet deps(%A : memref<1024x1024xf64>)
          params(%i : index)
          attributes {access_max_offsets = [1, 0],
                      access_min_offsets = [-1, 0],
                      dep_modes = [#codir.access_mode<readwrite>],
                      dep_storage_views = [#codir.storage_view<phase_redistributed>],
                      iteration_topology = #codir.iteration_topology<owner_strip>,
                      pattern = #codir.pattern<stencil_tiling_nd>,
                      repetition_structure = #codir.repetition_structure<full_timestep>,
                      tile_owner_dims = [0],
                      tile_shape = [16, 1024],
                      write_footprint = [1, 1024]} {
      ^bb0(%arg0: memref<1024x1024xf64>, %owner: index):
        %inner_c1 = arith.constant 1 : index
        %row_p = arith.addi %owner, %inner_c1 : index
        %row_m = arith.subi %owner, %inner_c1 : index
        %v0 = memref.load %arg0[%owner, %owner] : memref<1024x1024xf64>
        %v1 = memref.load %arg0[%row_p, %owner] : memref<1024x1024xf64>
        %v2 = memref.load %arg0[%row_m, %owner] : memref<1024x1024xf64>
        %s0 = arith.addf %v0, %v1 : f64
        %s1 = arith.addf %s0, %v2 : f64
        memref.store %s1, %arg0[%owner, %owner] : memref<1024x1024xf64>
        codir.yield
      }
    }
    return
  }

  // A readwrite in-place stencil is both the writer and the halo reader of the
  // same state. CODIR must keep it block-native and name the halo collective so
  // ARTS can realize a per-block single-writer DB instead of a coarse host DB.
  func.func @inplace_safe_readwrite_halo_storage(%A: memref<1024x1024xf64>,
                                                 %base_i: index,
                                                 %base_j: index) {
    codir.codelet deps(%A : memref<1024x1024xf64>)
        params(%base_i, %base_j : index, index)
        attributes {access_max_offsets = [1, 1],
                    access_min_offsets = [0, 0],
                    dep_modes = [#codir.access_mode<readwrite>],
                    dep_storage_views = [#codir.storage_view<host_whole>],
                    in_place_safe,
                    iteration_topology = #codir.iteration_topology<owner_tile>,
                    pattern = #codir.pattern<stencil_tiling_nd>,
                    tile_owner_dims = [0, 1],
                    tile_shape = [16, 32],
                    write_footprint = [1, 1]} {
    ^bb0(%arg0: memref<1024x1024xf64>, %i: index, %j: index):
      %inner_c1 = arith.constant 1 : index
      %ip1 = arith.addi %i, %inner_c1 : index
      %jp1 = arith.addi %j, %inner_c1 : index
      %center = memref.load %arg0[%i, %j] : memref<1024x1024xf64>
      %edge = memref.load %arg0[%ip1, %jp1] : memref<1024x1024xf64>
      %sum = arith.addf %center, %edge : f64
      memref.store %sum, %arg0[%i, %j] : memref<1024x1024xf64>
      codir.yield
    }
    return
  }

  // The in-place-safe shortcut cannot name a halo collective unless the same
  // write-footprint/tile-shape legality used by storage planning accepts block
  // storage. This catches metadata-only collective promises over host_whole.
  func.func @inplace_safe_halo_rejects_oversized_write(%A: memref<1024x1024xf64>,
                                                       %base_i: index,
                                                       %base_j: index) {
    codir.codelet deps(%A : memref<1024x1024xf64>)
        params(%base_i, %base_j : index, index)
        attributes {access_max_offsets = [1, 1],
                    access_min_offsets = [0, 0],
                    dep_modes = [#codir.access_mode<readwrite>],
                    dep_storage_views = [#codir.storage_view<host_whole>],
                    in_place_safe,
                    iteration_topology = #codir.iteration_topology<owner_tile>,
                    pattern = #codir.pattern<stencil_tiling_nd>,
                    tile_owner_dims = [0, 1],
                    tile_shape = [16, 32],
                    write_footprint = [17, 1]} {
    ^bb0(%arg0: memref<1024x1024xf64>, %i: index, %j: index):
      %inner_c1 = arith.constant 1 : index
      %ip1 = arith.addi %i, %inner_c1 : index
      %jp1 = arith.addi %j, %inner_c1 : index
      %center = memref.load %arg0[%i, %j] : memref<1024x1024xf64>
      %edge = memref.load %arg0[%ip1, %jp1] : memref<1024x1024xf64>
      %sum = arith.addf %center, %edge : f64
      memref.store %sum, %arg0[%i, %j] : memref<1024x1024xf64>
      codir.yield
    }
    return
  }

  // Generic layout-mismatch redistribution: no matmul benchmark shape and no
  // consumer-name heuristic. CODIR derives the all_gather family from neutral
  // layout evidence on the dependency while preserving the upstream layout and
  // grouping facts verbatim.
  func.func @generic_layout_mismatch_all_gather(%tmp: memref<128xf32>) {
    codir.codelet deps(%tmp : memref<128xf32>)
        attributes {array_layout = [{arrayId = 7 : i64,
                                     blockShape = [16],
                                     commVolumeBytes = 4096 : i64,
                                     kind = "block_contraction",
                                     muBlockCount = 8 : i64,
                                     ownerDims = [0],
                                     role = "write"}],
                    dep_modes = [#codir.access_mode<write>],
                    dep_storage_views = [#codir.storage_view<phase_redistributed>],
                    layouts_disagree = [7],
                    partition_graph = [{cuGroupCount = 2 : i64,
                                        cuGroupSize = 4 : i64,
                                        edgeClass = "layout_mismatch",
                                        edgeCommBytes = 4096 : i64,
                                        layoutKind = "block_contraction",
                                        muBlockCount = 8 : i64,
                                        role = "write"}],
                    partition_score = {chosenCuCount = 8 : i64,
                                       cuGroupCount = 2 : i64,
                                       cuGroupSize = 4 : i64,
                                       exposedCuCount = 8 : i64,
                                       muBlockCount = 8 : i64,
                                       targetLogicalWorkers = 8 : i64},
                    pattern = #codir.pattern<elementwise_pipeline>} {
    ^bb0(%arg0: memref<128xf32>):
      codir.yield
    }
    return
  }

  // The same neutral layout-mismatch evidence selects reduce_scatter when the
  // compute pattern is reduction-like. This proves the derivation is based on
  // compute family plus layout mismatch, not a benchmark name.
  func.func @generic_layout_mismatch_reduce_scatter(%partial: memref<128xf32>) {
    codir.codelet deps(%partial : memref<128xf32>)
        attributes {array_layout = [{arrayId = 11 : i64,
                                     blockShape = [16],
                                     commVolumeBytes = 8192 : i64,
                                     kind = "block_contraction",
                                     muBlockCount = 8 : i64,
                                     ownerDims = [0],
                                     role = "write"}],
                    dep_modes = [#codir.access_mode<readwrite>],
                    dep_storage_views = [#codir.storage_view<phase_redistributed>],
                    layouts_disagree = [11],
                    partial_reduction,
                    partition_graph = [{cuGroupCount = 2 : i64,
                                        cuGroupSize = 4 : i64,
                                        edgeClass = "layout_mismatch",
                                        edgeCommBytes = 8192 : i64,
                                        layoutKind = "block_contraction",
                                        muBlockCount = 8 : i64,
                                        role = "write"}],
                    partition_score = {chosenCuCount = 8 : i64,
                                       cuGroupCount = 2 : i64,
                                       cuGroupSize = 4 : i64,
                                       exposedCuCount = 8 : i64,
                                       muBlockCount = 8 : i64,
                                       targetLogicalWorkers = 8 : i64},
                    pattern = #codir.pattern<reduction>} {
    ^bb0(%arg0: memref<128xf32>):
      codir.yield
    }
    return
  }
}

// The all-gather producer writes %F (dep #0) consumed by the replicated_read
// sibling: dep #0 selects all_gather, the others none.
// CHECK-LABEL: func.func @all_gather
// CHECK: codir.codelet
// CHECK-SAME: dep_collectives = [#codir.collective<all_gather>, #codir.collective<none>, #codir.collective<none>]

// The cross-owner transpose-reduce writes %y (dep #0): dep #0 selects
// reduce_scatter, the matrix read none.
// CHECK-LABEL: func.func @reduce_scatter
// CHECK: codir.codelet
// CHECK-SAME: dep_collectives = [#codir.collective<reduce_scatter>, #codir.collective<none>]

// The iterative stencil write selects halo without pre-seeding dep_collectives.
// CHECK-LABEL: func.func @halo_collective
// CHECK: codir.codelet
// CHECK-SAME: dep_collectives = [#codir.collective<halo>]

// The in-place safe halo stencil is planned as block storage and selects halo;
// readwrite must not fall through to host_whole/all_gather.
// CHECK-LABEL: func.func @inplace_safe_readwrite_halo_storage
// CHECK: codir.codelet
// CHECK-SAME: dep_collectives = [#codir.collective<halo>]
// CHECK-SAME: dep_owner_dims = [{{\[}}0, 1]]
// CHECK-SAME: dep_storage_views = [#codir.storage_view<compute_block>]

// An oversized in-place-safe write footprint keeps host_whole and cannot select
// halo just because access offsets are nonzero.
// CHECK-LABEL: func.func @inplace_safe_halo_rejects_oversized_write
// CHECK: codir.codelet
// CHECK-SAME: dep_collectives = [#codir.collective<none>]
// CHECK-SAME: dep_owner_dims = [{{\[}}0, 1]]
// CHECK-SAME: dep_storage_views = [#codir.storage_view<host_whole>]

// Generic layout mismatch selects all_gather without relying on a matmul
// consumer predicate, and StoragePlanning does not rewrite the forwarded SDE
// layout/grouping facts.
// CHECK-LABEL: func.func @generic_layout_mismatch_all_gather
// CHECK: codir.codelet
// CHECK-SAME: array_layout = [{arrayId = 7 : i64
// CHECK-SAME: commVolumeBytes = 4096 : i64
// CHECK-SAME: kind = "block_contraction"
// CHECK-SAME: dep_collectives = [#codir.collective<all_gather>]
// CHECK-SAME: layouts_disagree = [7]
// CHECK-SAME: partition_graph = [{cuGroupCount = 2 : i64
// CHECK-SAME: cuGroupSize = 4 : i64
// CHECK-SAME: edgeClass = "layout_mismatch"
// CHECK-SAME: edgeCommBytes = 4096 : i64
// CHECK-SAME: partition_score = {chosenCuCount = 8 : i64
// CHECK-SAME: cuGroupSize = 4 : i64

// Reduction-like layout mismatch selects reduce_scatter using the same
// code-agnostic evidence path.
// CHECK-LABEL: func.func @generic_layout_mismatch_reduce_scatter
// CHECK: codir.codelet
// CHECK-SAME: array_layout = [{arrayId = 11 : i64
// CHECK-SAME: commVolumeBytes = 8192 : i64
// CHECK-SAME: kind = "block_contraction"
// CHECK-SAME: dep_collectives = [#codir.collective<reduce_scatter>]
// CHECK-SAME: layouts_disagree = [11]
// CHECK-SAME: partition_graph = [{cuGroupCount = 2 : i64
// CHECK-SAME: cuGroupSize = 4 : i64
// CHECK-SAME: edgeClass = "layout_mismatch"
// CHECK-SAME: edgeCommBytes = 8192 : i64
