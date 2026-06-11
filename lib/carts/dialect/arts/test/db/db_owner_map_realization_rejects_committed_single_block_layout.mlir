// RUN: not %carts-compile %s --pipeline post-db-refinement --start-from post-db-refinement --arts-config %inputs_dir/arts_multinode.cfg 2>&1 | %FileCheck %s

// A committed SDE block layout cannot be demoted to a local/single-block reject
// reason. If owner-map realization cannot distribute it, ARTS must fail closed.

// CHECK: carries a committed SDE block layout but is not eligible for distributed owner-map realization (single_block)

module attributes {arts.runtime_total_nodes = 2 : i64, arts.runtime_total_workers = 8 : i64} {
  func.func @single_block_committed_layout_rejected() {
    %c1 = arith.constant 1 : index
    %c16 = arith.constant 16 : index
    %route = arith.constant -1 : i32

    %guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <block>] route(%route : i32) sizes[%c1] elementType(f64) elementSizes[%c16] {planOwnerDims = [0], planPhysicalBlockShape = [16]} : (memref<?xi64>, memref<?xmemref<?xf64>>)
    return
  }
}
