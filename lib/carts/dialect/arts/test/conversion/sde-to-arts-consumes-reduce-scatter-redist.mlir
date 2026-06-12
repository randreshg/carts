// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-storage-to-arts-db,sde-accesses-to-arts-deps,finalize-sde-to-arts,verify-arts-objects-only)' 2>&1 | %FileCheck %s --implicit-check-not=sde.redist --implicit-check-not=sde.su_iterate --implicit-check-not=arts.db_access_window
// RUN: not %carts-compile %s --pass-pipeline='builtin.module(sde-storage-to-arts-db,sde-accesses-to-arts-deps,finalize-sde-to-arts,block-contraction-split)' 2>&1 | %FileCheck %s --check-prefix=UNSPLIT

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
// CHECK: %{{.*}}, %[[SRC_DEP:.*]] = arts.db_acquire[<in>] (%[[SRC_GUID]] :
// CHECK-SAME: %[[SRC_PTR]] :
// CHECK-SAME: replicatedRead
// CHECK: %{{.*}}, %[[DST_DEP:.*]] = arts.db_acquire[<inout>] (%[[DST_GUID]] :
// CHECK-SAME: %[[DST_PTR]] :
// CHECK: arts.edt
// CHECK-SAME: (%[[SRC_DEP]], %[[DST_DEP]])
// CHECK-SAME: partialReduction
// CHECK-SAME: partialReductionDepResultDimMaps = {{\[\[-1\], \[0\]\]}}
// CHECK-SAME: partialReductionDims = [0]
// CHECK-SAME: partialReductionOwnerDims = [0]

// UNSPLIT: unsplit full-grid replicated-read contraction dependency

module attributes {arts.runtime_total_nodes = 2 : i64, arts.runtime_total_workers = 8 : i64} {
  func.func @consume_reduce_scatter_redist() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index
    %c8 = arith.constant 8 : index
    %T = sde.mu_alloc : memref<2x4xf32>
    %G = sde.mu_alloc : memref<2x4xf32>

    sde.redist <reduce_scatter_like> %T : memref<2x4xf32> array_id(0) from owner [0] block [1, 4] to owner [0] block [1, 4] cost 64
    sde.su_iterate (%c0) to (%c8) step (%c4) reduction_strategy(<local_accumulate>) classification(<elementwise_pipeline>) {
    ^bb0(%j: index):
      sde.cu_region <parallel> {
        sde.mu_access_window read %T : memref<2x4xf32> array_id(0) owner_dims(1) block_lo [0] block_hi [2] valid [4]
        sde.mu_access_window readwrite %G : memref<2x4xf32> array_id(1) owner_dims(1) block_lo [0] block_hi [2] valid [4]
        %b = arith.divui %j, %c4 : index
        %e = arith.remui %j, %c4 : index
        %partial = memref.load %T[%b, %e] : memref<2x4xf32>
        %acc = memref.load %G[%b, %e] : memref<2x4xf32>
        %sum = arith.addf %acc, %partial : f32
        memref.store %sum, %G[%b, %e] : memref<2x4xf32>
      }
      sde.yield
    } {logicalWorkerSlice = [4], partialReduction, partialReductionDims = [0], partialReductionOwnerDims = [0], physicalBlockShape = [4], physicalOwnerDims = [0]}
    return
  }
}
