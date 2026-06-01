// RUN: %carts-compile %s --arts-config %inputs_dir/arts_multinode.cfg \
// RUN:   --pass-pipeline='builtin.module(reduction-planning,storage-planning,verify-codir,materialize-sde-boundary-to-arts,convert-codir-to-arts)' \
// RUN:   | %FileCheck %s

// Per-block single-writer summing settle, the
// arith.addf dual of the per-block all-gather. This reuses the cross-owner
// transpose-matvec reduce shape (atax y = A^T(Ax) / bicg s = A^T r): a producer
// writes the intermediate `tmp` block-distributed by row; a transpose-reduce
// codelet owns its output by column and contracts over the matrix ROW dim
// (reduce_scatter). The producer here OPTS IN to the block-native realization
// via `emit_block_native_settle` (+ a `partial_reduction_split_factor` naming
// the per-tile partial count). The compiler must then emit, on top of the
// legacy coarse gather, a per-block summing settle: an outer per-node loop
// driving one block loop, where each settled block is its OWN <block> DB written
// output-only (<out>) ONCE, accumulating the P per-tile partial blocks
// (RO-acquired <in>) with `+=`. Distinct DB per settled block => single
// uncontended writer => no shared exclusive-write frontier => race-free AND
// concurrent (the coarse-replica serialization removed at the root, in the
// reduction direction).
//
// Without `emit_block_native_settle`, this stays on the legacy coarse gather.

module attributes {arts.runtime_total_nodes = 2 : i64, arts.runtime_total_workers = 16 : i64} {
  func.func @per_block_summing_settle(%A: memref<128x128xf64>, %x: memref<128xf64>, %tmp: memref<128xf64>, %y: memref<128xf64>, %base: index) {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c16 = arith.constant 16 : index
    %c128 = arith.constant 128 : index

    // Producer: tmp = A*x, owner dim 0 (row), tmp written block-distributed.
    // Opts into the block-native summing settle with 2 per-tile partials.
    scf.for %i = %base to %c128 step %c16 {
      codir.codelet deps(%tmp, %A, %x : memref<128xf64>, memref<128x128xf64>, memref<128xf64>)
          params(%i : index)
          attributes {dep_modes = [#codir.access_mode<write>, #codir.access_mode<read>, #codir.access_mode<read>],
                      dep_storage_views = [#codir.storage_view<phase_redistributed>, #codir.storage_view<replicated_read>, #codir.storage_view<replicated_read>],
                      distribution_kind = #codir.distribution_kind<blocked>,
                      emit_block_native_settle,
                      iteration_topology = #codir.iteration_topology<owner_strip>,
                      logical_worker_slice = [16],
                      partial_reduction_split_factor = 2,
                      pattern = #codir.pattern<elementwise_pipeline>,
                      tile_owner_dims = [0],
                      tile_shape = [16]} {
      ^bb0(%argT: memref<128xf64>, %argA: memref<128x128xf64>, %argX: memref<128xf64>, %owner: index):
        %ic0 = arith.constant 0 : index
        %ic1 = arith.constant 1 : index
        %ic128 = arith.constant 128 : index
        %czero = arith.constant 0.000000e+00 : f64
        memref.store %czero, %argT[%owner] : memref<128xf64>
        scf.for %j = %ic0 to %ic128 step %ic1 {
          %t = memref.load %argT[%owner] : memref<128xf64>
          %a = memref.load %argA[%owner, %j] : memref<128x128xf64>
          %xv = memref.load %argX[%j] : memref<128xf64>
          %m = arith.mulf %a, %xv : f64
          %s = arith.addf %t, %m : f64
          memref.store %s, %argT[%owner] : memref<128xf64>
        }
        codir.yield
      }
    }

    // Transpose reduce: y = A^T*tmp. The matrix read A[reductionIV, owner]
    // reduces over A's leading (row) dim -> the cross-owner reduce_scatter
    // signature that gates the settle on the producer above.
    scf.for %jb = %base to %c128 step %c16 {
      codir.codelet deps(%y, %A, %tmp : memref<128xf64>, memref<128x128xf64>, memref<128xf64>)
          params(%jb : index)
          attributes {dep_modes = [#codir.access_mode<readwrite>, #codir.access_mode<read>, #codir.access_mode<read>],
                      dep_storage_views = [#codir.storage_view<compute_block>, #codir.storage_view<replicated_read>, #codir.storage_view<host_whole>],
                      distribution_kind = #codir.distribution_kind<blocked>,
                      iteration_topology = #codir.iteration_topology<owner_strip>,
                      logical_worker_slice = [16],
                      partial_reduction,
                      partial_reduction_dims = [0],
                      partial_reduction_owner_dims = [0],
                      pattern = #codir.pattern<elementwise_pipeline>,
                      tile_owner_dims = [0],
                      tile_shape = [16]} {
      ^bb0(%argY: memref<128xf64>, %argA: memref<128x128xf64>, %argT: memref<128xf64>, %owner: index):
        %ic0 = arith.constant 0 : index
        %ic1 = arith.constant 1 : index
        %ic128 = arith.constant 128 : index
        %old = memref.load %argY[%owner] : memref<128xf64>
        scf.for %i = %ic0 to %ic128 step %ic1 {
          %a = memref.load %argA[%i, %owner] : memref<128x128xf64>
          %tv = memref.load %argT[%i] : memref<128xf64>
          %m = arith.mulf %a, %tv : f64
          %next = arith.addf %old, %m : f64
          memref.store %next, %argY[%owner] : memref<128xf64>
        }
        codir.yield
      }
    }
    return
  }
}

// CHECK-LABEL: func.func @per_block_summing_settle

// The per-block summing settle creates a REPLICATED <block> DB (local_only +
// perBlockReplicated: all settled blocks present on every node, each its own
// GUID), NOT a single coarse <inout> replica. This is the race-free substrate:
// each settled block is a distinct DB, so the exclusive-write frontier
// degenerates to one uncontended writer per block.
// CHECK: %[[SGUID:.*]], %[[SPTR:.*]] = arts.db_alloc[<inout>, <heap>, <write>, <block>] {{.*}}{local_only, perBlockReplicated

// Outer per-node loop then per-block loop: every node settles all blocks.
// CHECK: arts.runtime_query <total_nodes>
// CHECK: scf.for {{.*}} step %c1
// CHECK: scf.for {{.*}} step %c4
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
