// RUN: %carts-compile %s --pipeline post-db-refinement --arts-config %inputs_dir/arts_multinode_8x64.cfg --distributed-db \
// RUN:   | %FileCheck %s

// A committed compute_block+halo stencil read must materialize a real
// per-block halo exchange even when an earlier host-whole writer is not a
// compatible block participant in the same bridge.

module attributes {arts.runtime_total_nodes = 8 : i64, arts.runtime_total_workers = 512 : i64} {
  func.func @readonly_halo_bridge_materializes_exchange() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c2 = arith.constant 2 : index
    %c4 = arith.constant 4 : index
    %c8 = arith.constant 8 : index
    %c16 = arith.constant 16 : index
    %cst = arith.constant 1.000000e+00 : f32
    %forcing = memref.alloc() : memref<16x8xf32>
    %state = memref.alloc() : memref<16x8xf32>
    %next = memref.alloc() : memref<16x8xf32>
    memref.store %cst, %forcing[%c0, %c0] : memref<16x8xf32>

    scf.for %i = %c0 to %c16 step %c8 {
      scf.for %j = %c0 to %c8 step %c2 {
        codir.codelet deps(%forcing, %state : memref<16x8xf32>, memref<16x8xf32>)
            params(%c16, %i, %j : index, index, index)
            attributes {dep_collectives = [#codir.collective<none>, #codir.collective<none>],
                        dep_modes = [#codir.access_mode<read>, #codir.access_mode<write>],
                        dep_owner_dims = [[0, 1], [0, 1]],
                        dep_storage_views = [#codir.storage_view<host_whole>, #codir.storage_view<host_whole>],
                        distribution_kind = #codir.distribution_kind<blocked>,
                        iteration_topology = #codir.iteration_topology<owner_tile>,
                        logical_worker_slice = [16, 2],
                        pattern = #codir.pattern<uniform>,
                        repetition_structure = #codir.repetition_structure<full_timestep>,
                        tile_owner_dims = [0, 1],
                        tile_shape = [8, 2]} {
        ^bb0(%src: memref<16x8xf32>, %dst: memref<16x8xf32>, %n: index, %base_i: index, %base_j: index):
          %v = memref.load %src[%base_i, %base_j] : memref<16x8xf32>
          memref.store %v, %dst[%base_i, %base_j] : memref<16x8xf32>
          codir.yield
        }
      }
    }

    scf.for %i = %c0 to %c16 step %c8 {
      scf.for %j = %c0 to %c8 step %c4 {
        codir.codelet deps(%forcing, %next, %state : memref<16x8xf32>, memref<16x8xf32>, memref<16x8xf32>)
            params(%c16, %i, %j : index, index, index)
            attributes {access_max_offsets = [1, 1],
                        access_min_offsets = [-1, -1],
                        dep_collectives = [#codir.collective<none>, #codir.collective<none>, #codir.collective<halo>],
                        dep_modes = [#codir.access_mode<read>, #codir.access_mode<write>, #codir.access_mode<read>],
                        dep_owner_dims = [[0, 1], [0, 1], [0, 1]],
                        dep_storage_views = [#codir.storage_view<compute_block>, #codir.storage_view<compute_block>, #codir.storage_view<compute_block>],
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
        ^bb0(%f: memref<16x8xf32>, %dst: memref<16x8xf32>,
             %src: memref<16x8xf32>, %n: index, %base_i: index, %base_j: index):
          %inner_c1 = arith.constant 1 : index
          %row_m = arith.subi %base_i, %inner_c1 : index
          %row_p = arith.addi %base_i, %inner_c1 : index
          %col_m = arith.subi %base_j, %inner_c1 : index
          %col_p = arith.addi %base_j, %inner_c1 : index
          %f0 = memref.load %f[%base_i, %base_j] : memref<16x8xf32>
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

    %result = memref.load %next[%c0, %c0] : memref<16x8xf32>
    func.call @use(%result) : (f32) -> ()
    memref.dealloc %next : memref<16x8xf32>
    memref.dealloc %state : memref<16x8xf32>
    memref.dealloc %forcing : memref<16x8xf32>
    return
  }

  func.func private @use(f32)
}

// CHECK-LABEL: func.func @readonly_halo_bridge_materializes_exchange
// CHECK: arts.db_alloc[<inout>, <heap>, <write>, <block>]{{.*}}perBlockSingleWriterStencil{{.*}}planHaloShape = [1, 1]{{.*}}stencil_supported_block_halo{{.*}}storage_bridge = #arts.storage_bridge<host_whole_to_compute_block>
// CHECK: arts.edt <task> <internode> route{{.*}}perBlockHaloExchange
// CHECK-SAME: storageBridgeCopy
// CHECK: arts.edt <task> <internode> route{{.*}}depPattern = #arts.dep_pattern<alternating_buffer_stencil>
// CHECK-SAME: planHaloShape = [1, 1]
