// RUN: not %carts-compile %s --pass-pipeline='builtin.module(verify-codir,dep-storage-assignment)' 2>&1 \
// RUN:   | %FileCheck %s

// CODIR must not turn a partitioned owner-compute stencil into host_whole or a
// coarse DB when SDE did not commit a realized tile storage plan.

// CHECK: owner-compute stencil has no realized tile storage plan
// CHECK-SAME: tile_owner_dims and tile_shape
// CHECK-SAME: refusing host_whole/coarse storage fallback

module {
  func.func @owner_compute_stencil_missing_tile_plan(%A: memref<64x64xf64>) {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c63 = arith.constant 63 : index
    codir.codelet deps(%A : memref<64x64xf64>)
        params(%c0, %c1, %c63 : index, index, index)
        attributes {access_max_offsets = [1, 1],
                    access_min_offsets = [-1, -1],
                    dep_modes = [#codir.access_mode<readwrite>],
                    dep_storage_views = [#codir.storage_view<host_whole>],
                    distribution_kind = #codir.distribution_kind<owner_compute>,
                    pattern = #codir.pattern<stencil_tiling_nd>,
                    plan_owner_dims = [0, 1],
                    spatial_dims = [0, 1],
                    write_footprint = [1, 16]} {
    ^bb0(%arg0: memref<64x64xf64>, %base: index, %step: index, %ub: index):
      %row_next = arith.addi %base, %step : index
      %v = memref.load %arg0[%row_next, %base] : memref<64x64xf64>
      memref.store %v, %arg0[%base, %base] : memref<64x64xf64>
      codir.yield
    }
    return
  }
}
