// RUN: %carts-compile %s --pass-pipeline='builtin.module(verify-codir,convert-codir-to-arts,verify-arts-objects-only)' \
// RUN:   | %FileCheck %s

// A rank-3 logical worker slice may cover multiple physical blocks per owner
// dimension. CODIR-to-ARTS must keep the grouped acquire window and rewrite
// each element access through a per-block db_ref selected by owner-local indices.

module attributes {arts.runtime_total_nodes = 4 : i64, arts.runtime_total_workers = 256 : i64} {
  func.func @grouped_rank3_block_window() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
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
            %inner_c1 = arith.constant 1 : index
            %inner_c8 = arith.constant 8 : index
            %value = arith.constant 1.000000e+00 : f32
            %iEnd = arith.addi %iBase, %inner_c8 : index
            %jEnd = arith.addi %jBase, %inner_c8 : index
            %kEnd = arith.addi %kBase, %inner_c8 : index
            scf.for %ii = %iBase to %iEnd step %inner_c1 {
              scf.for %jj = %jBase to %jEnd step %inner_c1 {
                scf.for %kk = %kBase to %kEnd step %inner_c1 {
                  memref.store %value, %arg0[%ii, %jj, %kk] : memref<16x16x16xf32>
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

  func.func @grouped_rank2_nested_physical_block_loop() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index
    %c8 = arith.constant 8 : index
    %c16 = arith.constant 16 : index
    %A = memref.alloc() : memref<16x16xf32>

    scf.for %i = %c0 to %c16 step %c8 {
      scf.for %j = %c0 to %c16 step %c8 {
        codir.codelet deps(%A : memref<16x16xf32>) params(%i, %j : index, index)
            attributes {dep_collectives = [#codir.collective<none>],
                        dep_modes = [#codir.access_mode<write>],
                        dep_owner_dims = [[0, 1]],
                        dep_storage_views = [#codir.storage_view<compute_block>],
                        distribution_kind = #codir.distribution_kind<blocked>,
                        iteration_topology = #codir.iteration_topology<owner_tile>,
                        logical_worker_slice = [8, 8],
                        pattern = #codir.pattern<uniform>,
                        tile_owner_dims = [0, 1],
                        tile_shape = [4, 8]} {
        ^bb0(%arg0: memref<16x16xf32>, %iBase: index, %jBase: index):
          %inner_c1 = arith.constant 1 : index
          %inner_c4 = arith.constant 4 : index
          %inner_c8 = arith.constant 8 : index
          %value = arith.constant 1.000000e+00 : f32
          %iEnd = arith.addi %iBase, %inner_c8 : index
          scf.for %iBlock = %iBase to %iEnd step %inner_c4 {
            %iBlockEnd = arith.addi %iBlock, %inner_c4 : index
            scf.for %ii = %iBlock to %iBlockEnd step %inner_c1 {
              %jEnd = arith.addi %jBase, %inner_c8 : index
              scf.for %jj = %jBase to %jEnd step %inner_c1 {
                memref.store %value, %arg0[%ii, %jj] : memref<16x16xf32>
              }
            }
          }
          codir.yield
        }
      }
    }
    return
  }

  func.func @grouped_halo_owner_slice_uses_storage_halo_origin() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index
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
                        distribution_kind = #codir.distribution_kind<blocked>,
                        halo_shape = [1, 1],
                        iteration_topology = #codir.iteration_topology<owner_tile>,
                        logical_worker_slice = [8, 8],
                        pattern = #codir.pattern<stencil_tiling_nd>,
                        tile_owner_dims = [0, 1],
                        tile_shape = [4, 4]} {
        ^bb0(%arg0: memref<16x16xf32>, %iBase: index, %jBase: index):
          %inner_c1 = arith.constant 1 : index
          %inner_c8 = arith.constant 8 : index
          %iEnd = arith.addi %iBase, %inner_c8 : index
          %jEnd = arith.addi %jBase, %inner_c8 : index
          scf.for %ii = %iBase to %iEnd step %inner_c1 {
            scf.for %jj = %jBase to %jEnd step %inner_c1 {
              %value = memref.load %arg0[%ii, %jj] : memref<16x16xf32>
            }
          }
          codir.yield
        }
      }
    }
    return
  }

  func.func @grouped_halo_lower_edge_uses_clamped_storage_origin() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index
    %c8 = arith.constant 8 : index
    %c16 = arith.constant 16 : index
    %A = memref.alloc() : memref<16xf32>

    scf.for %i = %c0 to %c16 step %c8 {
      codir.codelet deps(%A : memref<16xf32>) params(%i : index)
          attributes {access_max_offsets = [1],
                      access_min_offsets = [-1],
                      dep_collectives = [#codir.collective<halo>],
                      dep_modes = [#codir.access_mode<read>],
                      dep_owner_dims = [[0]],
                      dep_storage_views = [#codir.storage_view<compute_block>],
                      distribution_kind = #codir.distribution_kind<blocked>,
                      halo_shape = [1],
                      iteration_topology = #codir.iteration_topology<owner_strip>,
                      logical_worker_slice = [8],
                      pattern = #codir.pattern<stencil_tiling_nd>,
                      tile_owner_dims = [0],
                      tile_shape = [4]} {
      ^bb0(%arg0: memref<16xf32>, %iBase: index):
        %value = memref.load %arg0[%iBase] : memref<16xf32>
        codir.yield
      }
    }
    return
  }
}

// CHECK-LABEL: func.func @grouped_rank3_block_window
// CHECK: arts.db_alloc
// CHECK-SAME: <block>
// CHECK-SAME: planOwnerDims = [0, 1, 2]
// CHECK-SAME: planPhysicalBlockShape = [4, 4, 4]
// CHECK-NOT: planLogicalWorkerSlice
// CHECK: arts.db_acquire[<out>]
// CHECK-SAME: partitioning(<block>)
// CHECK-SAME: sizes[{{.*}}, {{.*}}, {{.*}}]
// CHECK: arts.edt <task>
// CHECK-SAME: planLogicalWorkerSlice = [8, 8, 8]
// CHECK-SAME: planOwnerDims = [0, 1, 2]
// CHECK: arith.divui
// CHECK: arith.divui
// CHECK: arith.divui
// CHECK: arts.db_ref
// CHECK: memref.store %{{.*}}, %{{.*}}[%{{.*}}, %{{.*}}, %{{.*}}] : memref<?x?x?xf32>

// CHECK-LABEL: func.func @grouped_rank2_nested_physical_block_loop
// CHECK: arts.db_acquire[<out>]
// CHECK-SAME: partitioning(<block>)
// CHECK-SAME: sizes[{{.*}}, {{.*}}]
// CHECK: arts.edt <task>
// CHECK: arith.divui
// CHECK: arts.db_ref
// CHECK: memref.store %{{.*}}, %{{.*}}[%{{.*}}, %{{.*}}] : memref<?x?xf32>

// CHECK-LABEL: func.func @grouped_halo_owner_slice_uses_storage_halo_origin
// CHECK: arts.db_alloc
// CHECK-SAME: stencil_supported_block_halo
// CHECK: arts.db_acquire[<in>]
// CHECK-SAME: partitioning(<block>)
// CHECK: arts.edt <task>
// CHECK: arith.divui
// CHECK: arith.cmpi uge
// CHECK: arith.select
// CHECK: arts.db_ref
// CHECK: memref.load %{{.*}}[%{{.*}}, %{{.*}}] : memref<?x?xf32>

// CHECK-LABEL: func.func @grouped_halo_lower_edge_uses_clamped_storage_origin
// CHECK: arts.db_alloc
// CHECK-SAME: stencil_supported_block_halo
// CHECK: arts.edt <task>
// CHECK: %[[GE:.*]] = arith.cmpi uge, %[[BLOCK_BASE:.*]], %[[HALO:.*]] : index
// CHECK: %[[SHIFTED:.*]] = arith.subi %[[BLOCK_BASE]], %[[HALO]] : index
// CHECK: %[[ORIGIN:.*]] = arith.select %[[GE]], %[[SHIFTED]], %{{.*}} : index
// CHECK: %[[LOCAL:.*]] = arith.subi %{{.*}}, %[[ORIGIN]] : index
// CHECK: arts.db_ref
// CHECK: memref.load %{{.*}}[%[[LOCAL]]] : memref<?xf32>
