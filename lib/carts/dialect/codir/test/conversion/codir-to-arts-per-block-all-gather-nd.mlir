// RUN: %carts-compile %s --pass-pipeline='builtin.module(materialize-sde-boundary-to-arts,convert-codir-to-arts,verify-arts-objects-only)' \
// RUN:   | %FileCheck %s

module attributes {arts.runtime_total_nodes = 4 : i64, arts.runtime_total_workers = 64 : i64} {
  func.func @per_block_all_gather_uses_nd_block_coordinates() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index
    %c8 = arith.constant 8 : index
    %F = sde.mu_alloc : memref<8x8xf32>

    scf.for %i = %c0 to %c8 step %c4 {
      scf.for %j = %c0 to %c8 step %c4 {
        codir.codelet deps(%F : memref<8x8xf32>) params(%i, %j : index, index)
            attributes {dep_collectives = [#codir.collective<all_gather>],
                        dep_modes = [#codir.access_mode<write>],
                        dep_storage_views = [#codir.storage_view<phase_redistributed>],
                        dep_owner_dims = [[0, 1]],
                        distribution_kind = #codir.distribution_kind<blocked>,
                        iteration_topology = #codir.iteration_topology<owner_strip>,
                        logical_worker_slice = [4, 4],
                        pattern = #codir.pattern<matmul>,
                        tile_owner_dims = [0, 1],
                        tile_shape = [4, 4]} {
        ^bb0(%arg0: memref<8x8xf32>, %rowBase: index, %colBase: index):
          %inner_c1 = arith.constant 1 : index
          %inner_c4 = arith.constant 4 : index
          %value = arith.constant 3.000000e+00 : f32
          %rowEnd = arith.addi %rowBase, %inner_c4 : index
          %colEnd = arith.addi %colBase, %inner_c4 : index
          scf.for %row = %rowBase to %rowEnd step %inner_c1 {
            scf.for %col = %colBase to %colEnd step %inner_c1 {
              memref.store %value, %arg0[%row, %col] : memref<8x8xf32>
            }
          }
          codir.yield
        }
      }
    }
    return
  }
}

// CHECK-LABEL: func.func @per_block_all_gather_uses_nd_block_coordinates
// CHECK: %[[SRCG:.*]], %[[SRCP:.*]] = arts.db_alloc{{.*}}<block>
// CHECK-SAME: planOwnerDims = [0, 1]
// CHECK-SAME: planPhysicalBlockShape = [4, 4]
// CHECK: %[[REPG:.*]], %[[REPP:.*]] = arts.db_alloc{{.*}}<block>
// CHECK-SAME: local_only
// CHECK-SAME: perBlockReplicated
// CHECK-SAME: planOwnerDims = [0, 1]
// CHECK: arts.db_acquire[<in>] (%[[SRCG]] : {{.*}}, %[[SRCP]] : {{.*}}) partitioning(<block>)
// CHECK-SAME: offsets[{{%[^,]+}}, {{%[^]]+}}]
// CHECK-SAME: sizes[{{%[^,]+}}, {{%[^]]+}}]
// CHECK: arts.db_acquire[<out>] (%[[REPG]] : {{.*}}, %[[REPP]] : {{.*}}) partitioning(<block>)
// CHECK-SAME: offsets[{{%[^,]+}}, {{%[^]]+}}]
// CHECK-SAME: sizes[{{%[^,]+}}, {{%[^]]+}}]
// CHECK: arts.edt <task>
// CHECK-SAME: perBlockAllGather
