// RUN: %carts-compile %s --pass-pipeline='builtin.module(verify-codir,dep-storage-assignment,verify-codir)' \
// RUN:   | %FileCheck %s

// A multi-dimensional stencil tile is the storage grain fact. Per-dep
// access inference may prove a smaller single-dim slice for one access path,
// but it must not narrow the backing DB below the 2-D execution tile.

module {
  func.func @multi_dimensional_alternating_stencil_preserves_tile_owner_dims() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index
    %c8 = arith.constant 8 : index
    %c32 = arith.constant 32 : index
    %c64 = arith.constant 64 : index
    %cst = arith.constant 1.000000e+00 : f64
    %f = memref.alloc() : memref<64x32xf64>
    %u = memref.alloc() : memref<64x32xf64>
    %unew = memref.alloc() : memref<64x32xf64>
    memref.store %cst, %f[%c0, %c0] : memref<64x32xf64>
    memref.store %cst, %u[%c0, %c0] : memref<64x32xf64>
    scf.for %i = %c0 to %c64 step %c8 {
      scf.for %j = %c0 to %c32 step %c4 {
        codir.codelet deps(%f, %unew, %u : memref<64x32xf64>, memref<64x32xf64>, memref<64x32xf64>)
            params(%c64, %i, %j : index, index, index)
            attributes {access_max_offsets = [1, 1],
                        access_min_offsets = [-1, -1],
                        dep_modes = [#codir.access_mode<read>, #codir.access_mode<write>, #codir.access_mode<read>],
                        dep_storage_views = [#codir.storage_view<host_whole>, #codir.storage_view<host_whole>, #codir.storage_view<host_whole>],
                        distribution_kind = #codir.distribution_kind<owner_compute>,
                        halo_shape = [1, 1],
                        iteration_topology = #codir.iteration_topology<owner_tile>,
                        logical_worker_slice = [8, 4],
                        pattern = #codir.pattern<alternating_buffer_stencil>,
                        plan_owner_dims = [0, 1],
                        repetition_structure = #codir.repetition_structure<full_timestep>,
                        spatial_dims = [0, 1],
                        tile_owner_dims = [0, 1],
                        tile_shape = [8, 4],
                        write_footprint = [1, 1]} {
        ^bb0(%arg0: memref<64x32xf64>, %arg1: memref<64x32xf64>, %arg2: memref<64x32xf64>, %n: index, %base_i: index, %base_j: index):
          %inner_c1 = arith.constant 1 : index
          %inner_c4 = arith.constant 4 : index
          %inner_c8 = arith.constant 8 : index
          %inner_c32 = arith.constant 32 : index
          %row_end_raw = arith.addi %base_i, %inner_c8 : index
          %row_end = arith.minui %row_end_raw, %n : index
          scf.for %row = %base_i to %row_end step %inner_c1 {
            %col_end_raw = arith.addi %base_j, %inner_c4 : index
            %col_end = arith.minui %col_end_raw, %inner_c32 : index
            scf.for %col = %base_j to %col_end step %inner_c1 {
              %row_prev = arith.subi %row, %inner_c1 : index
              %row_next = arith.addi %row, %inner_c1 : index
              %col_prev = arith.subi %col, %inner_c1 : index
              %col_next = arith.addi %col, %inner_c1 : index
              %north = memref.load %arg2[%row_prev, %col] : memref<64x32xf64>
              %east = memref.load %arg2[%row, %col_next] : memref<64x32xf64>
              %west = memref.load %arg2[%row, %col_prev] : memref<64x32xf64>
              %south = memref.load %arg2[%row_next, %col] : memref<64x32xf64>
              %forcing = memref.load %arg0[%row, %col] : memref<64x32xf64>
              %sum0 = arith.addf %north, %east : f64
              %sum1 = arith.addf %west, %south : f64
              %sum2 = arith.addf %sum0, %sum1 : f64
              %sum = arith.addf %sum2, %forcing : f64
              memref.store %sum, %arg1[%row, %col] : memref<64x32xf64>
            }
          }
          codir.yield
        }
      }
    }
    memref.dealloc %unew : memref<64x32xf64>
    memref.dealloc %u : memref<64x32xf64>
    memref.dealloc %f : memref<64x32xf64>
    return
  }

  func.func @full_timestep_uniform_copy_shared_with_stencil_keeps_block_storage_with_different_cu_group() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c8 = arith.constant 8 : index
    %cst = arith.constant 1.000000e+00 : f64
    %f = memref.alloc() : memref<8x8xf64>
    %u = memref.alloc() : memref<8x8xf64>
    %unew = memref.alloc() : memref<8x8xf64>
    memref.store %cst, %f[%c0, %c0] : memref<8x8xf64>
    memref.store %cst, %u[%c0, %c0] : memref<8x8xf64>
    memref.store %cst, %unew[%c0, %c0] : memref<8x8xf64>
    scf.for %i = %c0 to %c8 step %c8 {
      scf.for %j = %c0 to %c8 step %c8 {
        codir.codelet deps(%unew, %u : memref<8x8xf64>, memref<8x8xf64>)
            params(%c8, %i, %j : index, index, index)
            attributes {dep_modes = [#codir.access_mode<read>, #codir.access_mode<write>],
                        dep_storage_views = [#codir.storage_view<host_whole>, #codir.storage_view<host_whole>],
                        distribution_kind = #codir.distribution_kind<blocked>,
                        iteration_topology = #codir.iteration_topology<owner_tile>,
                        logical_worker_slice = [16, 8],
                        pattern = #codir.pattern<uniform>,
                        repetition_structure = #codir.repetition_structure<full_timestep>,
                        tile_owner_dims = [0, 1],
                        tile_shape = [8, 8]} {
        ^bb0(%src: memref<8x8xf64>, %dst: memref<8x8xf64>, %n: index, %base_i: index, %base_j: index):
          %v = memref.load %src[%base_i, %base_j] : memref<8x8xf64>
          memref.store %v, %dst[%base_i, %base_j] : memref<8x8xf64>
          codir.yield
        }
        codir.codelet deps(%f, %unew, %u : memref<8x8xf64>, memref<8x8xf64>, memref<8x8xf64>)
            params(%c8, %i, %j : index, index, index)
            attributes {access_max_offsets = [1, 1],
                        access_min_offsets = [-1, -1],
                        dep_modes = [#codir.access_mode<read>, #codir.access_mode<write>, #codir.access_mode<read>],
                        dep_storage_views = [#codir.storage_view<host_whole>, #codir.storage_view<host_whole>, #codir.storage_view<host_whole>],
                        distribution_kind = #codir.distribution_kind<owner_compute>,
                        halo_shape = [1, 1],
                        iteration_topology = #codir.iteration_topology<owner_tile>,
                        logical_worker_slice = [8, 8],
                        pattern = #codir.pattern<alternating_buffer_stencil>,
                        plan_owner_dims = [0, 1],
                        repetition_structure = #codir.repetition_structure<full_timestep>,
                        spatial_dims = [0, 1],
                        tile_owner_dims = [0, 1],
                        tile_shape = [8, 8],
                        write_footprint = [1, 1]} {
        ^bb0(%forcing: memref<8x8xf64>, %dst: memref<8x8xf64>, %src: memref<8x8xf64>, %n: index, %base_i: index, %base_j: index):
          %v = memref.load %forcing[%base_i, %base_j] : memref<8x8xf64>
          memref.store %v, %dst[%base_i, %base_j] : memref<8x8xf64>
          codir.yield
        }
      }
    }
    memref.dealloc %unew : memref<8x8xf64>
    memref.dealloc %u : memref<8x8xf64>
    memref.dealloc %f : memref<8x8xf64>
    return
  }

  func.func @full_timestep_uniform_copy_shared_with_stencil_uses_block_storage_for_nested_shapes() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index
    %c8 = arith.constant 8 : index
    %c16 = arith.constant 16 : index
    %cst = arith.constant 1.000000e+00 : f64
    %f = memref.alloc() : memref<16x16xf64>
    %u = memref.alloc() : memref<16x16xf64>
    %unew = memref.alloc() : memref<16x16xf64>
    memref.store %cst, %f[%c0, %c0] : memref<16x16xf64>
    memref.store %cst, %u[%c0, %c0] : memref<16x16xf64>
    memref.store %cst, %unew[%c0, %c0] : memref<16x16xf64>
    scf.for %i = %c0 to %c16 step %c4 {
      scf.for %j = %c0 to %c16 step %c8 {
        codir.codelet deps(%unew, %u : memref<16x16xf64>, memref<16x16xf64>)
            params(%c16, %i, %j : index, index, index)
            attributes {array_layout = [{arrayId = 0 : i64, blockShape = [8, 8], kind = "block_parallel", muBlockCount = 4 : i64, ownerDims = [0, 1], role = "read"},
                                        {arrayId = 1 : i64, blockShape = [4, 8], kind = "block_parallel", muBlockCount = 8 : i64, ownerDims = [0, 1], role = "write"}],
                        dep_array_ids = [0, 1],
                        dep_modes = [#codir.access_mode<read>, #codir.access_mode<write>],
                        dep_storage_views = [#codir.storage_view<host_whole>, #codir.storage_view<host_whole>],
                        distribution_kind = #codir.distribution_kind<blocked>,
                        iteration_topology = #codir.iteration_topology<owner_tile>,
                        logical_worker_slice = [4, 8],
                        partition_graph = [{blockShape = [8, 8], layoutKind = "block_parallel", muBlockCount = 4 : i64, muId = 0 : i64, ownerDims = [0, 1], role = "read"},
                                           {blockShape = [4, 8], layoutKind = "owner_block", muBlockCount = 8 : i64, muId = 1 : i64, ownerDims = [0, 1], role = "write"}],
                        pattern = #codir.pattern<uniform>,
                        repetition_structure = #codir.repetition_structure<full_timestep>,
                        tile_owner_dims = [0, 1],
                        tile_shape = [4, 8]} {
        ^bb0(%src: memref<16x16xf64>, %dst: memref<16x16xf64>, %n: index, %base_i: index, %base_j: index):
          %v = memref.load %src[%base_i, %base_j] : memref<16x16xf64>
          memref.store %v, %dst[%base_i, %base_j] : memref<16x16xf64>
          codir.yield
        }
        codir.codelet deps(%f, %unew, %u : memref<16x16xf64>, memref<16x16xf64>, memref<16x16xf64>)
            params(%c16, %i, %j : index, index, index)
            attributes {access_max_offsets = [1, 1],
                        access_min_offsets = [-1, -1],
                        array_layout = [{arrayId = 0 : i64, blockShape = [8, 8], kind = "block_parallel", muBlockCount = 4 : i64, ownerDims = [0, 1], role = "read"},
                                        {arrayId = 1 : i64, blockShape = [4, 8], kind = "block_parallel", muBlockCount = 8 : i64, ownerDims = [0, 1], role = "write"},
                                        {arrayId = 2 : i64, blockShape = [8, 8], kind = "block_parallel", muBlockCount = 4 : i64, ownerDims = [0, 1], role = "read"}],
                        dep_array_ids = [2, 1, 0],
                        dep_modes = [#codir.access_mode<read>, #codir.access_mode<write>, #codir.access_mode<read>],
                        dep_storage_views = [#codir.storage_view<host_whole>, #codir.storage_view<host_whole>, #codir.storage_view<host_whole>],
                        distribution_kind = #codir.distribution_kind<owner_compute>,
                        halo_shape = [1, 1],
                        iteration_topology = #codir.iteration_topology<owner_tile>,
                        logical_worker_slice = [4, 8],
                        partition_graph = [{blockShape = [8, 8], edgeClass = "layout_mismatch", layoutKind = "block_parallel", muBlockCount = 4 : i64, muId = 2 : i64, ownerDims = [0, 1], role = "read"},
                                           {blockShape = [4, 8], edgeClass = "layout_mismatch", layoutKind = "owner_block", muBlockCount = 8 : i64, muId = 1 : i64, ownerDims = [0, 1], role = "write"},
                                           {blockShape = [8, 8], edgeClass = "aligned", layoutKind = "block_parallel", muBlockCount = 4 : i64, muId = 0 : i64, ownerDims = [0, 1], role = "read"}],
                        pattern = #codir.pattern<alternating_buffer_stencil>,
                        plan_owner_dims = [0, 1],
                        repetition_structure = #codir.repetition_structure<full_timestep>,
                        spatial_dims = [0, 1],
                        tile_owner_dims = [0, 1],
                        tile_shape = [4, 8],
                        write_footprint = [1, 1]} {
        ^bb0(%forcing: memref<16x16xf64>, %dst: memref<16x16xf64>, %src: memref<16x16xf64>, %n: index, %base_i: index, %base_j: index):
          %inner_c1 = arith.constant 1 : index
          %row_next = arith.addi %base_i, %inner_c1 : index
          %col_next = arith.addi %base_j, %inner_c1 : index
          %f0 = memref.load %forcing[%base_i, %base_j] : memref<16x16xf64>
          %u0 = memref.load %src[%row_next, %base_j] : memref<16x16xf64>
          %u1 = memref.load %src[%base_i, %col_next] : memref<16x16xf64>
          %sum = arith.addf %u0, %u1 : f64
          %out = arith.addf %sum, %f0 : f64
          memref.store %out, %dst[%base_i, %base_j] : memref<16x16xf64>
          codir.yield
        }
      }
    }
    memref.dealloc %unew : memref<16x16xf64>
    memref.dealloc %u : memref<16x16xf64>
    memref.dealloc %f : memref<16x16xf64>
    return
  }
}

// CHECK-LABEL: func.func @multi_dimensional_alternating_stencil_preserves_tile_owner_dims
// CHECK: codir.codelet
// CHECK-SAME: dep_collectives = [#codir.collective<none>, #codir.collective<none>, #codir.collective<halo>]
// CHECK-SAME: dep_owner_dims = [{{\[}}0, 1], [0, 1], [0, 1]]
// CHECK-SAME: dep_storage_views = [#codir.storage_view<compute_block>, #codir.storage_view<compute_block>, #codir.storage_view<compute_block>]

// CHECK-LABEL: func.func @full_timestep_uniform_copy_shared_with_stencil_keeps_block_storage_with_different_cu_group
// CHECK: codir.codelet
// CHECK-SAME: dep_storage_views = [#codir.storage_view<phase_redistributed>, #codir.storage_view<phase_redistributed>]
// CHECK-SAME: logical_worker_slice = [16, 8]
// CHECK-SAME: pattern = #codir.pattern<uniform>
// CHECK: codir.codelet
// CHECK-SAME: dep_storage_views = [#codir.storage_view<compute_block>, #codir.storage_view<compute_block>, #codir.storage_view<compute_block>]
// CHECK-SAME: pattern = #codir.pattern<alternating_buffer_stencil>

// CHECK-LABEL: func.func @full_timestep_uniform_copy_shared_with_stencil_uses_block_storage_for_nested_shapes
// CHECK: codir.codelet
// CHECK-SAME: dep_storage_views = [#codir.storage_view<phase_redistributed>, #codir.storage_view<phase_redistributed>]
// CHECK-SAME: logical_worker_slice = [4, 8]
// CHECK-SAME: pattern = #codir.pattern<uniform>
// CHECK: codir.codelet
// CHECK-SAME: dep_collectives = [#codir.collective<none>, #codir.collective<none>, #codir.collective<halo>]
// CHECK-SAME: dep_storage_views = [#codir.storage_view<compute_block>, #codir.storage_view<compute_block>, #codir.storage_view<compute_block>]
// CHECK-SAME: pattern = #codir.pattern<alternating_buffer_stencil>
