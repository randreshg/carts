// RUN: %carts-compile %s --pass-pipeline='builtin.module(reduction-dep-mapping)' \
// RUN:   | %FileCheck %s

// Splits require a rank-1 per-owner partial tile. Owner-tiled
// multi-dimensional results keep the unsplit per-owner reduction while rank-1
// coarse tiles remain splittable.

module attributes {carts.logical_total_workers = 16 : i64} {
  // CHECK-LABEL: func.func @owner_tiled_result_declines_split
  // CHECK: codir.codelet
  // CHECK-SAME: partial_reduction_dep_result_dim_maps = {{\[}}[0, 0], [0, -1]]
  // CHECK-SAME: partial_reduction_owner_dims = [0]
  // CHECK-NOT: partial_reduction_split_required
  func.func @owner_tiled_result_declines_split() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %cst = arith.constant 0.000000e+00 : f64
    %res = memref.alloc() : memref<16x8xf64>
    %in = memref.alloc() : memref<128x128xf64>
    scf.for %arg = %c0 to %c1 step %c1 {
      codir.codelet deps(%res, %in : memref<16x8xf64>, memref<128x128xf64>)
          params(%arg : index) attributes {
            dep_modes = [#codir.access_mode<readwrite>, #codir.access_mode<read>],
            partial_reduction,
            partial_reduction_owner_dims = [0],
            partial_reduction_dims = [1],
            tile_owner_dims = [0],
            tile_shape = [8]} {
      ^bb0(%bres: memref<16x8xf64>, %bin: memref<128x128xf64>, %biv: index):
        %bc0 = arith.constant 0 : index
        %bc1 = arith.constant 1 : index
        %bcst = arith.constant 0.000000e+00 : f64
        memref.store %bcst, %bres[%bc0, %bc0] : memref<16x8xf64>
        %v = memref.load %bin[%bc0, %bc1] : memref<128x128xf64>
        codir.yield
      }
    }
    return
  }

  // CHECK-LABEL: func.func @rank_one_result_commits_split
  // CHECK: codir.codelet
  // CHECK-SAME: partial_reduction_owner_dims = [0]
  // CHECK-SAME: partial_reduction_split_required
  func.func @rank_one_result_commits_split() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %cst = arith.constant 0.000000e+00 : f64
    %res = memref.alloc() : memref<128xf64>
    %in = memref.alloc() : memref<128x128xf64>
    scf.for %arg = %c0 to %c1 step %c1 {
      codir.codelet deps(%res, %in : memref<128xf64>, memref<128x128xf64>)
          params(%arg : index) attributes {
            dep_modes = [#codir.access_mode<readwrite>, #codir.access_mode<read>],
            partial_reduction,
            partial_reduction_owner_dims = [0],
            partial_reduction_dims = [1],
            tile_owner_dims = [0],
            tile_shape = [64]} {
      ^bb0(%bres: memref<128xf64>, %bin: memref<128x128xf64>, %biv: index):
        %bc0 = arith.constant 0 : index
        %bc1 = arith.constant 1 : index
        %bcst = arith.constant 0.000000e+00 : f64
        memref.store %bcst, %bres[%bc0] : memref<128xf64>
        %v = memref.load %bin[%bc0, %bc1] : memref<128x128xf64>
        codir.yield
      }
    }
    return
  }

  // CHECK-LABEL: func.func @divrem_result_dim_maps
  // CHECK: codir.codelet
  // CHECK-SAME: partial_reduction_dep_result_dim_maps = {{\[}}[0, 1], [0, 1, -1]]
  func.func @divrem_result_dim_maps() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %res = memref.alloc() : memref<4x32xf64>
    %in = memref.alloc() : memref<4x32x128xf64>
    scf.for %arg = %c0 to %c1 step %c1 {
      codir.codelet deps(%res, %in : memref<4x32xf64>, memref<4x32x128xf64>)
          params(%arg, %arg : index, index) attributes {
            dep_modes = [#codir.access_mode<readwrite>, #codir.access_mode<read>],
            partial_reduction,
            partial_reduction_owner_dims = [0],
            partial_reduction_dims = [2],
            tile_owner_dims = [0],
            tile_shape = [32]} {
      ^bb0(%bres: memref<4x32xf64>, %bin: memref<4x32x128xf64>, %biv: index, %bred: index):
        %bc32 = arith.constant 32 : index
        %bcst = arith.constant 0.000000e+00 : f64
        %q0 = arith.divui %biv, %bc32 : index
        %r0 = arith.remui %biv, %bc32 : index
        %q1 = arith.divui %biv, %bc32 : index
        %r1 = arith.remui %biv, %bc32 : index
        memref.store %bcst, %bres[%q0, %r0] : memref<4x32xf64>
        %v = memref.load %bin[%q1, %r1, %bred] : memref<4x32x128xf64>
        codir.yield
      }
    }
    return
  }
}
