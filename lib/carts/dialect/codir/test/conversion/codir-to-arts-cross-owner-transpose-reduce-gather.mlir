// RUN: %carts-compile %s --arts-config %inputs_dir/arts_multinode.cfg \
// RUN:   --pass-pipeline='builtin.module(verify-codir,materialize-sde-boundary-to-arts,convert-codir-to-arts)' \
// RUN:   | %FileCheck %s

// A cross-owner transpose-reduce producer has already committed
// reduce_scatter/phase_redistributed storage for the intermediate. This
// boundary fixture checks the ARTS realization of the copy-out it requires:
// every node must assemble the complete coarse host view from the per-block
// substrate, with one flattened (node, block-group) launch loop. The invalid
// unsplit transpose consumer shape is intentionally not modeled here; a
// consumer that reads tmp across the reduction IV is not block-local until an
// upstream transform materializes split partials or a proper host bridge.

module attributes {arts.runtime_total_nodes = 2 : i64, arts.runtime_total_workers = 16 : i64} {
  func.func @cross_owner_reduce_scatter_copy_out() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c16 = arith.constant 16 : index
    %c128 = arith.constant 128 : index
    %tmp = memref.alloc() : memref<128xf64>

    scf.for %i = %c0 to %c128 step %c16 {
      codir.codelet deps(%tmp : memref<128xf64>)
          params(%i : index)
          attributes {dep_collectives = [#codir.collective<reduce_scatter>],
                      dep_modes = [#codir.access_mode<write>],
                      dep_owner_dims = [[0]],
                      dep_storage_views = [#codir.storage_view<phase_redistributed>],
                      distribution_kind = #codir.distribution_kind<blocked>,
                      iteration_topology = #codir.iteration_topology<owner_strip>,
                      logical_worker_slice = [16],
                      pattern = #codir.pattern<elementwise_pipeline>,
                      tile_owner_dims = [0],
                      tile_shape = [16]} {
      ^bb0(%argT: memref<128xf64>, %owner: index):
        %ic1 = arith.constant 1 : index
        %ic16 = arith.constant 16 : index
        %zero = arith.constant 0.000000e+00 : f64
        %end = arith.addi %owner, %ic16 : index
        scf.for %j = %owner to %end step %ic1 {
          memref.store %zero, %argT[%j] : memref<128xf64>
        }
        codir.yield
      }
    }

    %result = memref.load %tmp[%c0] : memref<128xf64>
    func.call @use(%result) : (f64) -> ()
    memref.dealloc %tmp : memref<128xf64>
    return
  }

  func.func private @use(f64)
}

// CHECK-LABEL: func.func @cross_owner_reduce_scatter_copy_out

// The copy-out is one flat launch loop over every (node, block-group) pair.
// The derived node ordinal routes the EDT; the derived block ordinal selects
// per-block DBs. DB/MU grain stays per block.
// CHECK: arts.runtime_query <total_nodes>
// CHECK: arith.ceildivui
// CHECK: scf.for {{.*}} step %c1
// CHECK: arith.divui
// CHECK: arith.remui
// CHECK: arts.db_acquire[<inout>] {{.*}} partitioning(<coarse>)
// CHECK: arts.db_acquire[<in>] {{.*}} partitioning(<block>)
// CHECK: arts.edt <task> <internode>
// CHECK-SAME: storageBridgeCopy
// CHECK: arts.barrier
