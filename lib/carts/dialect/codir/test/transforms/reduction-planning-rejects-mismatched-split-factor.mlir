// RUN: not %carts-compile %s --pass-pipeline='builtin.module(reduction-planning)' 2>&1 \
// RUN:   | %FileCheck %s

module attributes {carts.logical_total_workers = 4096 : i64} {
  func.func @mismatched_partial_reduction_split_factor(%A: memref<1920x16xf32>, %y: memref<1920xf32>, %base: index) {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c16 = arith.constant 16 : index
    %c1920 = arith.constant 1920 : index
    scf.for %i = %base to %c1920 step %c1 {
      codir.codelet deps(%y, %A : memref<1920xf32>, memref<1920x16xf32>)
          params(%i : index)
          attributes {dep_modes = [#codir.access_mode<readwrite>, #codir.access_mode<read>],
                      dep_storage_views = [#codir.storage_view<compute_block>, #codir.storage_view<compute_block>],
                      distribution_kind = #codir.distribution_kind<blocked>,
                      iteration_topology = #codir.iteration_topology<owner_strip>,
                      logical_worker_slice = [1],
                      partial_reduction,
                      partial_reduction_dep_result_dim_maps = [[0], [0, -1]],
                      partial_reduction_dims = [1],
                      partial_reduction_owner_dims = [0],
                      partial_reduction_split_dims = [1],
                      partial_reduction_split_factor = 2 : i64,
                      partial_reduction_split_owner_task_count = 1920 : i64,
                      partial_reduction_split_required,
                      partial_reduction_split_target_worker_count = 4096 : i64,
                      pattern = #codir.pattern<elementwise_pipeline>,
                      tile_owner_dims = [0],
                      tile_shape = [1]} {
      ^bb0(%arg0: memref<1920xf32>, %arg1: memref<1920x16xf32>, %owner: index):
        %inner_c0 = arith.constant 0 : index
        %inner_c1 = arith.constant 1 : index
        %inner_c16 = arith.constant 16 : index
        %old = memref.load %arg0[%owner] : memref<1920xf32>
        scf.for %j = %inner_c0 to %inner_c16 step %inner_c1 {
          %a = memref.load %arg1[%owner, %j] : memref<1920x16xf32>
          %next = arith.addf %old, %a : f32
          memref.store %next, %arg0[%owner] : memref<1920xf32>
        }
        codir.yield
      }
    }
    return
  }
}

// CHECK: existing partial-reduction split factor does not match recomputed CODIR reduction plan
