// RUN: not %carts-compile %s --pass-pipeline='builtin.module(verify-codir,dep-storage-assignment)' 2>&1 \
// RUN:   | %FileCheck %s

// CHECK: full-timestep uniform dependency
// CHECK-SAME: shares partitioned stencil storage with incompatible block shapes
// CHECK-SAME: refusing host_whole/coarse storage fallback

module {
  func.func @uniform_copy_incompatible_with_stencil_block_shape() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index
    %c7 = arith.constant 7 : index
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
            attributes {array_layout = [{arrayId = 0 : i64, blockShape = [7, 8], kind = "block_parallel", muBlockCount = 6 : i64, ownerDims = [0, 1], role = "read"},
                                        {arrayId = 1 : i64, blockShape = [8, 8], kind = "block_parallel", muBlockCount = 4 : i64, ownerDims = [0, 1], role = "write"}],
                        dep_array_ids = [0, 1],
                        dep_modes = [#codir.access_mode<read>, #codir.access_mode<write>],
                        dep_storage_views = [#codir.storage_view<host_whole>, #codir.storage_view<host_whole>],
                        distribution_kind = #codir.distribution_kind<blocked>,
                        iteration_topology = #codir.iteration_topology<owner_tile>,
                        logical_worker_slice = [4, 8],
                        partition_graph = [{blockShape = [7, 8], layoutKind = "block_parallel", muBlockCount = 6 : i64, muId = 0 : i64, ownerDims = [0, 1], role = "read"},
                                           {blockShape = [8, 8], layoutKind = "owner_block", muBlockCount = 4 : i64, muId = 1 : i64, ownerDims = [0, 1], role = "write"}],
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
                        array_layout = [{arrayId = 0 : i64, blockShape = [7, 8], kind = "block_parallel", muBlockCount = 6 : i64, ownerDims = [0, 1], role = "read"},
                                        {arrayId = 1 : i64, blockShape = [8, 8], kind = "block_parallel", muBlockCount = 4 : i64, ownerDims = [0, 1], role = "write"},
                                        {arrayId = 2 : i64, blockShape = [7, 8], kind = "block_parallel", muBlockCount = 6 : i64, ownerDims = [0, 1], role = "read"}],
                        dep_array_ids = [2, 1, 0],
                        dep_modes = [#codir.access_mode<read>, #codir.access_mode<write>, #codir.access_mode<read>],
                        dep_storage_views = [#codir.storage_view<host_whole>, #codir.storage_view<host_whole>, #codir.storage_view<host_whole>],
                        distribution_kind = #codir.distribution_kind<owner_compute>,
                        halo_shape = [1, 1],
                        iteration_topology = #codir.iteration_topology<owner_tile>,
                        logical_worker_slice = [8, 8],
                        partition_graph = [{blockShape = [7, 8], edgeClass = "layout_mismatch", layoutKind = "block_parallel", muBlockCount = 6 : i64, muId = 2 : i64, ownerDims = [0, 1], role = "read"},
                                           {blockShape = [8, 8], edgeClass = "layout_mismatch", layoutKind = "owner_block", muBlockCount = 4 : i64, muId = 1 : i64, ownerDims = [0, 1], role = "write"},
                                           {blockShape = [7, 8], edgeClass = "aligned", layoutKind = "block_parallel", muBlockCount = 6 : i64, muId = 0 : i64, ownerDims = [0, 1], role = "read"}],
                        pattern = #codir.pattern<alternating_buffer_stencil>,
                        plan_owner_dims = [0, 1],
                        repetition_structure = #codir.repetition_structure<full_timestep>,
                        spatial_dims = [0, 1],
                        tile_owner_dims = [0, 1],
                        tile_shape = [8, 8],
                        write_footprint = [1, 1]} {
        ^bb0(%forcing: memref<16x16xf64>, %dst: memref<16x16xf64>, %src: memref<16x16xf64>, %n: index, %base_i: index, %base_j: index):
          %inner_c1 = arith.constant 1 : index
          %row_next = arith.addi %base_i, %inner_c1 : index
          %f0 = memref.load %forcing[%base_i, %base_j] : memref<16x16xf64>
          %u0 = memref.load %src[%row_next, %base_j] : memref<16x16xf64>
          %out = arith.addf %u0, %f0 : f64
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
