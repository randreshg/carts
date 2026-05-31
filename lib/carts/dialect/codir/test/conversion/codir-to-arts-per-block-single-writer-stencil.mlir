// RUN: %carts-compile %s --pipeline post-db-refinement --arts-config %inputs_dir/arts_multinode_8x64.cfg --distributed-db | %FileCheck %s

// WF-6 keystone (ADR-0003 §2c): the per-block single-writer stencil substrate,
// the nearest-neighbor halo dual of the per-block all-gather. An iterative
// double-buffered stencil (jacobi2d) reads each owner block plus a one-cell halo
// of its neighbor blocks and WRITES the same block-distributed buffer the next
// half-step (the WAR the all-gather/summing-settle directions do not have). The
// legacy path resolves the cross-node halo by DECLINING distribution: the buffer
// becomes a single `local_only` whole-array replica per node, so on 2n each node
// computes only its strip on a private replica and the neighbor strip on the
// OTHER node is never observed (the 2n WRONG answer).
//
// When CODIR presents a `halo` entry in `dep_collectives` (auto-selected by
// StoragePlanning for full-timestep stencil producers, or explicitly provided
// in this direct conversion fixture), the compiler keeps the buffer DISTRIBUTED
// (block-scattered, one writer per block-GUID) and emits a per-block neighbor
// halo exchange: one EDT per owner block, RO-acquiring its top/bottom neighbor
// single-writer blocks (ordered after each neighbor's writer frontier) and
// writing this block's halo region ONCE. Distinct DB per block => single
// uncontended writer => race-free nearest-neighbor exchange, never a whole-array
// replica.

module attributes {arts.runtime_total_nodes = 8 : i64, arts.runtime_total_workers = 512 : i64} {
  func.func @per_block_single_writer_stencil() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c8 = arith.constant 8 : index
    %c24 = arith.constant 24 : index
    %A = memref.alloc() : memref<24x4xf32>

    // Iterative stencil half-step: reads A with a one-cell halo on the owner
    // (row) dim and writes A back, block-distributed over its row dim. Opts into
    // the block-native single-writer stencil substrate.
    scf.for %i = %c0 to %c24 step %c8 {
      codir.codelet deps(%A : memref<24x4xf32>) params(%i : index)
          attributes {dep_modes = [#codir.access_mode<readwrite>],
                      dep_storage_views = [#codir.storage_view<phase_redistributed>],
                      dep_collectives = [#codir.collective<halo>],
                      distribution_kind = #codir.distribution_kind<blocked>,
                      emit_block_native_stencil,
                      halo_shape = [1, 0],
                      iteration_topology = #codir.iteration_topology<owner_strip>,
                      logical_worker_slice = [8, 4],
                      pattern = #codir.pattern<stencil_tiling_nd>,
                      tile_owner_dims = [0],
                      tile_shape = [8, 4]} {
      ^bb0(%arg0: memref<24x4xf32>, %base: index):
        %inner_c0 = arith.constant 0 : index
        %inner_c1 = arith.constant 1 : index
        %inner_c4 = arith.constant 4 : index
        %inner_c8 = arith.constant 8 : index
        %inner_c23 = arith.constant 23 : index
        %cst = arith.constant 2.000000e-01 : f32
        %end_raw = arith.addi %base, %inner_c8 : index
        %end = arith.minui %end_raw, %inner_c23 : index
        scf.for %row = %base to %end step %inner_c1 {
          scf.for %col = %inner_c1 to %inner_c4 step %inner_c1 {
            %up = arith.subi %row, %inner_c1 : index
            %dn = arith.addi %row, %inner_c1 : index
            %a0 = memref.load %arg0[%row, %col] : memref<24x4xf32>
            %a1 = memref.load %arg0[%up, %col] : memref<24x4xf32>
            %a2 = memref.load %arg0[%dn, %col] : memref<24x4xf32>
            %s0 = arith.addf %a0, %a1 : f32
            %s1 = arith.addf %s0, %a2 : f32
            %r = arith.mulf %s1, %cst : f32
            memref.store %r, %arg0[%row, %col] : memref<24x4xf32>
          }
        }
        codir.yield
      }
    }

    // Host observes A (the verification checksum reads it), making A a
    // host-visible array, exactly as in the benchmark.
    %resultA = memref.load %A[%c0, %c0] : memref<24x4xf32>
    func.call @use(%resultA) : (f32) -> ()
    memref.dealloc %A : memref<24x4xf32>
    return
  }

  func.func private @use(f32)
}

// CHECK-LABEL: func.func @per_block_single_writer_stencil

// The stencil buffer stays a DISTRIBUTED <block> single-writer DB: the
// per-block single-writer stencil substrate clears local_only and stamps
// perBlockSingleWriterStencil, so the distributed-ownership pass block-scatters
// it (one writer per block-GUID) instead of replicating a whole-array copy.
// CHECK: arts.db_alloc[<inout>, <heap>, <write>, <block>]
// CHECK-SAME: perBlockSingleWriterStencil

// Per-block neighbor halo exchange: one block loop, where for each owner block
// the top/bottom neighbor blocks are RO-acquired (<in>) on distinct block GUIDs
// and this block is acquired output-only on its OWN distinct block DB by exactly
// one EDT (single writer, no shared exclusive-write frontier).
// CHECK: scf.for
// CHECK: arts.db_acquire[<{{inout|out}}>] {{.*}} partitioning(<block>)
// CHECK: arts.db_acquire[<in>] {{.*}} partitioning(<block>)
// CHECK: arts.db_acquire[<in>] {{.*}} partitioning(<block>)
// CHECK: arts.edt <task>
// CHECK-SAME: perBlockHaloExchange
// CHECK-SAME: storageBridgeCopy
// CHECK: arts.barrier

// func.func private @use
