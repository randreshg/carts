// RUN: %carts-compile %s --pass-pipeline='builtin.module(verify-codir,dep-storage-assignment,verify-codir)' \
// RUN:   | %FileCheck %s

// Rank-expanded partial-reduction result carriers are already physical block
// grids. CODIR must preserve that as compute-block storage instead of falling
// back to host_whole.

module {
  func.func @rank_expanded_partial_reduction_result_is_block_storage() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c64 = arith.constant 64 : index
    %partial = memref.alloc() : memref<64x255xf64>
    %input = memref.alloc() : memref<64x255xf64>
    scf.for %g = %c0 to %c64 step %c1 {
      codir.codelet deps(%partial, %input : memref<64x255xf64>, memref<64x255xf64>)
          params(%g : index)
          attributes {array_layout = [{arrayId = 0 : i64, blockShape = [255], commVolumeBytes = 0 : i64, kind = "owner_block", muBlockCount = 64 : i64, ownerDims = [0], role = "write"},
                                      {arrayId = 1 : i64, blockShape = [255], commVolumeBytes = 0 : i64, kind = "owner_block", muBlockCount = 64 : i64, ownerDims = [0], role = "read"}],
                      dep_array_ids = [0, 1],
                      dep_collectives = [#codir.collective<reduce_scatter>, #codir.collective<none>],
                      dep_modes = [#codir.access_mode<readwrite>, #codir.access_mode<read>],
                      dep_storage_views = [#codir.storage_view<host_whole>, #codir.storage_view<host_whole>],
                      distribution_kind = #codir.distribution_kind<blocked>,
                      iteration_topology = #codir.iteration_topology<owner_strip>,
                      partial_reduction,
                      partial_reduction_dims = [1],
                      partial_reduction_owner_dims = [0],
                      partition_graph = [{blockShape = [255], edgeClass = "aligned", edgeCommBytes = 0 : i64, layoutKind = "owner_block", muBlockCount = 64 : i64, muId = 0 : i64, ownerDims = [0], role = "write", tilePayloadBytes = 2040 : i64},
                                         {blockShape = [255], edgeClass = "aligned", edgeCommBytes = 0 : i64, layoutKind = "owner_block", muBlockCount = 64 : i64, muId = 1 : i64, ownerDims = [0], role = "read", tilePayloadBytes = 2040 : i64}],
                      pattern = #codir.pattern<elementwise_pipeline>,
                      reduction_strategy = #codir.reduction_strategy<local_accumulate>,
                      tile_owner_dims = [0],
                      tile_shape = [255]} {
      ^bb0(%p: memref<64x255xf64>, %in: memref<64x255xf64>, %g_arg: index):
        %inner_c0 = arith.constant 0 : index
        %inner_c1 = arith.constant 1 : index
        %inner_c255 = arith.constant 255 : index
        scf.for %i = %inner_c0 to %inner_c255 step %inner_c1 {
          %old = memref.load %p[%g_arg, %i] : memref<64x255xf64>
          %v = memref.load %in[%g_arg, %i] : memref<64x255xf64>
          %sum = arith.addf %old, %v : f64
          memref.store %sum, %p[%g_arg, %i] : memref<64x255xf64>
        }
        codir.yield
      }
    }
    memref.dealloc %input : memref<64x255xf64>
    memref.dealloc %partial : memref<64x255xf64>
    func.return
  }
}

// CHECK-LABEL: func.func @rank_expanded_partial_reduction_result_is_block_storage
// CHECK: codir.codelet
// CHECK-SAME: dep_collectives = [#codir.collective<reduce_scatter>, #codir.collective<none>]
// CHECK-SAME: dep_owner_dims = {{\[\[}}0], [0]]
// CHECK-SAME: dep_storage_views = [#codir.storage_view<compute_block>, #codir.storage_view<host_whole>]
