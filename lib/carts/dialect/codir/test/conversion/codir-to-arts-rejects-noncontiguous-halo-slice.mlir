// RUN: not %carts-compile %s --pipeline post-db-refinement --arts-config %inputs_dir/arts_multinode_8x64.cfg 2>&1 \
// RUN:   | %FileCheck %s

// A per-block halo bridge must send compact contiguous halo windows. If the
// committed owner dimension selects a strided face, CODIR must fail closed
// instead of emitting a full-block acquire.

// CHECK: cannot materialize non-contiguous lower halo destination slice

module attributes {arts.runtime_total_nodes = 8 : i64, arts.runtime_total_workers = 512 : i64} {
  func.func @reject_noncontiguous_halo_slice() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index
    %c8 = arith.constant 8 : index
    %c24 = arith.constant 24 : index
    %A = memref.alloc() : memref<4x24xf32>

    scf.for %j = %c0 to %c24 step %c8 {
      codir.codelet deps(%A : memref<4x24xf32>) params(%j : index)
          attributes {dep_modes = [#codir.access_mode<readwrite>],
                      dep_owner_dims = [[1]],
                      dep_storage_views = [#codir.storage_view<phase_redistributed>],
                      dep_collectives = [#codir.collective<halo>],
                      distribution_kind = #codir.distribution_kind<blocked>,
                      halo_shape = [0, 1],
                      iteration_topology = #codir.iteration_topology<owner_strip>,
                      logical_worker_slice = [4, 8],
                      pattern = #codir.pattern<stencil_tiling_nd>,
                      tile_owner_dims = [1],
                      tile_shape = [4, 8]} {
      ^bb0(%arg0: memref<4x24xf32>, %base: index):
        %inner_c0 = arith.constant 0 : index
        %inner_c1 = arith.constant 1 : index
        %inner_c4 = arith.constant 4 : index
        %inner_c8 = arith.constant 8 : index
        %inner_c23 = arith.constant 23 : index
        %end_raw = arith.addi %base, %inner_c8 : index
        %end = arith.minui %end_raw, %inner_c23 : index
        scf.for %row = %inner_c0 to %inner_c4 step %inner_c1 {
          scf.for %col = %base to %end step %inner_c1 {
            %left = arith.subi %col, %inner_c1 : index
            %right = arith.addi %col, %inner_c1 : index
            %a0 = memref.load %arg0[%row, %left] : memref<4x24xf32>
            %a1 = memref.load %arg0[%row, %right] : memref<4x24xf32>
            %sum = arith.addf %a0, %a1 : f32
            memref.store %sum, %arg0[%row, %col] : memref<4x24xf32>
          }
        }
        codir.yield
      }
    }

    %result = memref.load %A[%c0, %c0] : memref<4x24xf32>
    func.call @use(%result) : (f32) -> ()
    memref.dealloc %A : memref<4x24xf32>
    return
  }

  func.func private @use(f32)
}
