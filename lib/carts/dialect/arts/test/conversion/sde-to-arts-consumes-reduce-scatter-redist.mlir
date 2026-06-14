// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-storage-to-arts-db,sde-accesses-to-arts-deps,finalize-sde-to-arts,verify-arts-objects-only)' 2>&1 | %FileCheck %s --implicit-check-not=sde.redist --implicit-check-not=sde.su_reduce_scatter --implicit-check-not=sde.su_distribute --implicit-check-not=sde.su_iterate --implicit-check-not=arts.db_access_window
// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-storage-to-arts-db,sde-accesses-to-arts-deps,finalize-sde-to-arts,block-contraction-split)' 2>&1 | %FileCheck %s --check-prefix=SPLIT

// SDE commits the reduce-scatter movement family and partial-reduction axes.
// ARTS consumes those facts into task dependencies and task-level reduction
// metadata; the SDE carrier must not survive the boundary.
// The contraction-split pass must fail closed if the matching per-block replica
// graph has not been realized yet.

// CHECK-LABEL: func.func @consume_reduce_scatter_redist
// CHECK: %[[SRC_GUID:.*]], %[[SRC_PTR:.*]] = arts.db_alloc
// CHECK-SAME: elementSizes[%{{[^,]+}}, %{{[^]]+}}]
// CHECK: %[[DST_GUID:.*]], %[[DST_PTR:.*]] = arts.db_alloc
// CHECK-SAME: elementSizes[%{{[^,]+}}, %{{[^]]+}}]
// CHECK: %{{.*}}, %[[DST_DEP:.*]] = arts.db_acquire[<inout>]
// CHECK: %{{.*}}, %[[SRC_DEP:.*]] = arts.db_acquire[<in>] (%[[SRC_GUID]]
// CHECK-SAME: replicatedRead
// CHECK: arts.edt
// CHECK-SAME: partialReduction
// CHECK-SAME: partialReductionDepResultDimMaps = {{\[\[0\], \[-1\]\]}}
// CHECK-SAME: partialReductionDims = [0]
// CHECK-SAME: partialReductionOwnerDims = [0]

// CHECK-LABEL: func.func @consume_reduce_scatter_redist_2d_physical_window
// CHECK: arts.db_acquire[<in>]
// CHECK-SAME: replicatedRead
// CHECK: arts.edt

// SPLIT-LABEL: func.func @consume_reduce_scatter_redist
// SPLIT: arts.edt

module attributes {arts.runtime_total_nodes = 2 : i64, arts.runtime_total_workers = 8 : i64} {
  func.func @consume_reduce_scatter_redist() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index
    %c8 = arith.constant 8 : index
    %zero = arith.constant 0.0 : f32
    %T = sde.mu_alloc : memref<2x4xf32>
    %G = sde.mu_alloc : memref<2x4xf32>

    sde.su_iterate (%c0) to (%c8) step (%c4) classification(<elementwise_pipeline>) {
    ^bb0(%j: index):
      sde.array_layout_root write %T : memref<2x4xf32> array_id(0)
      sde.cu_region <single> {
        %b = arith.divui %j, %c4 : index
        %e = arith.remui %j, %c4 : index
        memref.store %zero, %T[%b, %e] : memref<2x4xf32>
        sde.yield
      }
      sde.yield
    } {arrayLayout = [{arrayId = 0 : i64, blockShape = [1, 4], kind = "block_contraction", muBlockCount = 2 : i64, ownerDims = [0], role = "write"}]}

    sde.su_distribute <owner_compute> {
      sde.su_reduce_scatter %T : memref<2x4xf32> array_id(0) owner [0] block [1, 4] reduce 0 kind <add>
      sde.su_iterate (%c0) to (%c8) step (%c4) classification(<elementwise_pipeline>) {
      ^bb0(%j: index):
        sde.array_layout_root read %T : memref<2x4xf32> array_id(0)
        sde.array_layout_root write %G : memref<2x4xf32> array_id(1)
        sde.cu_region <parallel> {
          %b = arith.divui %j, %c4 : index
          %e = arith.remui %j, %c4 : index
          %partial = memref.load %T[%b, %e] : memref<2x4xf32>
          %acc = memref.load %G[%b, %e] : memref<2x4xf32>
          %sum = arith.addf %acc, %partial : f32
          memref.store %sum, %G[%b, %e] : memref<2x4xf32>
        } {groupBlockCount = [2]}
        sde.yield
      } {arrayLayout = [{arrayId = 0 : i64, blockShape = [1, 4], kind = "block_contraction", muBlockCount = 2 : i64, ownerDims = [0], role = "read"}, {arrayId = 1 : i64, blockShape = [4], kind = "block_parallel", muBlockCount = 2 : i64, ownerDims = [0], role = "write"}], partialReduction, partialReductionDims = [0], partialReductionOwnerDims = [0]}
    }
    return
  }

  func.func @consume_reduce_scatter_redist_2d_physical_window() {
    %c0 = arith.constant 0 : index
    %c4 = arith.constant 4 : index
    %c8 = arith.constant 8 : index
    %zero = arith.constant 0.0 : f32
    %T = sde.mu_alloc : memref<2x2x4x4xf32>
    %G = sde.mu_alloc : memref<2x2x4x4xf32>

    sde.su_iterate (%c0, %c0) to (%c8, %c8) step (%c4, %c4) classification(<elementwise_pipeline>) {
    ^bb0(%i: index, %j: index):
      sde.array_layout_root write %T : memref<2x2x4x4xf32> array_id(0)
      sde.cu_region <single> {
        %bi = arith.divui %i, %c4 : index
        %bj = arith.divui %j, %c4 : index
        %ei = arith.remui %i, %c4 : index
        %ej = arith.remui %j, %c4 : index
        memref.store %zero, %T[%bi, %bj, %ei, %ej] : memref<2x2x4x4xf32>
        sde.yield
      }
      sde.yield
    } {arrayLayout = [{arrayId = 0 : i64, blockShape = [1, 1, 4, 4], kind = "block_contraction", muBlockCount = 4 : i64, ownerDims = [0, 1], role = "write"}]}

    sde.su_distribute <owner_compute> {
      sde.su_reduce_scatter %T : memref<2x2x4x4xf32> array_id(0) owner [0, 1] block [1, 1, 4, 4] reduce 0 kind <add>
      sde.su_iterate (%c0, %c0) to (%c8, %c8) step (%c4, %c4) classification(<elementwise_pipeline>) {
      ^bb0(%i: index, %j: index):
        sde.array_layout_root read %T : memref<2x2x4x4xf32> array_id(0)
        sde.array_layout_root write %G : memref<2x2x4x4xf32> array_id(1)
        sde.cu_region <parallel> {
          %bi = arith.divui %i, %c4 : index
          %bj = arith.divui %j, %c4 : index
          %ei = arith.remui %i, %c4 : index
          %ej = arith.remui %j, %c4 : index
          %partial = memref.load %T[%bi, %bj, %ei, %ej] : memref<2x2x4x4xf32>
          %acc = memref.load %G[%bi, %bj, %ei, %ej] : memref<2x2x4x4xf32>
          %sum = arith.addf %acc, %partial : f32
          memref.store %sum, %G[%bi, %bj, %ei, %ej] : memref<2x2x4x4xf32>
        } {groupBlockCount = [2, 2]}
        sde.yield
      } {arrayLayout = [{arrayId = 0 : i64, blockShape = [1, 1, 4, 4], kind = "block_contraction", muBlockCount = 4 : i64, ownerDims = [0, 1], role = "read"}, {arrayId = 1 : i64, blockShape = [4, 4], kind = "block_parallel", muBlockCount = 4 : i64, ownerDims = [0, 1], role = "write"}], partialReduction, partialReductionDims = [0], partialReductionOwnerDims = [0]}
    }
    return
  }
}
