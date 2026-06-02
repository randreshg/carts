// RUN: %carts-compile %s --pass-pipeline='builtin.module(materialize-sde-boundary-to-arts,convert-codir-to-arts,verify-arts-objects-only)' \
// RUN:   | %FileCheck %s --check-prefix=CONVERT
// RUN: %carts-compile %s --start-from codir-to-arts --pipeline post-db-refinement --arts-config %inputs_dir/arts_multinode_4x64.cfg --distributed-db \
// RUN:   | %FileCheck %s --check-prefix=DIST

module attributes {arts.runtime_total_nodes = 4 : i64, arts.runtime_total_workers = 256 : i64} {
  func.func @per_block_all_gather_uses_nd_block_coordinates() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index
    %c8 = arith.constant 8 : index
    %F = sde.mu_alloc : memref<8x8x8xf32>

    scf.for %i = %c0 to %c8 step %c4 {
      scf.for %j = %c0 to %c8 step %c4 {
        scf.for %k = %c0 to %c8 step %c4 {
        codir.codelet deps(%F : memref<8x8x8xf32>) params(%i, %j, %k : index, index, index)
            attributes {dep_collectives = [#codir.collective<all_gather>],
                        dep_modes = [#codir.access_mode<write>],
                        dep_storage_views = [#codir.storage_view<phase_redistributed>],
                        dep_owner_dims = [[0, 1, 2]],
                        distribution_kind = #codir.distribution_kind<blocked>,
                        iteration_topology = #codir.iteration_topology<owner_tile>,
                        logical_worker_slice = [4, 4, 4],
                        partition_score = {chosenCuCount = 256 : i64, concurrencyFloor = 256 : i64, cuGroupSize = 1 : i64, exposedCuCount = 256 : i64, muBlockCount = 512 : i64, targetLogicalWorkers = 256 : i64, tileBytes = 256 : i64},
                        pattern = #codir.pattern<uniform>,
                        tile_owner_dims = [0, 1, 2],
                        tile_shape = [4, 4, 4]} {
        ^bb0(%arg0: memref<8x8x8xf32>, %iBase: index, %jBase: index, %kBase: index):
          %inner_c1 = arith.constant 1 : index
          %inner_c4 = arith.constant 4 : index
          %value = arith.constant 3.000000e+00 : f32
          %iEnd = arith.addi %iBase, %inner_c4 : index
          %jEnd = arith.addi %jBase, %inner_c4 : index
          %kEnd = arith.addi %kBase, %inner_c4 : index
          scf.for %ii = %iBase to %iEnd step %inner_c1 {
            scf.for %jj = %jBase to %jEnd step %inner_c1 {
              scf.for %kk = %kBase to %kEnd step %inner_c1 {
                memref.store %value, %arg0[%ii, %jj, %kk] : memref<8x8x8xf32>
              }
            }
          }
          codir.yield
        }
        }
      }
    }
    return
  }

  func.func @per_block_all_gather_transposed_nd_preserves_owner_tile_order() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c8 = arith.constant 8 : index
    %c16 = arith.constant 16 : index
    %c32 = arith.constant 32 : index
    %c64 = arith.constant 64 : index
    %F = sde.mu_alloc : memref<64x64x64xf32>

    scf.for %i = %c0 to %c64 step %c8 {
      scf.for %j = %c0 to %c64 step %c16 {
        scf.for %k = %c0 to %c64 step %c32 {
          codir.codelet deps(%F : memref<64x64x64xf32>) params(%i, %j, %k : index, index, index)
              attributes {dep_collectives = [#codir.collective<all_gather>],
                          dep_modes = [#codir.access_mode<write>],
                          dep_owner_dims = [[2, 1, 0]],
                          dep_storage_views = [#codir.storage_view<phase_redistributed>],
                          distribution_kind = #codir.distribution_kind<blocked>,
                          iteration_topology = #codir.iteration_topology<owner_tile>,
                          logical_worker_slice = [8, 16, 32],
                          partition_score = {chosenCuCount = 256 : i64, concurrencyFloor = 256 : i64, cuGroupSize = 1 : i64, exposedCuCount = 256 : i64, muBlockCount = 256 : i64, targetLogicalWorkers = 256 : i64, tileBytes = 16384 : i64},
                          pattern = #codir.pattern<stencil_tiling_nd>,
                          plan_owner_dims = [0, 1, 2],
                          tile_owner_dims = [2, 1, 0],
                          tile_shape = [8, 16, 32]} {
          ^bb0(%arg0: memref<64x64x64xf32>, %iBase: index, %jBase: index, %kBase: index):
            %inner_c1 = arith.constant 1 : index
            %inner_c8 = arith.constant 8 : index
            %inner_c16 = arith.constant 16 : index
            %inner_c32 = arith.constant 32 : index
            %value = arith.constant 4.000000e+00 : f32
            %iEnd = arith.addi %iBase, %inner_c8 : index
            %jEnd = arith.addi %jBase, %inner_c16 : index
            %kEnd = arith.addi %kBase, %inner_c32 : index
            scf.for %ii = %iBase to %iEnd step %inner_c1 {
              scf.for %jj = %jBase to %jEnd step %inner_c1 {
                scf.for %kk = %kBase to %kEnd step %inner_c1 {
                  memref.store %value, %arg0[%kk, %jj, %ii] : memref<64x64x64xf32>
                }
              }
            }
            codir.yield
          }
        }
      }
    }
    return
  }
}

// CONVERT-LABEL: func.func @per_block_all_gather_uses_nd_block_coordinates
// CONVERT: %[[SRCG:.*]], %[[SRCP:.*]] = arts.db_alloc{{.*}}<block>
// CONVERT-SAME: planOwnerDims = [0, 1, 2]
// CONVERT-SAME: planPhysicalBlockShape = [4, 4, 4]
// CONVERT: %[[REPG:.*]], %[[REPP:.*]] = arts.db_alloc{{.*}}<block>
// CONVERT-SAME: local_only
// CONVERT-SAME: perBlockReplicated
// CONVERT-SAME: planOwnerDims = [0, 1, 2]
// CONVERT: arith.ceildivui
// CONVERT: scf.for
// CONVERT: arith.divui
// CONVERT: arith.remui
// CONVERT: arts.db_acquire[<in>] (%[[SRCG]] : {{.*}}, %[[SRCP]] : {{.*}}) partitioning(<block>)
// CONVERT-SAME: offsets[{{%[^,]+}}, {{%[^,]+}}, {{%[^]]+}}]
// CONVERT-SAME: sizes[{{%[^,]+}}, {{%[^,]+}}, {{%[^]]+}}]
// CONVERT: arts.db_acquire[<out>] (%[[REPG]] : {{.*}}, %[[REPP]] : {{.*}}) partitioning(<block>)
// CONVERT-SAME: offsets[{{%[^,]+}}, {{%[^,]+}}, {{%[^]]+}}]
// CONVERT-SAME: sizes[{{%[^,]+}}, {{%[^,]+}}, {{%[^]]+}}]
// CONVERT: arts.edt <task>
// CONVERT-SAME: perBlockAllGather
// CONVERT-LABEL: func.func @per_block_all_gather_transposed_nd_preserves_owner_tile_order
// CONVERT: arts.db_alloc{{.*}}<block>
// CONVERT-SAME: planOwnerDims = [2, 1, 0]
// CONVERT-SAME: planPhysicalBlockShape = [8, 16, 32]
// CONVERT: arts.edt <task>
// CONVERT-SAME: planOwnerDims = [2, 1, 0]
// CONVERT-SAME: stencil_owner_dims = [2, 1, 0]

// DIST-LABEL: func.func @per_block_all_gather_transposed_nd_preserves_owner_tile_order
// DIST: arts.db_alloc{{.*}}<block>
// DIST-SAME: distributed
// DIST-SAME: owner_block_shape = [32, 16, 8]
// DIST-SAME: owner_map_dims = [0, 1, 2]
// DIST-SAME: planOwnerDims = [2, 1, 0]
// DIST-SAME: planPhysicalBlockShape = [8, 16, 32]
// DIST: arts.db_acquire{{.*}}partitioning(<block>)
// DIST-SAME: offsets[{{%[^,]+}}, {{%[^,]+}}, {{%[^]]+}}]
// DIST-SAME: sizes[{{%[^,]+}}, {{%[^,]+}}, {{%[^]]+}}]
// DIST: arts.edt <task>
// DIST-SAME: planOwnerDims = [2, 1, 0]
// DIST-SAME: stencil_owner_dims = [2, 1, 0]
