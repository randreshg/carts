// RUN: %carts-compile %s --pipeline post-db-refinement --arts-config %inputs_dir/arts_multinode_8x64.cfg --distributed-db \
// RUN:   | %FileCheck %s --implicit-check-not=replicatedRead

// cross_dim_stencil_3d read-only deps must use the compute_block halo-exchange
// path, not replicated_read. The replicate path produces a local_only DB that
// relies on independent per-node initialization, but lacks the halo-exchange
// ordering guarantees needed at partition boundaries in distributed execution.
// The compute_block path gives each block a halo slot and uses the
// stencil_supported_block_halo mechanism instead.

module attributes {arts.runtime_total_nodes = 8 : i64, arts.runtime_total_workers = 512 : i64} {
  func.func @cross_dim_stencil_3d_read_only_uses_block() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index
    %c8 = arith.constant 8 : index
    %c65536 = arith.constant 65536 : index
    %cst = arith.constant 1.000000e+00 : f32
    // A is read-only stencil input; B is write-only output.
    %A = memref.alloc() : memref<65536x4x4xf32>
    %B = memref.alloc() : memref<65536x4x4xf32>
    memref.store %cst, %A[%c0, %c0, %c0] : memref<65536x4x4xf32>
    scf.for %i = %c1 to %c65536 step %c8 {
      codir.codelet deps(%A, %B : memref<65536x4x4xf32>, memref<65536x4x4xf32>) params(%i : index)
          attributes {access_max_offsets = [1, 1, 1],
                      access_min_offsets = [0, 0, 0],
                      dep_modes = [#codir.access_mode<read>, #codir.access_mode<write>],
                      dep_storage_views = [#codir.storage_view<compute_block>, #codir.storage_view<compute_block>],
                      distribution_kind = #codir.distribution_kind<blocked>,
                      halo_shape = [1],
                      iteration_topology = #codir.iteration_topology<owner_strip>,
                      logical_worker_slice = [8, 4, 4],
                      pattern = #codir.pattern<cross_dim_stencil_3d>,
                      plan_owner_dims = [0, 1, 2],
                      spatial_dims = [0, 1, 2],
                      tile_owner_dims = [0],
                      tile_shape = [8, 4, 4],
                      write_footprint = [1, 1, 1]} {
      ^bb0(%arg0: memref<65536x4x4xf32>, %arg1: memref<65536x4x4xf32>, %base: index):
        %inner_c0 = arith.constant 0 : index
        %inner_c1 = arith.constant 1 : index
        %hi = arith.addi %base, %inner_c1 : index
        %val = memref.load %arg0[%hi, %inner_c0, %inner_c0] : memref<65536x4x4xf32>
        memref.store %val, %arg1[%base, %inner_c0, %inner_c0] : memref<65536x4x4xf32>
        codir.yield
      }
    }
    %result = memref.load %B[%c0, %c0, %c0] : memref<65536x4x4xf32>
    func.call @use(%result) : (f32) -> ()
    memref.dealloc %B : memref<65536x4x4xf32>
    memref.dealloc %A : memref<65536x4x4xf32>
    return
  }

  func.func private @use(f32)
}

// CHECK-LABEL: func.func @cross_dim_stencil_3d_read_only_uses_block
// CHECK: arts.edt <task> <internode>{{.*}}depPattern = #arts.dep_pattern<cross_dim_stencil_3d>
// CHECK-SAME: planHaloShape = [1]
// CHECK-SAME: stencil_supported_block_halo
