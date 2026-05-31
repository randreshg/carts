// RUN: %carts-compile %s --pipeline post-db-refinement --arts-config %inputs_dir/arts_multinode_8x64.cfg --distributed-db \
// RUN:   | %FileCheck %s --implicit-check-not=replicatedRead

// stencil_tiling_nd read-only deps with multiple tile owner dims (2D tile
// distribution) must use the compute_block halo-exchange path, not
// replicated_read. With two owner dims, each per-node tile is a 2D shard;
// the local_only replicate path cannot guarantee correct boundary values
// across partition edges in either dimension. The compute_block path gives
// each tile a halo slot and uses the stencil_supported_block_halo mechanism
// instead. (conv-2d megalarge regression: 2n checksum mismatch with
// tile_owner_dims = [0, 1].)

module attributes {arts.runtime_total_nodes = 8 : i64, arts.runtime_total_workers = 512 : i64} {
  func.func @stencil_tiling_nd_2d_read_only_uses_block() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c16 = arith.constant 16 : index
    %c32 = arith.constant 32 : index
    %c64000 = arith.constant 64000 : index
    %cst = arith.constant 1.000000e+00 : f32
    // A is read-only stencil input; B is write-only output.
    %A = memref.alloc() : memref<64000x64000xf32>
    %B = memref.alloc() : memref<64000x64000xf32>
    memref.store %cst, %A[%c0, %c0] : memref<64000x64000xf32>
    scf.for %i = %c1 to %c64000 step %c16 {
      scf.for %j = %c1 to %c64000 step %c32 {
        codir.codelet deps(%A, %B : memref<64000x64000xf32>, memref<64000x64000xf32>) params(%i, %j : index, index)
            attributes {access_max_offsets = [1, 1],
                        access_min_offsets = [-1, -1],
                        dep_modes = [#codir.access_mode<read>, #codir.access_mode<write>],
                        dep_storage_views = [#codir.storage_view<compute_block>, #codir.storage_view<compute_block>],
                        distribution_kind = #codir.distribution_kind<owner_compute>,
                        halo_shape = [1, 1],
                        iteration_topology = #codir.iteration_topology<owner_tile>,
                        logical_worker_slice = [16, 32],
                        pattern = #codir.pattern<stencil_tiling_nd>,
                        plan_owner_dims = [0, 1],
                        spatial_dims = [0, 1],
                        tile_owner_dims = [0, 1],
                        tile_shape = [16, 32],
                        write_footprint = [1, 1]} {
        ^bb0(%arg0: memref<64000x64000xf32>, %arg1: memref<64000x64000xf32>, %base_i: index, %base_j: index):
          %inner_c0 = arith.constant 0 : index
          %inner_c1 = arith.constant 1 : index
          %lo_i = arith.subi %base_i, %inner_c1 : index
          %hi_i = arith.addi %base_i, %inner_c1 : index
          %lo_j = arith.subi %base_j, %inner_c1 : index
          %hi_j = arith.addi %base_j, %inner_c1 : index
          %v0 = memref.load %arg0[%lo_i, %lo_j] : memref<64000x64000xf32>
          %v1 = memref.load %arg0[%base_i, %base_j] : memref<64000x64000xf32>
          %v2 = memref.load %arg0[%hi_i, %hi_j] : memref<64000x64000xf32>
          %sum01 = arith.addf %v0, %v1 : f32
          %sum = arith.addf %sum01, %v2 : f32
          memref.store %sum, %arg1[%base_i, %base_j] : memref<64000x64000xf32>
          codir.yield
        }
      }
    }
    %result = memref.load %B[%c0, %c0] : memref<64000x64000xf32>
    func.call @use(%result) : (f32) -> ()
    memref.dealloc %B : memref<64000x64000xf32>
    memref.dealloc %A : memref<64000x64000xf32>
    return
  }

  func.func private @use(f32)
}

// CHECK-LABEL: func.func @stencil_tiling_nd_2d_read_only_uses_block
// CHECK: %[[DB_GUID:.*]], %[[DB_PTR:.*]] = arts.db_alloc{{.*}}<block>{{.*}}sizes[%{{[^,]+}}, %{{[^]]+}}]{{.*}}elementSizes[%{{[^,]+}}, %{{[^]]+}}]
// CHECK-SAME: planOwnerDims = [0, 1]
// CHECK-SAME: planPhysicalBlockShape = [16, 32]
// CHECK: arts.db_acquire{{.*}}(%[[DB_GUID]] : {{.*}}, %[[DB_PTR]] : {{.*}}) partitioning(<block>), indices[], offsets[%{{[^,]+}}, %{{[^]]+}}], sizes[%{{[^,]+}}, %{{[^]]+}}]
// CHECK: arts.edt <task>{{.*}}depPattern = #arts.dep_pattern<stencil_tiling_nd>
// CHECK-SAME: planHaloShape = [1, 1]
// CHECK-SAME: stencil_supported_block_halo
