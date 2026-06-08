// RUN: %carts-compile %s --pass-pipeline='builtin.module(verify-codir,convert-codir-to-arts,realize-edt-distribution-plan,verify-arts-objects-only)' \
// RUN:   | %FileCheck %s --implicit-check-not=replicated_read

// Every owner-dim-shifted read-only input of a single-owner-dim owner-strip
// stencil is served by its own per-block halo exchange, not coarse replication.
// Both read-only inputs get block-native halo storage and ghost-row acquires,
// so halo coverage is per dependency and not limited to the first input.

// CHECK-DAG: db_alloc[<inout>, <heap>, <write>, <block>]{{.*}}planHaloShape = [1, 0]{{.*}}stencil_supported_block_halo
// CHECK-DAG: db_alloc[<inout>, <heap>, <write>, <block>]{{.*}}planHaloShape = [1, 0]{{.*}}stencil_supported_block_halo
// CHECK-DAG: arts.edt{{.*}}perBlockHaloExchange
// CHECK-DAG: arts.edt{{.*}}perBlockHaloExchange
// CHECK-DAG: db_acquire[<in>]{{.*}}partitioning(<block>){{.*}}element_offsets
// CHECK-DAG: db_acquire[<in>]{{.*}}partitioning(<block>){{.*}}element_offsets
// CHECK-DAG: db_acquire[<in>]{{.*}}partitioning(<block>){{.*}}element_offsets
// CHECK-DAG: db_acquire[<in>]{{.*}}partitioning(<block>){{.*}}element_offsets

module attributes {arts.runtime_total_nodes = 8 : i64, arts.runtime_total_workers = 512 : i64} {
  func.func @use(%v: f32) -> () { return }
  func.func @multi_ro_owner_strip_halo() {
    %c0 = arith.constant 0 : index
    %c8 = arith.constant 8 : index
    %c24 = arith.constant 24 : index
    %cst = arith.constant 1.0 : f32
    %out = memref.alloc() : memref<24x4xf32>
    %vx = memref.alloc() : memref<24x4xf32>
    %vy = memref.alloc() : memref<24x4xf32>
    memref.store %cst, %vx[%c0, %c0] : memref<24x4xf32>
    memref.store %cst, %vy[%c0, %c0] : memref<24x4xf32>
    scf.for %i = %c0 to %c24 step %c8 {
      codir.codelet deps(%out, %vx, %vy : memref<24x4xf32>, memref<24x4xf32>, memref<24x4xf32>) params(%i : index)
          attributes {dep_modes = [#codir.access_mode<write>, #codir.access_mode<read>, #codir.access_mode<read>],
                      dep_owner_dims = [[0], [0], [0]],
                      dep_storage_views = [#codir.storage_view<compute_block>, #codir.storage_view<compute_block>, #codir.storage_view<compute_block>],
                      dep_collectives = [#codir.collective<none>, #codir.collective<halo>, #codir.collective<halo>],
                      distribution_kind = #codir.distribution_kind<owner_compute>,
                      access_max_offsets = [1, 0], access_min_offsets = [-1, 0],
                      halo_shape = [1, 0],
                      iteration_topology = #codir.iteration_topology<owner_strip>,
                      logical_worker_slice = [8, 4],
                      pattern = #codir.pattern<stencil_tiling_nd>,
                      plan_owner_dims = [0],
                      repetition_structure = #codir.repetition_structure<full_timestep>,
                      spatial_dims = [0, 1],
                      tile_owner_dims = [0], tile_shape = [8, 4],
                      write_footprint = [0, 0]} {
      ^bb0(%bw: memref<24x4xf32>, %ax: memref<24x4xf32>, %ay: memref<24x4xf32>, %base: index):
        %ic1 = arith.constant 1 : index
        %ic4 = arith.constant 4 : index
        %ic8 = arith.constant 8 : index
        %ic23 = arith.constant 23 : index
        %er = arith.addi %base, %ic8 : index
        %e = arith.minui %er, %ic23 : index
        scf.for %row = %base to %e step %ic1 {
          scf.for %col = %ic1 to %ic4 step %ic1 {
            %up = arith.subi %row, %ic1 : index
            %dn = arith.addi %row, %ic1 : index
            %x1 = memref.load %ax[%up, %col] : memref<24x4xf32>
            %x2 = memref.load %ax[%dn, %col] : memref<24x4xf32>
            %y1 = memref.load %ay[%up, %col] : memref<24x4xf32>
            %y2 = memref.load %ay[%dn, %col] : memref<24x4xf32>
            %s0 = arith.addf %x1, %x2 : f32
            %s1 = arith.addf %s0, %y1 : f32
            %s = arith.addf %s1, %y2 : f32
            memref.store %s, %bw[%row, %col] : memref<24x4xf32>
          }
        }
        codir.yield
      }
    }
    %r = memref.load %out[%c0, %c0] : memref<24x4xf32>
    func.call @use(%r) : (f32) -> ()
    return
  }
}
