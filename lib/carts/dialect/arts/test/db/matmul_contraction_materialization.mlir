// RUN: %carts-compile %s --pass-pipeline='builtin.module(matmul-contraction-materialization)' \
// RUN:   | %FileCheck %s --implicit-check-not=partialReductionDims

// The chained-matmul G consumer reads its
// contraction operand F from the coarse `replicatedRead` copy (guid_f). A sibling
// `perBlockReplicated` all-gather replica (guid_rep) holds F cross-node-complete,
// block by block. This pass splits G's k-loop into per-(G-block, k-tile)
// partial-product producer EDTs that read the replica BLOCK-NATIVE (k-tile t =
// replica block t), each writing a distinct per-tile partial DB, then a per-block
// summing settle reduces the partials into G's settled block with arith.addf.
//
// Every DB an EDT touches arrives as an OUTSIDE-the-EDT block-arg acquire (no EDT
// body GEPs an outer alloc): the ABI legality the in-body-acquire shortcut could
// not meet.

module attributes {arts.runtime_total_nodes = 2 : i64,
                   arts.runtime_total_workers = 4 : i64} {
  func.func @matmul_contraction() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c2 = arith.constant 2 : index
    %c4 = arith.constant 4 : index
    %cst = arith.constant 0.000000e+00 : f64
    %route = arith.constant -1 : i32

    // G result block DB (owner blocks of 2 rows x 4 cols).
    %g_guid, %g_ptr = arts.db_alloc[<inout>, <heap>, <write>, <block>]
      route(%route : i32) sizes[%c2] elementType(f64) elementSizes[%c2, %c4]
      {planLogicalWorkerSlice = [2, 4], planOwnerDims = [0], planPhysicalBlockShape = [2, 4]}
      : (memref<?xi64>, memref<?xmemref<?x?xf64>>)
    // E (LHS) block DB.
    %e_guid, %e_ptr = arts.db_alloc[<in>, <heap>, <read>, <block>]
      route(%route : i32) sizes[%c2] elementType(f64) elementSizes[%c2, %c4]
      {planLogicalWorkerSlice = [2, 4], planOwnerDims = [0], planPhysicalBlockShape = [2, 4]}
      : (memref<?xi64>, memref<?xmemref<?x?xf64>>)
    // F coarse copy, read via replicatedRead by G.
    %f_guid, %f_ptr = arts.db_alloc[<inout>, <heap>, <write>, <coarse>]
      route(%route : i32) sizes[%c1] elementType(f64) elementSizes[%c4, %c4]
      : (memref<?xi64>, memref<?xmemref<?x?xf64>>)
    // F per-block all-gather replica (2 blocks of 2 rows x 4 cols == whole F).
    %rep_guid, %rep_ptr = arts.db_alloc[<inout>, <heap>, <write>, <block>]
      route(%route : i32) sizes[%c2] elementType(f64) elementSizes[%c2, %c4]
      {local_only, perBlockReplicated, planOwnerDims = [0], planPhysicalBlockShape = [2, 4]}
      : (memref<?xi64>, memref<?xmemref<?x?xf64>>)

    scf.for %owner = %c0 to %c2 step %c1 {
      %g_acq_guid, %g_acq = arts.db_acquire[<inout>]
        (%g_guid : memref<?xi64>, %g_ptr : memref<?xmemref<?x?xf64>>)
        partitioning(<block>), indices[], offsets[%owner], sizes[%c1]
        -> (memref<?xi64>, memref<?xmemref<?x?xf64>>)
      %e_acq_guid, %e_acq = arts.db_acquire[<in>]
        (%e_guid : memref<?xi64>, %e_ptr : memref<?xmemref<?x?xf64>>)
        partitioning(<block>), indices[], offsets[%owner], sizes[%c1]
        -> (memref<?xi64>, memref<?xmemref<?x?xf64>>)
      %f_acq_guid, %f_acq = arts.db_acquire[<in>]
        (%f_guid : memref<?xi64>, %f_ptr : memref<?xmemref<?x?xf64>>)
        partitioning(<coarse>), indices[], offsets[%c0], sizes[%c1] {replicatedRead}
        -> (memref<?xi64>, memref<?xmemref<?x?xf64>>)
      arts.edt <task> <internode> route(%route) (%g_acq, %e_acq, %f_acq)
          : memref<?xmemref<?x?xf64>>, memref<?xmemref<?x?xf64>>, memref<?xmemref<?x?xf64>>
          params(%c2, %owner : index, index)
          attributes {partialReductionDims = [2], partialReductionOwnerDims = [0, 1],
                      planLogicalWorkerSlice = [2, 4], planOwnerDims = [0],
                      planPhysicalBlockShape = [2, 4]} {
      ^bb0(%g_arg: memref<?xmemref<?x?xf64>>, %e_arg: memref<?xmemref<?x?xf64>>,
           %f_arg: memref<?xmemref<?x?xf64>>, %rows: index, %own: index):
        %g_blk = arts.db_ref %g_arg[%c0] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
        %e_blk = arts.db_ref %e_arg[%c0] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
        %f_blk = arts.db_ref %f_arg[%c0] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
        scf.for %r = %c0 to %rows step %c1 {
          scf.for %col = %c0 to %c4 step %c1 {
            memref.store %cst, %g_blk[%r, %col] : memref<?x?xf64>
          }
          // Contraction k-loop: reads F[k, col].
          scf.for %k = %c0 to %c4 step %c1 {
            scf.for %col = %c0 to %c4 step %c1 {
              %ev = memref.load %e_blk[%r, %k] : memref<?x?xf64>
              %fv = memref.load %f_blk[%k, %col] : memref<?x?xf64>
              %m = arith.mulf %ev, %fv : f64
              %acc = memref.load %g_blk[%r, %col] : memref<?x?xf64>
              %s = arith.addf %acc, %m : f64
              memref.store %s, %g_blk[%r, %col] : memref<?x?xf64>
            }
          }
        }
      }
    }
    return
  }
}

// The original partial-reduction G EDT is retired (no partialReductionDims left,
// enforced by --implicit-check-not above).

// CHECK-LABEL: func.func @matmul_contraction

// Per-(G-block, k-tile) partials DB: a replicated block DB, one block per tile.
// CHECK: arts.db_alloc[<inout>, <heap>, <write>, <block>] {{.*}}{local_only, perBlockReplicated

// Tile-0 producer: F replica block 0 (<in>), E block (<in>), partial tile 0
// (<out>), all acquired OUTSIDE the EDT and delivered as block-args.
// CHECK: arts.db_acquire[<in>] (%[[REP:[a-z0-9_]+]] : {{.*}}) partitioning(<block>), indices[], offsets[%[[T0OFF:[a-z0-9_]+]]]
// CHECK: arts.db_acquire[<in>] {{.*}} partitioning(<block>)
// CHECK: arts.db_acquire[<out>] {{.*}} partitioning(<block>), indices[], offsets[%[[T0OFF]]]
// CHECK: arts.edt <task>
// CHECK: arith.mulf
// CHECK: arith.addf

// Tile-1 producer reads replica block 1 and writes partial tile 1.
// CHECK: arts.db_acquire[<in>] (%[[REP]] : {{.*}}) partitioning(<block>), indices[], offsets[%[[T1OFF:[a-z0-9_]+]]]
// CHECK: arts.db_acquire[<in>] {{.*}} partitioning(<block>)
// CHECK: arts.db_acquire[<out>] {{.*}} partitioning(<block>), indices[], offsets[%[[T1OFF]]]
// CHECK: arts.edt <task>

// Per-block summing settle: reduce the P partials into G's settled block with
// arith.addf, written once.
// CHECK: arts.edt <task>
// CHECK-SAME: perBlockSummingSettle
// CHECK: arts.db_ref
// CHECK: arith.addf
// CHECK: memref.store
