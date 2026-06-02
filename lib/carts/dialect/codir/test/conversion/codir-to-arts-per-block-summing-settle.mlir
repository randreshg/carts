// RUN: %carts-compile %s --pass-pipeline='builtin.module(verify-codir,materialize-sde-boundary-to-arts,convert-codir-to-arts)' \
// RUN:   | %FileCheck %s

// Per-block single-writer summing settle, the arith.addf dual of the per-block
// all-gather. CODIR has already committed reduce_scatter on the producer
// storage bridge, so CODIR-to-ARTS must realize a block-native settle: one
// replicated per-block result DB, RO partial-block deps, and one output-only
// settled block dep per lane. This test stays at the CODIR-to-ARTS boundary; an
// unsplit transpose-reduce consumer that reads tmp across the reduction IV is
// not block-local and must be transformed before it can use compute_block
// storage.

module attributes {arts.runtime_total_nodes = 2 : i64, arts.runtime_total_workers = 16 : i64} {
  func.func @per_block_summing_settle() {
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
                      partial_reduction_split_factor = 2,
                      pattern = #codir.pattern<elementwise_pipeline>,
                      tile_owner_dims = [0],
                      tile_shape = [16]} {
      ^bb0(%argT: memref<128xf64>, %owner: index):
        %ic1 = arith.constant 1 : index
        %ic16 = arith.constant 16 : index
        %czero = arith.constant 0.000000e+00 : f64
        %end = arith.addi %owner, %ic16 : index
        scf.for %j = %owner to %end step %ic1 {
          memref.store %czero, %argT[%j] : memref<128xf64>
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

// CHECK-LABEL: func.func @per_block_summing_settle

// The per-block summing settle creates a REPLICATED <block> DB (local_only +
// perBlockReplicated: all settled blocks present on every node, each its own
// GUID), NOT a single coarse <inout> replica. This is the race-free substrate:
// each settled block is a distinct DB, so the exclusive-write frontier
// degenerates to one uncontended writer per block.
// CHECK: %[[SGUID:.*]], %[[SPTR:.*]] = arts.db_alloc[<inout>, <heap>, <write>, <block>] {{.*}}{local_only, perBlockReplicated

// One flat launch loop covers every (node, block-group) pair. The derived
// node ordinal routes the task, while the derived block ordinal selects each
// settled block. DB grain stays per block.
// CHECK: arts.runtime_query <total_nodes>
// CHECK: arith.ceildivui
// CHECK: scf.for {{.*}} step %c1
// CHECK: arith.divui
// CHECK: arith.remui
// The P per-tile partials are RO-acquired (<in>) on distinct block GUIDs; the
// settled block is acquired output-only on its OWN distinct block DB by exactly
// one EDT (single writer, no shared exclusive-write frontier).
// CHECK: arts.db_acquire[<in>] {{.*}} partitioning(<block>)
// CHECK: arts.db_acquire[<in>] {{.*}} partitioning(<block>)
// CHECK: arts.db_acquire[<{{inout|out}}>] (%[[SGUID]] : {{.*}}, %[[SPTR]] : {{.*}}) partitioning(<block>)
// CHECK: arts.edt <task>
// CHECK-SAME: perBlockSummingSettle
// CHECK-SAME: storageBridgeCopy

// The settle body sums the partials with arith.addf and stores ONCE into the
// settled block (no RW accumulator, no read-modify-write of a shared buffer).
// CHECK: arts.db_ref
// CHECK: arith.addf
// CHECK: memref.store
// CHECK: arts.barrier
