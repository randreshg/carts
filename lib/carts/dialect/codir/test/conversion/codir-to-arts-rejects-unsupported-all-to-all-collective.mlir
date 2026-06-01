// RUN: not %carts-compile %s --pass-pipeline='builtin.module(verify-codir,materialize-sde-boundary-to-arts,convert-codir-to-arts,verify-arts-objects-only)' 2>&1 \
// RUN:   | %FileCheck %s

module attributes {arts.runtime_total_nodes = 4 : i64, arts.runtime_total_workers = 64 : i64} {
  func.func @reject_unsupported_all_to_all_bridge() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index
    %c8 = arith.constant 8 : index
    %c24 = arith.constant 24 : index
    %F = memref.alloc() : memref<24x4xf32>

    scf.for %i = %c0 to %c24 step %c8 {
      codir.codelet deps(%F : memref<24x4xf32>) params(%i : index)
          attributes {dep_collectives = [#codir.collective<all_to_all>],
                      dep_modes = [#codir.access_mode<write>],
                      dep_storage_views = [#codir.storage_view<phase_redistributed>],
                      dep_owner_dims = [[0]],
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

    %result = memref.load %F[%c0, %c0] : memref<24x4xf32>
    func.call @use(%result) : (f32) -> ()
    return
  }

  func.func private @use(f32)
}

// CHECK: cannot materialize CODIR collective all_to_all
