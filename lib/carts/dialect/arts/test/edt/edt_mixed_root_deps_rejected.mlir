// RUN: not %carts-compile %s --pipeline post-db-refinement --start-from post-db-refinement --arts-config %inputs_dir/arts_1t.cfg 2>&1 \
// RUN:   | %FileCheck %s

// An EDT must not carry both a coarse and a subpartitioned (block) acquire of
// the same root DB without planned-block evidence. ARTS realizes committed
// grain; it fails closed so the single grain is committed upstream rather than
// silently collapsing the mixed shape to one coarse dependency.

// CHECK: error: 'arts.edt' op carries both a coarse and a subpartitioned acquire of the same root datablock without planned-block evidence

module {
  func.func @reject_mixed_root_dependencies() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index
    %route = arith.constant -1 : i32

    %guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <coarse>] route(%route : i32) sizes[%c1] elementType(f64) elementSizes[%c4] : (memref<?xi64>, memref<?xmemref<?xf64>>)
    %block_guid, %block_ptr = arts.db_acquire[<inout>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<?xf64>>) partitioning(<block>), indices[], offsets[%c1], sizes[%c1] -> (memref<?xi64>, memref<?xmemref<?xf64>>)
    %coarse_guid, %coarse_ptr = arts.db_acquire[<in>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<?xf64>>) partitioning(<coarse>), indices[], offsets[%c0], sizes[%c1] -> (memref<?xi64>, memref<?xmemref<?xf64>>)

    arts.edt <task> <intranode> route(%route) (%block_ptr, %coarse_ptr) : memref<?xmemref<?xf64>>, memref<?xmemref<?xf64>> {
    ^bb0(%block_dep: memref<?xmemref<?xf64>>, %coarse_dep: memref<?xmemref<?xf64>>):
      %block_view = arts.db_ref %block_dep[%c0] : memref<?xmemref<?xf64>> -> memref<?xf64>
      %coarse_view = arts.db_ref %coarse_dep[%c0] : memref<?xmemref<?xf64>> -> memref<?xf64>
      %value = memref.load %coarse_view[%c0] : memref<?xf64>
      memref.store %value, %block_view[%c0] : memref<?xf64>
      arts.db_release(%block_dep) : memref<?xmemref<?xf64>>
      arts.db_release(%coarse_dep) : memref<?xmemref<?xf64>>
      arts.yield
    }

    arts.db_release(%block_ptr) : memref<?xmemref<?xf64>>
    arts.db_release(%coarse_ptr) : memref<?xmemref<?xf64>>
    return
  }
}
