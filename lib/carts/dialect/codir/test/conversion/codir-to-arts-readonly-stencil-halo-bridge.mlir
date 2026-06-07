// RUN: %carts-compile %s --pipeline post-db-refinement --arts-config %inputs_dir/arts_multinode_8x64.cfg \
// RUN:   | %FileCheck %s --implicit-check-not=stencil_read_internode_use --implicit-check-not=host_whole_to_compute_block

// Large read-only stencil deps with cross-tile halo are promoted to
// replicated-read at storage-planning. A single coarse DB hosts the data and
// the per-EDT acquire carries `replicatedRead`, so the runtime can duplicate
// it across nodes via PREFER_DUPLICATE instead of paying for a host_whole →
// compute_block bridge.

module attributes {arts.runtime_total_nodes = 8 : i64, arts.runtime_total_workers = 512 : i64} {
  func.func @read_only_stencil_halo_bridge() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index
    %c8 = arith.constant 8 : index
    %c65545 = arith.constant 65545 : index
    %c65546 = arith.constant 65546 : index
    %cst = arith.constant 1.000000e+00 : f32
    %A = memref.alloc() : memref<65546x4xf32>
    memref.store %cst, %A[%c0, %c0] : memref<65546x4xf32>
    scf.for %i = %c1 to %c65545 step %c8 {
      codir.codelet deps(%A : memref<65546x4xf32>) params(%i : index)
          attributes {access_max_offsets = [1, 0],
                      access_min_offsets = [-1, 0],
                      dep_modes = [#codir.access_mode<read>],
                      dep_storage_views = [#codir.storage_view<compute_block>],
                      distribution_kind = #codir.distribution_kind<blocked>,
                      halo_shape = [1],
                      iteration_topology = #codir.iteration_topology<owner_strip>,
                      logical_worker_slice = [8, 4],
                      pattern = #codir.pattern<stencil_tiling_nd>,
                      plan_owner_dims = [0, 1],
                      spatial_dims = [0, 1],
                      tile_owner_dims = [0],
                      tile_shape = [8, 4],
                      write_footprint = [1, 1]} {
      ^bb0(%arg0: memref<65546x4xf32>, %base: index):
        %inner_c0 = arith.constant 0 : index
        %inner_c1 = arith.constant 1 : index
        %lo = arith.subi %base, %inner_c1 : index
        %hi = arith.addi %base, %inner_c1 : index
        %before = memref.load %arg0[%lo, %inner_c0] : memref<65546x4xf32>
        %after = memref.load %arg0[%hi, %inner_c0] : memref<65546x4xf32>
        %sum = arith.addf %before, %after : f32
        func.call @use(%sum) : (f32) -> ()
        codir.yield
      }
    }
    %result = memref.load %A[%c0, %c0] : memref<65546x4xf32>
    func.call @use(%result) : (f32) -> ()
    memref.dealloc %A : memref<65546x4xf32>
    return
  }

  func.func private @use(f32)
}

// CHECK-LABEL: func.func @read_only_stencil_halo_bridge
// CHECK: arts.db_alloc{{.*}}<coarse>
// CHECK-SAME: local_only
// CHECK: arts.db_acquire[<in>]
// CHECK-SAME: partitioning(<coarse>)
// CHECK-SAME: {preserve_access_mode = #arts.preserve_access_mode, replicatedRead
// CHECK-SAME: runtime_db_mode = #arts.runtime_db_mode<ro>
// CHECK: arts.edt <task> <internode>
// CHECK-SAME: distribution_kind = #arts.distribution_kind<block>
// CHECK-SAME: planHaloShape = [1]
// CHECK-SAME: planPhysicalBlockShape = [8, 4]
// CHECK-SAME: stencil_supported_block_halo
