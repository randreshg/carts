// RUN: %carts-compile %s --pass-pipeline='builtin.module(verify-codir,convert-codir-to-arts,verify-arts-objects-only)' --distributed-db | %FileCheck %s

// The all-gather bridge may batch adjacent blocks to amortize launch overhead,
// but a forwarded SDE partition_score is an executable concurrency floor: when
// SDE exposed one CU per block, ARTS must not group those blocks into one bridge
// EDT and erase the ready-task parallelism.

module attributes {arts.runtime_total_nodes = 8 : i64, arts.runtime_total_workers = 512 : i64} {
  func.func @partition_score_keeps_bridge_blocks_concurrent() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index
    %c8 = arith.constant 8 : index
    %c24 = arith.constant 24 : index
    %F = memref.alloc() : memref<24x4xf32>

    scf.for %i = %c0 to %c24 step %c8 {
      codir.codelet deps(%F : memref<24x4xf32>) params(%i : index)
          attributes {dep_collectives = [#codir.collective<all_gather>],
                      dep_modes = [#codir.access_mode<write>],
                      dep_storage_views = [#codir.storage_view<phase_redistributed>],
                      distribution_kind = #codir.distribution_kind<blocked>,
                      iteration_topology = #codir.iteration_topology<owner_strip>,
                      logical_worker_slice = [8, 4],
                      partition_score = {exposedCuCount = 3 : i64, targetLogicalWorkers = 3 : i64},
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

    %result = memref.load %F[%c0, %c0] : memref<24x4xf32>
    func.call @use(%result) : (f32) -> ()
    memref.dealloc %F : memref<24x4xf32>
    return
  }

  func.func @partition_score_rounding_keeps_non_divisible_floor() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index
    %c8 = arith.constant 8 : index
    %c80 = arith.constant 80 : index
    %F = memref.alloc() : memref<80x4xf32>

    scf.for %i = %c0 to %c80 step %c8 {
      codir.codelet deps(%F : memref<80x4xf32>) params(%i : index)
          attributes {dep_collectives = [#codir.collective<all_gather>],
                      dep_modes = [#codir.access_mode<write>],
                      dep_storage_views = [#codir.storage_view<phase_redistributed>],
                      distribution_kind = #codir.distribution_kind<blocked>,
                      iteration_topology = #codir.iteration_topology<owner_strip>,
                      logical_worker_slice = [8, 4],
                      partition_score = {exposedCuCount = 6 : i64, targetLogicalWorkers = 6 : i64},
                      pattern = #codir.pattern<matmul>,
                      tile_owner_dims = [0],
                      tile_shape = [8, 4]} {
      ^bb0(%arg0: memref<80x4xf32>, %base: index):
        %inner_c0 = arith.constant 0 : index
        %inner_c1 = arith.constant 1 : index
        %inner_c4 = arith.constant 4 : index
        %inner_c8 = arith.constant 8 : index
        %inner_c80 = arith.constant 80 : index
        %value = arith.constant 4.000000e+00 : f32
        %end_raw = arith.addi %base, %inner_c8 : index
        %end = arith.minui %end_raw, %inner_c80 : index
        scf.for %row = %base to %end step %inner_c1 {
          scf.for %col = %inner_c0 to %inner_c4 step %inner_c1 {
            memref.store %value, %arg0[%row, %col] : memref<80x4xf32>
          }
        }
        codir.yield
      }
    }

    %result = memref.load %F[%c0, %c0] : memref<80x4xf32>
    func.call @use(%result) : (f32) -> ()
    memref.dealloc %F : memref<80x4xf32>
    return
  }

  func.func private @use(f32)
}

// CHECK-LABEL: func.func @partition_score_keeps_bridge_blocks_concurrent
// CHECK: arts.runtime_query <total_nodes>
// CHECK: scf.for {{.*}} step %c1
// CHECK: scf.for {{.*}} step %c1
// CHECK: arts.db_acquire[<in>] {{.*}} partitioning(<block>)
// CHECK: arts.db_acquire[<out>] {{.*}} partitioning(<block>)
// CHECK: arts.edt <task>
// CHECK-SAME: perBlockAllGather

// CHECK-LABEL: func.func @partition_score_rounding_keeps_non_divisible_floor
// CHECK-NOT: arith.constant 2 : index
// CHECK: arts.runtime_query <total_nodes>
// CHECK: scf.for {{.*}} step %c1
// CHECK: scf.for {{.*}} step %c1
// CHECK: arts.edt <task>
// CHECK-SAME: perBlockAllGather
