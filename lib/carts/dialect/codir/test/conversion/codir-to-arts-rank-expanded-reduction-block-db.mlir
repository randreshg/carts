// RUN: %carts-compile %s --pass-pipeline='builtin.module(verify-codir,dep-storage-assignment,verify-codir,convert-sde-boundary-to-arts,convert-codir-to-arts,verify-arts-objects-only)' \
// RUN:   | %FileCheck %s --implicit-check-not=single_block

// CODIR-to-ARTS consumes CODIR's rank-expanded reduction facts mechanically:
// owner dim 0 is the leading grid coordinate and the DB payload block is the
// full physical shape [1, 255], not the rank-short [255] that would collapse to
// one block.

module {
  func.func @rank_expanded_partial_reduction_db_uses_full_physical_block() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c64 = arith.constant 64 : index
    %partial = memref.alloc() : memref<64x255xf64>
    scf.for %g = %c0 to %c64 step %c1 {
      codir.codelet deps(%partial : memref<64x255xf64>)
          params(%g : index)
          attributes {array_layout = [{arrayId = 0 : i64, blockShape = [255], commVolumeBytes = 0 : i64, kind = "owner_block", muBlockCount = 64 : i64, ownerDims = [0], role = "write"}],
                      dep_array_ids = [0],
                      dep_collectives = [#codir.collective<reduce_scatter>],
                      dep_modes = [#codir.access_mode<readwrite>],
                      dep_storage_views = [#codir.storage_view<compute_block>],
                      distribution_kind = #codir.distribution_kind<blocked>,
                      iteration_topology = #codir.iteration_topology<owner_strip>,
                      partial_reduction,
                      partial_reduction_dims = [1],
                      partial_reduction_owner_dims = [0],
                      partition_graph = [{blockShape = [255], edgeClass = "aligned", edgeCommBytes = 0 : i64, layoutKind = "owner_block", muBlockCount = 64 : i64, muId = 0 : i64, ownerDims = [0], role = "write", tilePayloadBytes = 2040 : i64}],
                      pattern = #codir.pattern<elementwise_pipeline>,
                      reduction_strategy = #codir.reduction_strategy<local_accumulate>,
                      tile_owner_dims = [0],
                      tile_shape = [255]} {
      ^bb0(%p: memref<64x255xf64>, %g_arg: index):
        %inner_c0 = arith.constant 0 : index
        %inner_c1 = arith.constant 1 : index
        %inner_c255 = arith.constant 255 : index
        scf.for %i = %inner_c0 to %inner_c255 step %inner_c1 {
          %v = memref.load %p[%g_arg, %i] : memref<64x255xf64>
          memref.store %v, %p[%g_arg, %i] : memref<64x255xf64>
        }
        codir.yield
      }
    }
    memref.dealloc %partial : memref<64x255xf64>
    func.return
  }
}

// CHECK-LABEL: func.func @rank_expanded_partial_reduction_db_uses_full_physical_block
// CHECK: %[[C64:.*]] = arith.constant 64 : index
// CHECK: %[[OUTER:.*]] = arith.divui %{{.*}}, %{{.*}} : index
// CHECK: arts.db_alloc
// CHECK-SAME: <block>
// CHECK-SAME: sizes[%[[OUTER]]]
// CHECK-SAME: planOwnerDims = [0]
// CHECK-SAME: planPhysicalBlockShape = [1, 255]
// CHECK-NOT: partitioning(<coarse>)
// CHECK: arts.db_acquire
// CHECK-SAME: partitioning(<block>)
