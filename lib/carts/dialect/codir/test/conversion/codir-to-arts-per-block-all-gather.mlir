// RUN: %carts-compile %s --pipeline post-db-refinement --arts-config %inputs_dir/arts_multinode_8x64.cfg --distributed-db | %FileCheck %s

// Per-block single-writer all-gather substrate. This mirrors 3mm's shape:
// a producer writes F as a
// phase_redistributed (compute_block bridge) intermediate, and a sibling
// matmul codelet reads the SAME buffer on the contraction dim as a
// replicated_read operand (3mm's G = E*F). That read is the all-gather signal.
//
// The compiler must emit, on top of the existing coarse owner-local write-back,
// a per-block all-gather: an outer per-node loop driving one block loop, where
// each gathered block is its OWN <block> DB written output-only (<out>) while
// the producer's block is acquired read-only (<in>). Distinct DB per block ⇒
// single uncontended writer ⇒ no shared exclusive-write frontier ⇒ the writes
// are race-free AND concurrent (the phase-2 coarse-replica serialization is
// removed at the root, not relaxed).

module attributes {arts.runtime_total_nodes = 8 : i64, arts.runtime_total_workers = 512 : i64} {
  func.func @per_block_all_gather_for_replicated_read_consumer() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index
    %c8 = arith.constant 8 : index
    %c24 = arith.constant 24 : index
    %F = memref.alloc() : memref<24x4xf32>

    // Producer: F = C*D, written block-distributed over its row dim.
    scf.for %i = %c0 to %c24 step %c8 {
      codir.codelet deps(%F : memref<24x4xf32>) params(%i : index)
          attributes {dep_modes = [#codir.access_mode<write>],
                      dep_storage_views = [#codir.storage_view<phase_redistributed>],
                      distribution_kind = #codir.distribution_kind<blocked>,
                      iteration_topology = #codir.iteration_topology<owner_strip>,
                      logical_worker_slice = [8, 4],
                      pattern = #codir.pattern<matmul>,
                      tile_owner_dims = [0],
                      tile_shape = [8, 4]} {
      ^bb0(%arg0: memref<24x4xf32>, %base: index):
        %inner_c0 = arith.constant 0 : index
        %inner_c1 = arith.constant 1 : index
        %inner_c4 = arith.constant 4 : index
        %inner_c8 = arith.constant 8 : index
        %inner_c24 = arith.constant 24 : index
        %value = arith.constant 3.000000e+00 : f32
        %end_raw = arith.addi %base, %inner_c8 : index
        %end = arith.minui %end_raw, %inner_c24 : index
        scf.for %row = %base to %end step %inner_c1 {
          scf.for %col = %inner_c0 to %inner_c4 step %inner_c1 {
            memref.store %value, %arg0[%row, %col] : memref<24x4xf32>
          }
        }
        codir.yield
      }
    }

    // Consumer: G = E*F reads F as replicated_read on the contraction dim
    // (whole F, all rows). This is the sibling reader that triggers the gate.
    %G = memref.alloc() : memref<24x4xf32>
    scf.for %i = %c0 to %c24 step %c8 {
      codir.codelet deps(%G, %F : memref<24x4xf32>, memref<24x4xf32>) params(%i : index)
          attributes {dep_modes = [#codir.access_mode<write>, #codir.access_mode<read>],
                      dep_storage_views = [#codir.storage_view<compute_block>, #codir.storage_view<replicated_read>],
                      distribution_kind = #codir.distribution_kind<blocked>,
                      iteration_topology = #codir.iteration_topology<owner_strip>,
                      logical_worker_slice = [8, 4],
                      pattern = #codir.pattern<matmul>,
                      tile_owner_dims = [0],
                      tile_shape = [8, 4]} {
      ^bb0(%argG: memref<24x4xf32>, %argF: memref<24x4xf32>, %base: index):
        %inner_c0 = arith.constant 0 : index
        %inner_c1 = arith.constant 1 : index
        %inner_c4 = arith.constant 4 : index
        %inner_c8 = arith.constant 8 : index
        %inner_c24 = arith.constant 24 : index
        %end_raw = arith.addi %base, %inner_c8 : index
        %end = arith.minui %end_raw, %inner_c24 : index
        scf.for %row = %base to %end step %inner_c1 {
          scf.for %col = %inner_c0 to %inner_c4 step %inner_c1 {
            %acc0 = arith.constant 0.000000e+00 : f32
            // Contraction over F's row dim (all rows): replicated read.
            %acc = scf.for %k = %inner_c0 to %inner_c24 step %inner_c1
                iter_args(%a = %acc0) -> (f32) {
              %fv = memref.load %argF[%k, %col] : memref<24x4xf32>
              %na = arith.addf %a, %fv : f32
              scf.yield %na : f32
            }
            memref.store %acc, %argG[%row, %col] : memref<24x4xf32>
          }
        }
        codir.yield
      }
    }

    // Host observes both arrays (3mm checksums F and G); this makes F a
    // host-visible array, exactly as in the benchmark.
    %resultF = memref.load %F[%c0, %c0] : memref<24x4xf32>
    func.call @use(%resultF) : (f32) -> ()
    %result = memref.load %G[%c0, %c0] : memref<24x4xf32>
    func.call @use(%result) : (f32) -> ()
    memref.dealloc %F : memref<24x4xf32>
    memref.dealloc %G : memref<24x4xf32>
    return
  }

  func.func private @use(f32)
}

// The producer's F is a distributed <block> bridge DB: F is already a set of N
// per-block single-writer DBs (one GUID per block, written once by its
// owner-routed compute EDT) — the substrate the all-gather pulls from.
// CHECK-LABEL: func.func @per_block_all_gather_for_replicated_read_consumer
// CHECK: arts.db_alloc[<inout>, <heap>, <write>, <block>]
// CHECK-SAME: distributed
// CHECK-SAME: storage_bridge = #arts.storage_bridge<host_whole_to_compute_block>

// The per-block all-gather creates a REPLICATED <block> DB (local_only +
// perBlockReplicated: all N blocks present on every node, each its own GUID),
// NOT a single coarse <inout> replica. This is the race-free substrate: each
// gathered block is a distinct DB, so the exclusive-write frontier degenerates
// to one uncontended writer per block.
// CHECK: %[[REPGUID:.*]], %[[REPPTR:.*]] = arts.db_alloc[<inout>, <heap>, <write>, <block>]
// CHECK-SAME: local_only
// CHECK-SAME: perBlockReplicated

// Outer per-node loop: every node assembles all blocks. The block loop may
// fold to one grouped iteration when the static block count is small; that is
// still a per-block DB substrate because every lane acquires a distinct block.
// CHECK: scf.for
// The producer block is RO-acquired (<in>); the gathered block is acquired on
// its OWN distinct block DB by exactly one EDT — single writer, no shared
// exclusive-write frontier (contrast the coarse <inout> replica below).
// CHECK: arts.db_acquire[<in>] {{.*}} partitioning(<block>)
// CHECK: arts.db_acquire[<{{inout|out}}>] (%[[REPGUID]] : {{.*}}, %[[REPPTR]] : {{.*}}) partitioning(<block>)
// CHECK: arts.db_acquire[<in>] {{.*}} partitioning(<block>)
// CHECK: arts.db_acquire[<{{inout|out}}>] (%[[REPGUID]] : {{.*}}, %[[REPPTR]] : {{.*}}) partitioning(<block>)
// CHECK: arts.edt <task>
// CHECK-SAME: perBlockAllGather
// CHECK-SAME: storageBridgeCopy
// CHECK: arts.barrier

// The existing whole-array consumer (3mm's G) still reads F via the coarse
// replica (replicatedRead): rewiring it to read the per-block DBs needs
// contraction tiling of its k-loop. The substrate is proven; the
// consumer boundary is reported, not papered over by re-coarsening.
// CHECK: arts.db_acquire[<in>] {{.*}} partitioning(<coarse>){{.*}}replicatedRead
