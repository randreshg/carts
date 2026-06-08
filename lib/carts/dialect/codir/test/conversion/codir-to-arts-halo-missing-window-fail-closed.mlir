// RUN: not %carts-compile %s --pass-pipeline='builtin.module(convert-codir-to-arts)' 2>&1 \
// RUN:   | %FileCheck %s

// A compute-block dependency that commits a halo collective must carry the
// access-window halo facts that realize block-native halo storage. When those
// facts are absent CODIR-to-ARTS fails closed instead of degrading to coarse
// storage, so a missing upstream halo carrier cannot silently miscompile.

// CHECK: 'codir.codelet' op dependency #0 commits a halo collective but has no access-window halo facts to materialize block-native halo storage

module attributes {arts.runtime_total_nodes = 8 : i64, arts.runtime_total_workers = 512 : i64} {
  func.func @halo_without_window() {
    %c0 = arith.constant 0 : index
    %c8 = arith.constant 8 : index
    %c24 = arith.constant 24 : index
    scf.for %i = %c0 to %c24 step %c8 {
      %A = memref.alloc() : memref<24x4xf32>
      codir.codelet deps(%A : memref<24x4xf32>) params(%i : index)
          attributes {dep_modes = [#codir.access_mode<readwrite>],
                      dep_owner_dims = [[0]],
                      dep_storage_views = [#codir.storage_view<compute_block>],
                      dep_collectives = [#codir.collective<halo>],
                      distribution_kind = #codir.distribution_kind<blocked>,
                      iteration_topology = #codir.iteration_topology<owner_strip>,
                      logical_worker_slice = [8, 4],
                      pattern = #codir.pattern<stencil_tiling_nd>,
                      tile_owner_dims = [0],
                      tile_shape = [8, 4]} {
      ^bb0(%arg0: memref<24x4xf32>, %base: index):
        %ic0 = arith.constant 0 : index
        %ic1 = arith.constant 1 : index
        %ic4 = arith.constant 4 : index
        %ic8 = arith.constant 8 : index
        %er = arith.addi %base, %ic8 : index
        scf.for %row = %base to %er step %ic1 {
          scf.for %col = %ic0 to %ic4 step %ic1 {
            %v = memref.load %arg0[%row, %col] : memref<24x4xf32>
            memref.store %v, %arg0[%row, %col] : memref<24x4xf32>
          }
        }
        codir.yield
      }
    }
    return
  }
}
