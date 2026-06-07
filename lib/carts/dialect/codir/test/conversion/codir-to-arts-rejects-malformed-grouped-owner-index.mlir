// RUN: not %carts-compile %s --pass-pipeline='builtin.module(verify-codir,convert-codir-to-arts)' 2>&1 \
// RUN:   | %FileCheck %s

// Grouped block-local rewriting must fail closed when an owner dimension access
// does not select the owner slice that defines the acquired grouped window.

module attributes {arts.runtime_total_nodes = 4 : i64, arts.runtime_total_workers = 256 : i64} {
  func.func @reject_non_owner_grouped_index() {
    %c0 = arith.constant 0 : index
    %c4 = arith.constant 4 : index
    %c8 = arith.constant 8 : index
    %c16 = arith.constant 16 : index
    %A = memref.alloc() : memref<16x16x16xf32>

    scf.for %i = %c0 to %c16 step %c8 {
      scf.for %j = %c0 to %c16 step %c8 {
        scf.for %k = %c0 to %c16 step %c8 {
          codir.codelet deps(%A : memref<16x16x16xf32>) params(%i, %j, %k : index, index, index)
              attributes {dep_collectives = [#codir.collective<none>],
                          dep_modes = [#codir.access_mode<write>],
                          dep_owner_dims = [[0, 1, 2]],
                          dep_storage_views = [#codir.storage_view<compute_block>],
                          distribution_kind = #codir.distribution_kind<blocked>,
                          iteration_topology = #codir.iteration_topology<owner_tile>,
                          logical_worker_slice = [8, 8, 8],
                          pattern = #codir.pattern<uniform>,
                          tile_owner_dims = [0, 1, 2],
                          tile_shape = [4, 4, 4]} {
          ^bb0(%arg0: memref<16x16x16xf32>, %iBase: index, %jBase: index, %kBase: index):
            %inner_c0 = arith.constant 0 : index
            %value = arith.constant 1.000000e+00 : f32
            memref.store %value, %arg0[%inner_c0, %jBase, %kBase] : memref<16x16x16xf32>
            codir.yield
          }
        }
      }
    }
    return
  }
}

// CHECK: grouped planned block-local access does not stay within the block window
// CHECK: failed to rewrite planned block dependency accesses to block-local indices
