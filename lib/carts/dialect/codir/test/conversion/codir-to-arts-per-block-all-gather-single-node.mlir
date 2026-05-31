// RUN: %carts-compile %s --pass-pipeline='builtin.module(verify-codir,convert-codir-to-arts,verify-arts-objects-only)' | %FileCheck %s --implicit-check-not=perBlockAllGather

module attributes {arts.runtime_total_nodes = 1 : i64, arts.runtime_total_workers = 64 : i64} {
  func.func @single_node_all_gather_collective_is_local_bridge_only() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index
    %c8 = arith.constant 8 : index
    %c24 = arith.constant 24 : index
    %F = sde.mu_alloc : memref<24x4xf32>

    scf.for %i = %c0 to %c24 step %c8 {
      codir.codelet deps(%F : memref<24x4xf32>) params(%i : index)
          attributes {dep_collectives = [#codir.collective<all_gather>],
                      dep_modes = [#codir.access_mode<write>],
                      dep_storage_views = [#codir.storage_view<phase_redistributed>],
                      distribution_kind = #codir.distribution_kind<blocked>,
                      iteration_topology = #codir.iteration_topology<owner_strip>,
                      logical_worker_slice = [8, 4],
                      pattern = #codir.pattern<matmul>,
                      tile_owner_dims = [0],
                      tile_shape = [8, 4]} {
      ^bb0(%arg0: memref<24x4xf32>, %base: index):
        %inner_c0 = arith.constant 0 : index
        %inner_c1 = arith.constant 1 : index
        %inner_c4 = arith.constant 4 : index
        %inner_c8 = arith.constant 8 : index
        %inner_c24 = arith.constant 24 : index
        %value = arith.constant 3.000000e+00 : f32
        %end_raw = arith.addi %base, %inner_c8 : index
        %end = arith.minui %end_raw, %inner_c24 : index
        scf.for %row = %base to %end step %inner_c1 {
          scf.for %col = %inner_c0 to %inner_c4 step %inner_c1 {
            memref.store %value, %arg0[%row, %col] : memref<24x4xf32>
          }
        }
        codir.yield
      }
    }

    %G = memref.alloc() : memref<24x4xf32>
    scf.for %i = %c0 to %c24 step %c8 {
      codir.codelet deps(%G, %F : memref<24x4xf32>, memref<24x4xf32>) params(%i : index)
          attributes {dep_collectives = [#codir.collective<none>, #codir.collective<none>],
                      dep_modes = [#codir.access_mode<write>, #codir.access_mode<read>],
                      dep_storage_views = [#codir.storage_view<compute_block>, #codir.storage_view<replicated_read>],
                      distribution_kind = #codir.distribution_kind<blocked>,
                      iteration_topology = #codir.iteration_topology<owner_strip>,
                      logical_worker_slice = [8, 4],
                      pattern = #codir.pattern<matmul>,
                      tile_owner_dims = [0],
                      tile_shape = [8, 4]} {
      ^bb0(%argG: memref<24x4xf32>, %argF: memref<24x4xf32>, %base: index):
        %inner_c0 = arith.constant 0 : index
        %inner_c1 = arith.constant 1 : index
        %inner_c4 = arith.constant 4 : index
        %inner_c8 = arith.constant 8 : index
        %inner_c24 = arith.constant 24 : index
        %end_raw = arith.addi %base, %inner_c8 : index
        %end = arith.minui %end_raw, %inner_c24 : index
        scf.for %row = %base to %end step %inner_c1 {
          scf.for %col = %inner_c0 to %inner_c4 step %inner_c1 {
            %acc0 = arith.constant 0.000000e+00 : f32
            %acc = scf.for %k = %inner_c0 to %inner_c24 step %inner_c1
                iter_args(%a = %acc0) -> (f32) {
              %fv = memref.load %argF[%k, %col] : memref<24x4xf32>
              %na = arith.addf %a, %fv : f32
              scf.yield %na : f32
            }
            memref.store %acc, %argG[%row, %col] : memref<24x4xf32>
          }
        }
        codir.yield
      }
    }

    %result = memref.load %G[%c0, %c0] : memref<24x4xf32>
    func.call @use(%result) : (f32) -> ()
    memref.dealloc %G : memref<24x4xf32>
    return
  }

  func.func private @use(f32)
}

// CHECK-LABEL: func.func @single_node_all_gather_collective_is_local_bridge_only
// CHECK: arts.db_alloc[<inout>, <heap>, <write>, <block>]
// CHECK-SAME: storage_bridge = #arts.storage_bridge<host_whole_to_compute_block>
// CHECK: arts.db_acquire[<inout>] {{.*}} partitioning(<coarse>)
// CHECK: arts.barrier
