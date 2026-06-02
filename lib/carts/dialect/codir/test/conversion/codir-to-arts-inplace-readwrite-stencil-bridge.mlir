// RUN: %carts-compile %s --pass-pipeline='builtin.module(storage-planning)' \
// RUN:   | %FileCheck %s --check-prefix=CODIR
// RUN: %carts-compile %s --pipeline post-db-refinement --arts-config %inputs_dir/arts_multinode_8x64.cfg --distributed-db \
// RUN:   | %FileCheck %s --check-prefix=ARTS

// In-place readwrite stencils are the single writer and a halo reader of the
// same state. CODIR storage planning must name halo/block storage, and ARTS must
// realize that as a distributed per-block DB instead of keeping the writer in a
// coarse host-whole DB.

module attributes {arts.runtime_total_nodes = 8 : i64, arts.runtime_total_workers = 512 : i64} {
  func.func @inplace_readwrite_stencil_bridge() {
    %c0 = arith.constant 0 : index
    %c4 = arith.constant 4 : index
    %c7 = arith.constant 7 : index
    %c8 = arith.constant 8 : index
    %c15 = arith.constant 15 : index
    %c16 = arith.constant 16 : index
    %cst = arith.constant 1.000000e+00 : f32
    %A = memref.alloc() : memref<16x8xf32>
    memref.store %cst, %A[%c0, %c0] : memref<16x8xf32>
    scf.for %i = %c0 to %c15 step %c8 {
      scf.for %j = %c0 to %c7 step %c4 {
        codir.codelet deps(%A : memref<16x8xf32>) params(%i, %j : index, index)
            attributes {access_max_offsets = [1, 1],
                        access_min_offsets = [0, 0],
                        dep_modes = [#codir.access_mode<readwrite>],
                        dep_storage_views = [#codir.storage_view<host_whole>],
                        distribution_kind = #codir.distribution_kind<owner_compute>,
                        in_place_safe,
                        iteration_topology = #codir.iteration_topology<owner_tile>,
                        logical_worker_slice = [8, 4],
                        pattern = #codir.pattern<stencil_tiling_nd>,
                        plan_owner_dims = [0, 1],
                        tile_owner_dims = [0, 1],
                        tile_shape = [8, 4],
                        write_footprint = [1, 1]} {
        ^bb0(%arg0: memref<16x8xf32>, %base_i: index, %base_j: index):
          %inner_c1 = arith.constant 1 : index
          %ip1 = arith.addi %base_i, %inner_c1 : index
          %jp1 = arith.addi %base_j, %inner_c1 : index
          %center = memref.load %arg0[%base_i, %base_j] : memref<16x8xf32>
          %edge = memref.load %arg0[%ip1, %jp1] : memref<16x8xf32>
          %sum = arith.addf %center, %edge : f32
          memref.store %sum, %arg0[%base_i, %base_j] : memref<16x8xf32>
          codir.yield
        }
      }
    }
    %result = memref.load %A[%c0, %c0] : memref<16x8xf32>
    func.call @use(%result) : (f32) -> ()
    memref.dealloc %A : memref<16x8xf32>
    return
  }

  func.func private @use(f32)
}

// CODIR-LABEL: func.func @inplace_readwrite_stencil_bridge
// CODIR: codir.codelet
// CODIR-SAME: dep_collectives = [#codir.collective<halo>]
// CODIR-SAME: dep_owner_dims = [{{\[}}0, 1]]
// CODIR-SAME: dep_storage_views = [#codir.storage_view<compute_block>]

// ARTS-LABEL: func.func @inplace_readwrite_stencil_bridge
// ARTS: arts.db_alloc[<inout>, <heap>, <write>, <coarse>]
// ARTS-SAME: local_only
// ARTS: arts.db_alloc[<inout>, <heap>, <write>, <block>]
// ARTS-SAME: distributed
// ARTS-SAME: perBlockSingleWriterStencil
// ARTS-SAME: planOwnerDims = [0, 1]
// ARTS-SAME: planPhysicalBlockShape = [8, 4]
// ARTS-SAME: storage_bridge = #arts.storage_bridge<host_whole_to_compute_block>
// ARTS: arts.edt <task> <internode> route{{.*}}depPattern = #arts.dep_pattern<stencil_tiling_nd>
// ARTS-SAME: inPlaceSafe
// ARTS-SAME: planOwnerDims = [0, 1]
