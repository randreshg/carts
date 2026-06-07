// RUN: %carts-compile %s --pass-pipeline='builtin.module(verify-codir,convert-codir-to-arts,realize-edt-distribution-plan,verify-arts-objects-only)' \
// RUN:   | %FileCheck %s --implicit-check-not=host_whole

// CODIR-to-ARTS materializes movement from the committed dep_collectives carrier.
// A committed halo collective on a compute-block dependency is realized as
// block-native storage with a per-block reference, never coarse-gathered.

module attributes {arts.runtime_total_nodes = 4 : i64, arts.runtime_total_workers = 256 : i64} {
  func.func @halo_collective_materializes_block_storage() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c8 = arith.constant 8 : index
    %c16 = arith.constant 16 : index
    %A = memref.alloc() : memref<16x16xf32>
    scf.for %i = %c0 to %c16 step %c8 {
      scf.for %j = %c0 to %c16 step %c8 {
        codir.codelet deps(%A : memref<16x16xf32>) params(%i, %j : index, index)
            attributes {access_max_offsets = [1, 1], access_min_offsets = [-1, -1],
                        dep_collectives = [#codir.collective<halo>],
                        dep_modes = [#codir.access_mode<read>],
                        dep_owner_dims = [[0, 1]],
                        dep_storage_views = [#codir.storage_view<compute_block>],
                        halo_shape = [1, 1],
                        pattern = #codir.pattern<stencil_tiling_nd>,
                        tile_owner_dims = [0, 1], tile_shape = [4, 4]} {
        ^bb0(%arg0: memref<16x16xf32>, %iBase: index, %jBase: index):
          %inner_c1 = arith.constant 1 : index
          %inner_c8 = arith.constant 8 : index
          %iEnd = arith.addi %iBase, %inner_c8 : index
          %jEnd = arith.addi %jBase, %inner_c8 : index
          scf.for %ii = %iBase to %iEnd step %inner_c1 {
            scf.for %jj = %jBase to %jEnd step %inner_c1 {
              %v = memref.load %arg0[%ii, %jj] : memref<16x16xf32>
            }
          }
          codir.yield
        }
      }
    }
    return
  }
}

// CHECK-LABEL: func.func @halo_collective_materializes_block_storage
// CHECK: arts.db_alloc
// CHECK-SAME: <block>
// CHECK: arts.db_acquire
// CHECK-SAME: partitioning(<block>)
// CHECK: arts.edt <task>
