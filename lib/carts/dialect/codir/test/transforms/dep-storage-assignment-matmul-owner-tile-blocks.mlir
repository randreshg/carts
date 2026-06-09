// RUN: %carts-compile %s --pass-pipeline='builtin.module(verify-codir,dep-storage-assignment,verify-codir)' \
// RUN:   | %FileCheck %s

module {
  func.func @matmul_owner_tile_deps_stay_block_native() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c16 = arith.constant 16 : index
    %c128 = arith.constant 128 : index
    %cst = arith.constant 0.000000e+00 : f64
    %a = memref.alloc() : memref<128x128xf64>
    %b = memref.alloc() : memref<128x128xf64>
    %c = memref.alloc() : memref<128x128xf64>
    scf.for %i = %c0 to %c128 step %c16 {
      scf.for %j = %c0 to %c128 step %c16 {
        codir.codelet deps(%a, %b, %c : memref<128x128xf64>, memref<128x128xf64>, memref<128x128xf64>)
            params(%i, %j : index, index)
            attributes {dep_modes = [#codir.access_mode<read>, #codir.access_mode<read>, #codir.access_mode<readwrite>],
                        dep_storage_views = [#codir.storage_view<host_whole>, #codir.storage_view<host_whole>, #codir.storage_view<host_whole>],
                        distribution_kind = #codir.distribution_kind<blocked>,
                        iteration_topology = #codir.iteration_topology<owner_tile_2d>,
                        logical_worker_slice = [16, 16],
                        pattern = #codir.pattern<matmul>,
                        tile_owner_dims = [0, 1],
                        tile_shape = [16, 16]} {
        ^bb0(%arg0: memref<128x128xf64>, %arg1: memref<128x128xf64>, %arg2: memref<128x128xf64>, %base_i: index, %base_j: index):
          %inner_c0 = arith.constant 0 : index
          %inner_c1 = arith.constant 1 : index
          %inner_c16 = arith.constant 16 : index
          %inner_c128 = arith.constant 128 : index
          %inner_cst = arith.constant 0.000000e+00 : f64
          %i_end_raw = arith.addi %base_i, %inner_c16 : index
          %i_end = arith.minui %i_end_raw, %inner_c128 : index
          scf.for %row = %base_i to %i_end step %inner_c1 {
            %j_end_raw = arith.addi %base_j, %inner_c16 : index
            %j_end = arith.minui %j_end_raw, %inner_c128 : index
            scf.for %col = %base_j to %j_end step %inner_c1 {
              memref.store %inner_cst, %arg2[%row, %col] : memref<128x128xf64>
              scf.for %k = %inner_c0 to %inner_c128 step %inner_c1 {
                %lhs = memref.load %arg0[%row, %k] : memref<128x128xf64>
                %rhs = memref.load %arg1[%k, %col] : memref<128x128xf64>
                %mul = arith.mulf %lhs, %rhs : f64
                %old = memref.load %arg2[%row, %col] : memref<128x128xf64>
                %sum = arith.addf %old, %mul : f64
                memref.store %sum, %arg2[%row, %col] : memref<128x128xf64>
              }
            }
          }
          codir.yield
        }
      }
    }
    memref.dealloc %c : memref<128x128xf64>
    memref.dealloc %b : memref<128x128xf64>
    memref.dealloc %a : memref<128x128xf64>
    func.return
  }
}

// CHECK-LABEL: func.func @matmul_owner_tile_deps_stay_block_native
// CHECK: codir.codelet
// CHECK-SAME: dep_owner_dims = [{{\[}}0], [1], [0, 1]]
// CHECK-SAME: dep_storage_views = [#codir.storage_view<compute_block>, #codir.storage_view<compute_block>, #codir.storage_view<compute_block>]
// CHECK-SAME: pattern = #codir.pattern<matmul>
