// RUN: %carts-compile %s --arts-config %inputs_dir/arts_multinode.cfg \
// RUN:   --pass-pipeline='builtin.module(reduction-planning,storage-planning,convert-codir-to-arts,verify-arts-objects-only)' \
// RUN:   | %FileCheck %s

// Cross-owner transpose-matvec reduce (atax y = A^T(Ax) / bicg s = A^T r).
//
// A producer codelet writes the intermediate tmp block-distributed by row.
// The transpose-reduce codelet owns its output by column and contracts over the
// matrix's ROW dim: the matrix access `A[reductionIV, owner]` makes the dep map
// `[-1, ownerDim]` (the reduction marker in the LEADING position). This is the
// CROSS-OWNER signature, distinct from the owner-local pipeline reduction which
// maps the matrix `[ownerDim, -1]` (covered by the unchanged metadata test).
//
// At 2n the default lowering gathers tmp to a coarse buffer with an INTRANODE
// copy-out, leaving each node holding only its own strip; the cross-owner
// reduction then reads incomplete values (the 2n value collapse). The fix gates
// the copy-out onto a CROSS-NODE gather: an outer per-node loop drives the
// per-block copy so every node assembles the complete coarse buffer, pulling
// the blocks it did not produce through the cross-node read-only block acquire.

module attributes {arts.runtime_total_nodes = 2 : i64, arts.runtime_total_workers = 16 : i64} {
  func.func @cross_owner_transpose_reduce(%A: memref<128x128xf64>, %x: memref<128xf64>, %tmp: memref<128xf64>, %y: memref<128xf64>, %base: index) {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c16 = arith.constant 16 : index
    %c128 = arith.constant 128 : index

    // Producer: tmp = A*x, owner dim 0 (row), tmp written block-distributed.
    scf.for %i = %base to %c128 step %c16 {
      codir.codelet deps(%tmp, %A, %x : memref<128xf64>, memref<128x128xf64>, memref<128xf64>)
          params(%i : index)
          attributes {dep_modes = [#codir.access_mode<write>, #codir.access_mode<read>, #codir.access_mode<read>],
                      distribution_kind = #codir.distribution_kind<blocked>,
                      iteration_topology = #codir.iteration_topology<owner_strip>,
                      logical_worker_slice = [16],
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

    // Transpose reduce: y = A^T*tmp. Owner dim 0 of result (column). The matrix
    // read A[reductionIV, owner] reduces over A's leading (row) dim -> dep map
    // `[-1, 0]`, the cross-owner transpose signature. tmp is read whole.
    scf.for %jb = %base to %c128 step %c16 {
      codir.codelet deps(%y, %A, %tmp : memref<128xf64>, memref<128x128xf64>, memref<128xf64>)
          params(%jb : index)
          attributes {dep_modes = [#codir.access_mode<readwrite>, #codir.access_mode<read>, #codir.access_mode<read>],
                      dep_storage_views = [#codir.storage_view<compute_block>, #codir.storage_view<compute_block>, #codir.storage_view<host_whole>],
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

// The transpose-reduce EDT carries the cross-owner dep map `[-1, 0]` for the
// matrix (reduction marker in the leading position).
// CHECK-LABEL: func.func @cross_owner_transpose_reduce
// CHECK: partialReductionDepResultDimMaps = {{\[\[}}0], [-1, 0], [-1]]

// The gather copy-out for the intermediate is an OUTER per-node loop (driven by
// a total_nodes runtime query) wrapping the per-block copy, so every node
// assembles the complete coarse buffer. A plain intranode copy-out has no such
// per-node loop.
// CHECK: arts.runtime_query <total_nodes>
// CHECK: scf.for
// CHECK: scf.for
// CHECK: arts.db_acquire[<inout>] {{.*}} partitioning(<coarse>)
// CHECK: arts.db_acquire[<in>] {{.*}} partitioning(<block>)
// CHECK: arts.edt <task> <internode>
// CHECK-SAME: storageBridgeCopy
// CHECK: arts.barrier
