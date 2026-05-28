// RUN: %carts-compile %s --pipeline post-db-refinement --arts-config %inputs_dir/arts_multinode_8x64.cfg --distributed-db \
// RUN:   | %FileCheck %s

// jacobi-style alternating-buffer stencils whose SDE→CODIR materializer
// stamps the initial dep view as host_whole (pointer-of-pointer roots,
// whole-storage tokens) must still emerge with block-distributed storage.
// The storage planner promotes host_whole → compute_block via the stencil
// demote predicates; the slice predicate would normally undo that promotion
// (stencil halos cross the owner slice by construction), so the planner
// must re-check the demote predicates in the slice fallback path.

module attributes {arts.runtime_total_nodes = 8 : i64, arts.runtime_total_workers = 512 : i64} {
  func.func @jacobi_alternating_buffer_host_whole_promoted_to_block() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c8 = arith.constant 8 : index
    %c65545 = arith.constant 65545 : index
    %cst = arith.constant 1.000000e+00 : f32
    %A = memref.alloc() : memref<65546x4xf32>
    %B = memref.alloc() : memref<65546x4xf32>
    memref.store %cst, %A[%c0, %c0] : memref<65546x4xf32>
    memref.store %cst, %B[%c0, %c0] : memref<65546x4xf32>
    scf.for %i = %c1 to %c65545 step %c8 {
      codir.codelet deps(%A, %B : memref<65546x4xf32>, memref<65546x4xf32>) params(%i : index)
          attributes {access_max_offsets = [1, 0],
                      access_min_offsets = [-1, 0],
                      dep_modes = [#codir.access_mode<read>, #codir.access_mode<write>],
                      dep_storage_views = [#codir.storage_view<host_whole>, #codir.storage_view<host_whole>],
                      distribution_kind = #codir.distribution_kind<blocked>,
                      halo_shape = [1],
                      iteration_topology = #codir.iteration_topology<owner_strip>,
                      logical_worker_slice = [8, 4],
                      pattern = #codir.pattern<jacobi_alternating_buffers>,
                      plan_owner_dims = [0, 1],
                      spatial_dims = [0, 1],
                      tile_owner_dims = [0],
                      tile_shape = [8, 4],
                      write_footprint = [1, 1]} {
      ^bb0(%arg0: memref<65546x4xf32>, %arg1: memref<65546x4xf32>, %base: index):
        %inner_c0 = arith.constant 0 : index
        %inner_c1 = arith.constant 1 : index
        %lo = arith.subi %base, %inner_c1 : index
        %hi = arith.addi %base, %inner_c1 : index
        %before = memref.load %arg0[%lo, %inner_c0] : memref<65546x4xf32>
        %after = memref.load %arg0[%hi, %inner_c0] : memref<65546x4xf32>
        %sum = arith.addf %before, %after : f32
        memref.store %sum, %arg1[%base, %inner_c0] : memref<65546x4xf32>
        codir.yield
      }
    }
    %result = memref.load %B[%c0, %c0] : memref<65546x4xf32>
    func.call @use(%result) : (f32) -> ()
    memref.dealloc %B : memref<65546x4xf32>
    memref.dealloc %A : memref<65546x4xf32>
    return
  }

  func.func private @use(f32)
}

// CHECK-LABEL: func.func @jacobi_alternating_buffer_host_whole_promoted_to_block
// CHECK: arts.edt <task> <internode>{{.*}}depPattern = #arts.dep_pattern<jacobi_alternating_buffers>
// CHECK-SAME: distribution_kind = #arts.distribution_kind<block>
// CHECK-SAME: planHaloShape = [1]
// CHECK-SAME: stencil_supported_block_halo
