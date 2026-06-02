// RUN: %carts-compile %s --pipeline post-db-refinement --arts-config %inputs_dir/arts_64t.cfg \
// RUN:   | %FileCheck %s

// A single-node, multi-block alternating-buffer stencil still needs a real
// halo refresh before the stencil read phase when the read state was written by
// an earlier block copy phase in the same timestep.

module attributes {arts.runtime_total_nodes = 1 : i64, arts.runtime_total_workers = 64 : i64} {
  func.func @alternating_buffer_single_node_halo_refresh() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index
    %c8 = arith.constant 8 : index
    %c16 = arith.constant 16 : index
    %cst = arith.constant 1.000000e+00 : f32
    %f = memref.alloc() : memref<16x8xf32>
    %u = memref.alloc() : memref<16x8xf32>
    %unew = memref.alloc() : memref<16x8xf32>
    memref.store %cst, %f[%c0, %c0] : memref<16x8xf32>
    memref.store %cst, %u[%c0, %c0] : memref<16x8xf32>
    memref.store %cst, %unew[%c0, %c0] : memref<16x8xf32>
    scf.for %i = %c0 to %c16 step %c8 {
      scf.for %j = %c0 to %c8 step %c4 {
        codir.codelet deps(%unew, %u : memref<16x8xf32>, memref<16x8xf32>)
            params(%c16, %i, %j : index, index, index)
            attributes {dep_modes = [#codir.access_mode<read>, #codir.access_mode<write>],
                        dep_storage_views = [#codir.storage_view<host_whole>, #codir.storage_view<host_whole>],
                        distribution_kind = #codir.distribution_kind<blocked>,
                        iteration_topology = #codir.iteration_topology<owner_tile>,
                        logical_worker_slice = [8, 4],
                        pattern = #codir.pattern<uniform>,
                        repetition_structure = #codir.repetition_structure<full_timestep>,
                        tile_owner_dims = [0, 1],
                        tile_shape = [8, 4]} {
        ^bb0(%src: memref<16x8xf32>, %dst: memref<16x8xf32>, %n: index, %base_i: index, %base_j: index):
          %v = memref.load %src[%base_i, %base_j] : memref<16x8xf32>
          memref.store %v, %dst[%base_i, %base_j] : memref<16x8xf32>
          codir.yield
        }
        codir.codelet deps(%f, %unew, %u : memref<16x8xf32>, memref<16x8xf32>, memref<16x8xf32>)
            params(%c16, %i, %j : index, index, index)
            attributes {access_max_offsets = [1, 1],
                        access_min_offsets = [-1, -1],
                        dep_modes = [#codir.access_mode<read>, #codir.access_mode<write>, #codir.access_mode<read>],
                        dep_storage_views = [#codir.storage_view<host_whole>, #codir.storage_view<host_whole>, #codir.storage_view<host_whole>],
                        distribution_kind = #codir.distribution_kind<owner_compute>,
                        halo_shape = [1, 1],
                        iteration_topology = #codir.iteration_topology<owner_tile>,
                        logical_worker_slice = [8, 4],
                        pattern = #codir.pattern<alternating_buffer_stencil>,
                        plan_owner_dims = [0, 1],
                        repetition_structure = #codir.repetition_structure<full_timestep>,
                        spatial_dims = [0, 1],
                        tile_owner_dims = [0, 1],
                        tile_shape = [8, 4],
                        write_footprint = [1, 1]} {
        ^bb0(%forcing: memref<16x8xf32>, %dst: memref<16x8xf32>,
             %src: memref<16x8xf32>, %n: index, %base_i: index, %base_j: index):
          %inner_c1 = arith.constant 1 : index
          %row_m = arith.subi %base_i, %inner_c1 : index
          %row_p = arith.addi %base_i, %inner_c1 : index
          %col_m = arith.subi %base_j, %inner_c1 : index
          %col_p = arith.addi %base_j, %inner_c1 : index
          %f0 = memref.load %forcing[%base_i, %base_j] : memref<16x8xf32>
          %n0 = memref.load %src[%row_m, %base_j] : memref<16x8xf32>
          %s0 = memref.load %src[%row_p, %base_j] : memref<16x8xf32>
          %w0 = memref.load %src[%base_i, %col_m] : memref<16x8xf32>
          %e0 = memref.load %src[%base_i, %col_p] : memref<16x8xf32>
          %sum0 = arith.addf %n0, %s0 : f32
          %sum1 = arith.addf %w0, %e0 : f32
          %sum2 = arith.addf %sum0, %sum1 : f32
          %sum = arith.addf %sum2, %f0 : f32
          memref.store %sum, %dst[%base_i, %base_j] : memref<16x8xf32>
          codir.yield
        }
      }
    }
    %result = memref.load %unew[%c0, %c0] : memref<16x8xf32>
    func.call @use(%result) : (f32) -> ()
    memref.dealloc %unew : memref<16x8xf32>
    memref.dealloc %u : memref<16x8xf32>
    memref.dealloc %f : memref<16x8xf32>
    return
  }

  func.func private @use(f32)
}

// CHECK-LABEL: func.func @alternating_buffer_single_node_halo_refresh
// CHECK: arts.db_alloc[<inout>, <heap>, <write>, <block>]{{.*}}perBlockSingleWriterStencil
// CHECK-SAME: stencil_supported_block_halo
// CHECK: arts.db_acquire[<in>]{{.*}}bounds_valid
// CHECK: arts.edt <task> <intranode>{{.*}}perBlockHaloExchange
// CHECK-SAME: storageBridgeCopy
// CHECK-NOT: arts.runtime_query <total_nodes>
// CHECK: arts.barrier
// CHECK: arts.edt <task> <intranode>{{.*}}depPattern = #arts.dep_pattern<alternating_buffer_stencil>
