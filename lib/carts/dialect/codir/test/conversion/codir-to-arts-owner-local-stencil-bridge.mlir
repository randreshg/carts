// RUN: %carts-compile %s --start-from codir-to-arts --pipeline post-db-refinement --arts-config %inputs_dir/arts_multinode.cfg \
// RUN:   | %FileCheck %s

// A compute-block stencil read that shifts only in non-owner dimensions is
// owner-local after the host-whole -> compute-block copy-in. CODIR-to-ARTS
// commits that as a per-block single-writer stencil bridge. A read that shifts
// along the owner dimension still needs the halo path and must fail closed when
// no halo collective was committed.

module {
  func.func @owner_local_readonly_stencil_bridge() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c8 = arith.constant 8 : index
    %c16 = arith.constant 16 : index
    %c64 = arith.constant 64 : index
    %cst = arith.constant 1.000000e+00 : f64
    %A = memref.alloc() : memref<64x16xf64>
    memref.store %cst, %A[%c0, %c0] : memref<64x16xf64>
    scf.for %i = %c0 to %c64 step %c8 {
      codir.codelet deps(%A : memref<64x16xf64>) params(%i : index)
          attributes {access_max_offsets = [0, 0],
                      access_min_offsets = [0, -1],
                      dep_collectives = [#codir.collective<none>],
                      dep_modes = [#codir.access_mode<read>],
                      dep_owner_dims = [[0]],
                      dep_storage_views = [#codir.storage_view<compute_block>],
                      logical_worker_slice = [8, 16],
                      pattern = #codir.pattern<stencil_tiling_nd>,
                      plan_owner_dims = [0, 1],
                      spatial_dims = [0, 1],
                      tile_owner_dims = [0],
                      tile_shape = [8, 16],
                      write_footprint = [1, 1]} {
      ^bb0(%arg0: memref<64x16xf64>, %base: index):
        %inner_c0 = arith.constant 0 : index
        %value = memref.load %arg0[%base, %inner_c0] : memref<64x16xf64>
        func.call @use(%value) : (f64) -> ()
        codir.yield
      }
    }
    memref.dealloc %A : memref<64x16xf64>
    return
  }

  func.func @owner_dim_crossing_stencil_bridge_without_halo_stays_local() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c8 = arith.constant 8 : index
    %c16 = arith.constant 16 : index
    %c64 = arith.constant 64 : index
    %cst = arith.constant 1.000000e+00 : f64
    %A = memref.alloc() : memref<64x16xf64>
    memref.store %cst, %A[%c0, %c0] : memref<64x16xf64>
    scf.for %i = %c8 to %c64 step %c8 {
      codir.codelet deps(%A : memref<64x16xf64>) params(%i : index)
          attributes {access_max_offsets = [0, 0],
                      access_min_offsets = [-1, 0],
                      dep_collectives = [#codir.collective<none>],
                      dep_modes = [#codir.access_mode<read>],
                      dep_owner_dims = [[0]],
                      dep_storage_views = [#codir.storage_view<compute_block>],
                      logical_worker_slice = [8, 16],
                      pattern = #codir.pattern<stencil_tiling_nd>,
                      plan_owner_dims = [0, 1],
                      spatial_dims = [0, 1],
                      tile_owner_dims = [0],
                      tile_shape = [8, 16],
                      write_footprint = [1, 1]} {
      ^bb0(%arg0: memref<64x16xf64>, %base: index):
        %inner_c0 = arith.constant 0 : index
        %inner_c1 = arith.constant 1 : index
        %im1 = arith.subi %base, %inner_c1 : index
        %value = memref.load %arg0[%im1, %inner_c0] : memref<64x16xf64>
        func.call @use(%value) : (f64) -> ()
        codir.yield
      }
    }
    memref.dealloc %A : memref<64x16xf64>
    return
  }

  func.func private @use(f64)
}

// CHECK-LABEL: func.func @owner_local_readonly_stencil_bridge
// CHECK: arts.db_alloc{{.*}}<coarse>
// CHECK-SAME: local_only
// CHECK: arts.db_alloc[<inout>, <heap>, <write>, <block>]
// CHECK-SAME: distributed
// CHECK-SAME: owner_block_shape = [8]
// CHECK-SAME: owner_map_dims = [0]
// CHECK-SAME: perBlockSingleWriterStencil
// CHECK-SAME: planOwnerDims = [0]
// CHECK-SAME: planPhysicalBlockShape = [8, 16]
// CHECK-SAME: storage_bridge = #arts.storage_bridge<host_whole_to_compute_block>
// CHECK: arts.edt <task> <internode>{{.*}}depPattern = #arts.dep_pattern<stencil_tiling_nd>
// CHECK-SAME: stencil_min_offsets = [0, -1]
// CHECK-NOT: distributed_reject_reason = "stencil_read_internode_use"

// CHECK-LABEL: func.func @owner_dim_crossing_stencil_bridge_without_halo_stays_local
// CHECK: arts.db_alloc{{.*}}<coarse>
// CHECK-SAME: local_only
// CHECK: arts.db_alloc[<inout>, <heap>, <write>, <block>]
// CHECK-SAME: distributed_reject_reason = "stencil_read_internode_use"
// CHECK-SAME: local_only
// CHECK-SAME: planOwnerDims = [0]
// CHECK-SAME: planPhysicalBlockShape = [8, 16]
// CHECK-SAME: storage_bridge = #arts.storage_bridge<host_whole_to_compute_block>
