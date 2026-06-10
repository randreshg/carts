// RUN: %carts-compile %s --pass-pipeline='builtin.module(verify-codir,codir-halo-exchange,verify-codir)' \
// RUN:   | %FileCheck %s

// CHECK-LABEL: func.func @halo_dep_rewritten_to_exchange
// CHECK: %[[SRC:.*]] = memref.alloc() : memref<16x16xf32>
// CHECK: %[[HALO:.*]] = codir.halo_exchange %[[SRC]] : memref<16x16xf32> -> memref<16x16xf32>
// CHECK: codir.codelet deps(%[[HALO]] : memref<16x16xf32>)
// CHECK-SAME: dep_collectives = [#codir.collective<halo>]
// CHECK-SAME: dep_storage_views = [#codir.storage_view<compute_block>]

module {
  func.func @halo_dep_rewritten_to_exchange() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c8 = arith.constant 8 : index
    %c16 = arith.constant 16 : index
    %A = memref.alloc() : memref<16x16xf32>
    scf.for %i = %c0 to %c16 step %c8 {
      scf.for %j = %c0 to %c16 step %c8 {
        codir.codelet deps(%A : memref<16x16xf32>) params(%i, %j : index, index)
            attributes {access_max_offsets = [1, 1],
                        access_min_offsets = [-1, -1],
                        dep_collectives = [#codir.collective<halo>],
                        dep_modes = [#codir.access_mode<read>],
                        dep_owner_dims = [[0, 1]],
                        dep_storage_views = [#codir.storage_view<compute_block>],
                        halo_shape = [1, 1],
                        tile_owner_dims = [0, 1],
                        tile_shape = [8, 8]} {
        ^bb0(%arg0: memref<16x16xf32>, %base_i: index, %base_j: index):
          %v = memref.load %arg0[%base_i, %base_j] : memref<16x16xf32>
          codir.yield
        }
      }
    }
    memref.dealloc %A : memref<16x16xf32>
    return
  }
}
