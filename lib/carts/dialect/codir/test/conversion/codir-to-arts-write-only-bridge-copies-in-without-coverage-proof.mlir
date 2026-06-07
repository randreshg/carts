// RUN: %carts-compile %s --pass-pipeline='builtin.module(verify-codir,storage-planning,verify-codir,materialize-sde-boundary-to-arts,convert-codir-to-arts)' \
// RUN:   --arts-config %inputs_dir/arts_multinode_8x64.cfg | %FileCheck %s

// A write-only compute-block host bridge whose writer covers only part of its
// block must be seeded from the host view first. Coverage is decided from the
// committed write_footprint, not from the source pattern: this writer carries a
// pattern but no committed write_footprint, so full coverage is NOT proven and
// the initial copy-in is materialized anyway. The source pattern no longer
// suppresses the seed.

// CHECK-LABEL: func.func @write_only_bridge_copies_in_without_coverage_proof
// CHECK: %[[HOST:.*]] = arts.db_ref
// CHECK: arts.db_alloc
// CHECK-SAME: <block>
// CHECK-SAME: storage_bridge = #arts.storage_bridge<host_whole_to_compute_block>
// CHECK: arts.db_acquire[<in>]
// CHECK-SAME: partitioning(<coarse>)
// CHECK: arts.db_acquire[<out>]
// CHECK-SAME: partitioning(<block>)
// CHECK: arts.edt <task> <internode>
// CHECK-SAME: storageBridgeCopy

module attributes {arts.runtime_total_nodes = 8 : i64, arts.runtime_total_workers = 512 : i64} {
  func.func private @use(f32)

  func.func @write_only_bridge_copies_in_without_coverage_proof() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c8 = arith.constant 8 : index
    %c18 = arith.constant 18 : index
    %A = memref.alloc() : memref<18x4xf32>
    scf.for %i = %c0 to %c18 step %c8 {
      codir.codelet deps(%A : memref<18x4xf32>) params(%i : index)
          attributes {dep_modes = [#codir.access_mode<write>],
                      dep_storage_views = [#codir.storage_view<compute_block>],
                      distribution_kind = #codir.distribution_kind<blocked>,
                      iteration_topology = #codir.iteration_topology<owner_strip>,
                      logical_worker_slice = [8, 4],
                      pattern = #codir.pattern<uniform>,
                      tile_owner_dims = [0],
                      tile_shape = [8, 4]} {
      ^bb0(%arg0: memref<18x4xf32>, %base: index):
        %inner_c0 = arith.constant 0 : index
        %inner_cst = arith.constant 2.000000e+00 : f32
        memref.store %inner_cst, %arg0[%base, %inner_c0] : memref<18x4xf32>
        codir.yield
      }
    }
    %result = memref.load %A[%c0, %c0] : memref<18x4xf32>
    func.call @use(%result) : (f32) -> ()
    memref.dealloc %A : memref<18x4xf32>
    return
  }
}
