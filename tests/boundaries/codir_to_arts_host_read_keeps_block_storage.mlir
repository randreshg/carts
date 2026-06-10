// RUN: %carts-compile %s --pass-pipeline='builtin.module(verify-codir,convert-codir-to-arts,verify-arts-objects-only)' \
// RUN:   --arts-config %inputs_dir/arts_8t.cfg \
// RUN:   | %FileCheck %s --implicit-check-not=codir.

// A compute_block writer array consumed through static-typed memref.subview
// dependencies must keep its budget BLOCK DB as canonical MU storage even when
// the same array is read on the host outside any scheduling unit. The host read
// is served by a coarse view; the compute_block subviews stay on the block DB.
//
// Regression: the host-whole bridge used to swap a coarse dynamic memref under
// the already static-typed compute_block subviews (verifier-rejected
// static-strided subview over a dynamic source) and to leave an orphan second
// block DB that no compute dep used. The block DB must sit UNDER every
// compute_block subview; only the genuine host load is served from a coarse DB.

// CHECK-LABEL: func.func @host_read_keeps_block_storage
// The lone host read is served by a coarse DB.
// CHECK: arts.db_alloc[<inout>, <heap>, <write>, <coarse>]
// The compute_block deps are served by exactly one block DB (no orphan second
// block DB), tagged as the host-whole -> compute-block storage bridge, and the
// dynamic block payload is cast back to the MU's static type so the
// compute_block subviews keep static strides.
// CHECK: arts.db_alloc[<inout>, <heap>, <write>, <block>]
// CHECK-SAME: storage_bridge = #arts.storage_bridge<host_whole_to_compute_block>
// CHECK: memref.cast %{{.*}} : memref<?x?xf32> to memref<8x8xf32>
// CHECK: arts.edt

// No second (orphan) block DB is emitted after the single committed one.
// CHECK-NOT: arts.db_alloc[<inout>, <heap>, <write>, <block>]

module attributes {arts.runtime_total_nodes = 1 : i64, arts.runtime_total_workers = 8 : i64} {
  func.func @host_read_keeps_block_storage() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c8 = arith.constant 8 : index

    %mu = sde.mu_alloc : memref<8x8xf32>

    // Two static-typed identity subviews of the same MU, both consumed as
    // compute_block deps inside one dispatch loop (init writer + stencil reader).
    %wview = memref.subview %mu[0, 0] [8, 8] [1, 1] : memref<8x8xf32> to memref<8x8xf32>
    %rview = memref.subview %mu[0, 0] [8, 8] [1, 1] : memref<8x8xf32> to memref<8x8xf32>

    scf.for %b = %c0 to %c8 step %c1 {
      // Writer codelet: owns block rows over the MU through a subview.
      codir.codelet deps(%wview : memref<8x8xf32>) params(%b : index)
          attributes {dep_collectives = [#codir.collective<none>],
                      dep_modes = [#codir.access_mode<write>],
                      dep_owner_dims = [[0]],
                      dep_storage_views = [#codir.storage_view<compute_block>],
                      iteration_topology = #codir.iteration_topology<owner_strip>,
                      logical_worker_slice = [8],
                      pattern = #codir.pattern<uniform>,
                      tile_owner_dims = [0],
                      tile_shape = [8],
                      write_footprint = [8]} {
      ^bb0(%arg0: memref<8x8xf32>, %block: index):
        %icst = arith.constant 0 : index
        %one = arith.constant 1.0 : f32
        memref.store %one, %arg0[%block, %icst] : memref<8x8xf32>
        codir.yield
      }
      // Stencil reader codelet: compute_block read over the same MU through a
      // second subview, in the same dispatch loop.
      codir.codelet deps(%rview : memref<8x8xf32>) params(%b : index)
          attributes {access_max_offsets = [0, 0],
                      access_min_offsets = [0, 0],
                      dep_collectives = [#codir.collective<none>],
                      dep_modes = [#codir.access_mode<read>],
                      dep_owner_dims = [[0]],
                      dep_storage_views = [#codir.storage_view<compute_block>],
                      iteration_topology = #codir.iteration_topology<owner_strip>,
                      logical_worker_slice = [8],
                      pattern = #codir.pattern<uniform>,
                      tile_owner_dims = [0],
                      tile_shape = [8]} {
      ^bb0(%arg0: memref<8x8xf32>, %block: index):
        %icst = arith.constant 0 : index
        %v = memref.load %arg0[%block, %icst] : memref<8x8xf32>
        codir.yield
      }
    }

    // Host read outside any scheduling unit: served by the coarse view, must
    // NOT pull the compute_block subviews onto a coarse dynamic source.
    scf.for %i = %c0 to %c8 step %c1 {
      %hv = memref.load %mu[%i, %i] : memref<8x8xf32>
    }
    return
  }
}
