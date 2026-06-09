// RUN: %carts-compile %s --pass-pipeline='builtin.module(verify-codir,dep-storage-assignment,verify-codir)' \
// RUN:   | %FileCheck %s --check-prefix=CODIR
// RUN: %carts-compile %s --pass-pipeline='builtin.module(verify-codir,dep-storage-assignment,verify-codir,materialize-sde-boundary-to-arts,convert-codir-to-arts,verify-arts-objects-only)' \
// RUN:   | %FileCheck %s --check-prefix=ARTS --implicit-check-not=codir.codelet

module {
  func.func @replicated_read_block_window() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index
    %c16 = arith.constant 16 : index
    %zero = arith.constant 0.000000e+00 : f64
    %y = memref.alloc() : memref<16xf64>
    %a = memref.alloc() : memref<16x16xf64>
    %tmp = memref.alloc() : memref<16xf64>
    memref.store %zero, %a[%c0, %c0] : memref<16x16xf64>
    scf.for %i = %c0 to %c16 step %c4 {
      codir.codelet deps(%tmp : memref<16xf64>)
          params(%i : index)
          attributes {completion_barrier,
                      dep_collectives = [#codir.collective<none>],
                      dep_modes = [#codir.access_mode<readwrite>],
                      dep_storage_views = [#codir.storage_view<phase_redistributed>],
                      distribution_kind = #codir.distribution_kind<blocked>,
                      in_place_safe,
                      iteration_topology = #codir.iteration_topology<owner_strip>,
                      logical_worker_slice = [4],
                      pattern = #codir.pattern<elementwise_pipeline>,
                      tile_owner_dims = [0],
                      tile_shape = [4]} {
      ^bb0(%vec: memref<16xf64>, %base_i: index):
        %inner_c1 = arith.constant 1 : index
        %inner_c4 = arith.constant 4 : index
        %inner_c16 = arith.constant 16 : index
        %inner_zero = arith.constant 0.000000e+00 : f64
        %end_raw = arith.addi %base_i, %inner_c4 : index
        %end = arith.minui %end_raw, %inner_c16 : index
        scf.for %ii = %base_i to %end step %inner_c1 {
          memref.store %inner_zero, %vec[%ii] : memref<16xf64>
        }
        codir.yield
      }
    }
    scf.for %j = %c0 to %c16 step %c4 {
      codir.codelet deps(%y, %a, %tmp : memref<16xf64>, memref<16x16xf64>, memref<16xf64>)
          params(%j : index)
          attributes {completion_barrier,
                      dep_collectives = [#codir.collective<reduce_scatter>, #codir.collective<none>, #codir.collective<reduce_scatter>],
                      dep_modes = [#codir.access_mode<readwrite>, #codir.access_mode<read>, #codir.access_mode<read>],
                      dep_storage_views = [#codir.storage_view<phase_redistributed>, #codir.storage_view<compute_block>, #codir.storage_view<compute_block>],
                      distribution_kind = #codir.distribution_kind<blocked>,
                      in_place_safe,
                      iteration_topology = #codir.iteration_topology<owner_strip>,
                      logical_worker_slice = [4],
                      partial_reduction,
                      partial_reduction_dep_result_dim_maps = [[0], [-1, 0], [-1]],
                      partial_reduction_dims = [1],
                      partial_reduction_owner_dims = [0],
                      pattern = #codir.pattern<elementwise_pipeline>,
                      reduction_strategy = #codir.reduction_strategy<local_accumulate>,
                      tile_owner_dims = [0],
                      tile_shape = [4]} {
      ^bb0(%out: memref<16xf64>, %mat: memref<16x16xf64>, %vec: memref<16xf64>, %base_j: index):
        %inner_c0 = arith.constant 0 : index
        %inner_c1 = arith.constant 1 : index
        %inner_c4 = arith.constant 4 : index
        %inner_c16 = arith.constant 16 : index
        %inner_zero = arith.constant 0.000000e+00 : f64
        %end_raw = arith.addi %base_j, %inner_c4 : index
        %end = arith.minui %end_raw, %inner_c16 : index
        scf.for %jj = %base_j to %end step %inner_c1 {
          memref.store %inner_zero, %out[%jj] : memref<16xf64>
          scf.for %i = %inner_c0 to %inner_c16 step %inner_c1 {
            %acc = memref.load %out[%jj] : memref<16xf64>
            %aval = memref.load %mat[%i, %jj] : memref<16x16xf64>
            %tval = memref.load %vec[%i] : memref<16xf64>
            %prod = arith.mulf %aval, %tval : f64
            %sum = arith.addf %acc, %prod : f64
            memref.store %sum, %out[%jj] : memref<16xf64>
          }
        }
        codir.yield
      }
    }
    memref.dealloc %tmp : memref<16xf64>
    memref.dealloc %a : memref<16x16xf64>
    memref.dealloc %y : memref<16xf64>
    return
  }
}

// CODIR-LABEL: func.func @replicated_read_block_window
// CODIR: codir.codelet deps(%{{[A-Za-z0-9_]+}}, %{{[A-Za-z0-9_]+}}, %{{[A-Za-z0-9_]+}} : memref<16xf64>, memref<16x16xf64>, memref<16xf64>)
// CODIR-SAME: dep_collectives = [#codir.collective<reduce_scatter>, #codir.collective<none>, #codir.collective<none>]
// CODIR-SAME: dep_owner_dims = [{{\[}}0], [1], [0]]
// CODIR-SAME: dep_storage_views = [#codir.storage_view<phase_redistributed>, #codir.storage_view<host_whole>, #codir.storage_view<replicated_read>]

// ARTS-LABEL: func.func @replicated_read_block_window
// ARTS: arts.db_acquire[<in>]{{.*}}replicatedRead
// ARTS: arts.edt <task>
// ARTS: arith.divui
// ARTS: arts.db_ref %{{.*}}[%{{.*}}] : memref<?xmemref<?xf64>> -> memref<?xf64>
// ARTS: memref.load %{{.*}}[%{{.*}}] : memref<?xf64>
