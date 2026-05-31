// RUN: %carts-compile %s --pass-pipeline='builtin.module(matmul-3mm-contraction-materialization)' \
// RUN:   | %FileCheck %s --implicit-check-not=perBlockSummingSettle

// The 3mm contraction materializer must not split one G owner block into one
// producer per fine-grain F replica block when the replica block count is larger
// than the node count. Standard 3mm on 2 nodes has this shape (many F blocks, two
// nodes); materializing all fine-grain tiles creates thousands of tiny EDTs and
// loses to the coarser replicated-read path.

module attributes {arts.runtime_total_nodes = 2 : i64,
                   arts.runtime_total_workers = 4 : i64} {
  func.func @matmul_3mm_contraction_cost_gate() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c2 = arith.constant 2 : index
    %c4 = arith.constant 4 : index
    %c8 = arith.constant 8 : index
    %cst = arith.constant 0.000000e+00 : f64
    %route = arith.constant -1 : i32

    %g_guid, %g_ptr = arts.db_alloc[<inout>, <heap>, <write>, <block>]
      route(%route : i32) sizes[%c2] elementType(f64) elementSizes[%c2, %c4]
      {planLogicalWorkerSlice = [2, 4], planOwnerDims = [0], planPhysicalBlockShape = [2, 4]}
      : (memref<?xi64>, memref<?xmemref<?x?xf64>>)
    %e_guid, %e_ptr = arts.db_alloc[<in>, <heap>, <read>, <block>]
      route(%route : i32) sizes[%c2] elementType(f64) elementSizes[%c2, %c8]
      {planLogicalWorkerSlice = [2, 8], planOwnerDims = [0], planPhysicalBlockShape = [2, 8]}
      : (memref<?xi64>, memref<?xmemref<?x?xf64>>)
    %f_guid, %f_ptr = arts.db_alloc[<inout>, <heap>, <write>, <coarse>]
      route(%route : i32) sizes[%c1] elementType(f64) elementSizes[%c8, %c4]
      : (memref<?xi64>, memref<?xmemref<?x?xf64>>)
    %rep_guid, %rep_ptr = arts.db_alloc[<inout>, <heap>, <write>, <block>]
      route(%route : i32) sizes[%c4] elementType(f64) elementSizes[%c2, %c4]
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
          scf.for %k = %c0 to %c8 step %c1 {
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

// CHECK-LABEL: func.func @matmul_3mm_contraction_cost_gate
// CHECK: arts.edt <task>
// CHECK-SAME: partialReductionDims = [2]
