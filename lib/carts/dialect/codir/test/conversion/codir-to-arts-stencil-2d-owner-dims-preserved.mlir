// RUN: %carts-compile %s --pass-pipeline='builtin.module(verify-codir,storage-planning,verify-codir,materialize-sde-boundary-to-arts,convert-codir-to-arts,verify-arts-objects-only)' \
// RUN:   | %FileCheck %s

// Regression for the stencil production invariant: a 2-D execution tile must
// be backed by 2-D block DBs. This fixture uses row halos so the compact halo
// windows remain contiguous while still proving owner dims are preserved.

module {
  func.func @multi_dimensional_alternating_stencil_uses_2d_block_dbs() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index
    %c8 = arith.constant 8 : index
    %c32 = arith.constant 32 : index
    %c64 = arith.constant 64 : index
    %cst = arith.constant 1.000000e+00 : f64
    %f = memref.alloc() : memref<64x32xf64>
    %u = memref.alloc() : memref<64x32xf64>
    %unew = memref.alloc() : memref<64x32xf64>
    memref.store %cst, %f[%c0, %c0] : memref<64x32xf64>
    memref.store %cst, %u[%c0, %c0] : memref<64x32xf64>
    scf.for %i = %c0 to %c64 step %c8 {
      scf.for %j = %c0 to %c32 step %c4 {
        codir.codelet deps(%f, %unew, %u : memref<64x32xf64>, memref<64x32xf64>, memref<64x32xf64>)
            params(%c64, %i, %j : index, index, index)
            attributes {access_max_offsets = [1, 0],
                        access_min_offsets = [-1, 0],
                        dep_modes = [#codir.access_mode<read>, #codir.access_mode<write>, #codir.access_mode<read>],
                        dep_storage_views = [#codir.storage_view<host_whole>, #codir.storage_view<host_whole>, #codir.storage_view<host_whole>],
                        distribution_kind = #codir.distribution_kind<owner_compute>,
                        halo_shape = [1, 0],
                        iteration_topology = #codir.iteration_topology<owner_tile>,
                        logical_worker_slice = [8, 4],
                        pattern = #codir.pattern<alternating_buffer_stencil>,
                        plan_owner_dims = [0, 1],
                        repetition_structure = #codir.repetition_structure<full_timestep>,
                        spatial_dims = [0, 1],
                        tile_owner_dims = [0, 1],
                        tile_shape = [8, 4],
                        write_footprint = [1, 1]} {
        ^bb0(%arg0: memref<64x32xf64>, %arg1: memref<64x32xf64>, %arg2: memref<64x32xf64>, %n: index, %base_i: index, %base_j: index):
          %inner_c1 = arith.constant 1 : index
          %inner_c4 = arith.constant 4 : index
          %inner_c8 = arith.constant 8 : index
          %inner_c32 = arith.constant 32 : index
          %row_end_raw = arith.addi %base_i, %inner_c8 : index
          %row_end = arith.minui %row_end_raw, %n : index
          scf.for %row = %base_i to %row_end step %inner_c1 {
            %col_end_raw = arith.addi %base_j, %inner_c4 : index
            %col_end = arith.minui %col_end_raw, %inner_c32 : index
            scf.for %col = %base_j to %col_end step %inner_c1 {
              %row_prev = arith.subi %row, %inner_c1 : index
              %row_next = arith.addi %row, %inner_c1 : index
              %north = memref.load %arg2[%row_prev, %col] : memref<64x32xf64>
              %south = memref.load %arg2[%row_next, %col] : memref<64x32xf64>
              %forcing = memref.load %arg0[%row, %col] : memref<64x32xf64>
              %sum0 = arith.addf %north, %south : f64
              %sum = arith.addf %sum0, %forcing : f64
              memref.store %sum, %arg1[%row, %col] : memref<64x32xf64>
            }
          }
          codir.yield
        }
      }
    }
    memref.dealloc %unew : memref<64x32xf64>
    memref.dealloc %u : memref<64x32xf64>
    memref.dealloc %f : memref<64x32xf64>
    return
  }
}

// CHECK-LABEL: func.func @multi_dimensional_alternating_stencil_uses_2d_block_dbs
// CHECK: arts.db_alloc{{.*}}<block>{{.*}}sizes[%{{[^,]+}}, %{{[^]]+}}]{{.*}}elementSizes[%{{[^,]+}}, %{{[^]]+}}]
// CHECK-SAME: planOwnerDims = [0, 1]
// CHECK: arts.db_alloc{{.*}}<block>{{.*}}sizes[%{{[^,]+}}, %{{[^]]+}}]{{.*}}elementSizes[%{{[^,]+}}, %{{[^]]+}}]
// CHECK-SAME: planOwnerDims = [0, 1]
// CHECK: arts.db_alloc{{.*}}<block>{{.*}}sizes[%{{[^,]+}}, %{{[^]]+}}]{{.*}}elementSizes[%{{[^,]+}}, %{{[^]]+}}]
// CHECK-SAME: planOwnerDims = [0, 1]
// CHECK: arts.db_acquire[<in>] {{.*}}memref<?x?xi64>{{.*}}partitioning(<block>)
// CHECK: arts.db_acquire[<out>] {{.*}}memref<?x?xi64>{{.*}}partitioning(<block>)
// CHECK: arts.db_acquire[<in>] {{.*}}memref<?x?xi64>{{.*}}partitioning(<block>)
// CHECK: arts.edt <task> <intranode> route{{.*}} attributes {depPattern = #arts.dep_pattern<alternating_buffer_stencil>
// CHECK-SAME: planOwnerDims = [0, 1]
