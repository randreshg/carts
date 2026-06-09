// RUN: not %carts-compile %s --pass-pipeline='builtin.module(convert-sde-boundary-to-arts)' 2>&1 | %FileCheck %s

// A partitioned SDE MU with committed CODIR owner facts must not become a
// coarse DB just because the dependency storage view was left host_whole.

// CHECK: refusing coarse DB fallback

module {
  func.func @reject_partitioned_mu_coarse_fallback() {
    %A = sde.mu_alloc : memref<4x256xf32>
    codir.codelet deps(%A : memref<4x256xf32>)
        attributes {dep_collectives = [#codir.collective<none>],
                    dep_modes = [#codir.access_mode<write>],
                    dep_owner_dims = [[0]],
                    dep_storage_views = [#codir.storage_view<host_whole>],
                    tile_owner_dims = [0],
                    tile_shape = [256]} {
    ^bb0(%arg0: memref<4x256xf32>):
      %c0 = arith.constant 0 : index
      %cst = arith.constant 0.000000e+00 : f32
      memref.store %cst, %arg0[%c0, %c0] : memref<4x256xf32>
      codir.yield
    }
    return
  }
}
