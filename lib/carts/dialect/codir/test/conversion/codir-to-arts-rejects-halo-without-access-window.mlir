// RUN: not %carts-compile %s --pass-pipeline='builtin.module(verify-codir,convert-codir-to-arts)' 2>&1 | %FileCheck %s

// A committed halo collective on a compute-block dependency can only be realized
// from access-window halo facts. When those facts are missing, CODIR-to-ARTS
// fails closed instead of silently degrading to a coarse storage fallback.

// CHECK: error: 'codir.codelet' op dependency #0 commits a halo collective but has no access-window halo facts

module {
  func.func @halo_without_access_window() {
    %c0 = arith.constant 0 : index
    %c8 = arith.constant 8 : index
    %c16 = arith.constant 16 : index
    %A = memref.alloc() : memref<16xf32>
    scf.for %i = %c0 to %c16 step %c8 {
      codir.codelet deps(%A : memref<16xf32>) params(%i : index)
          attributes {dep_collectives = [#codir.collective<halo>],
                      dep_modes = [#codir.access_mode<read>],
                      dep_owner_dims = [[0]],
                      dep_storage_views = [#codir.storage_view<compute_block>],
                      tile_owner_dims = [0], tile_shape = [8]} {
      ^bb0(%arg0: memref<16xf32>, %iBase: index):
        %v = memref.load %arg0[%iBase] : memref<16xf32>
        codir.yield
      }
    }
    return
  }
}
