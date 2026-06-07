// RUN: %carts-compile %s --pipeline post-db-refinement --start-from post-db-refinement --arts-config %inputs_dir/arts_1t.cfg | %FileCheck %s

// A planned/preserved block dependency is upstream layout evidence. ARTS may
// not collapse it to a coarse edge just because another dependency reaches the
// same root DB.

// CHECK-LABEL: func.func @preserve_planned_mixed_root_dependencies
// CHECK: arts.db_acquire[<out>] {{.*}} partitioning(<block>)
// CHECK-SAME: preserve_dep_edge = #arts.preserve_dep_edge
// CHECK: arts.db_acquire[<in>] {{.*}} partitioning(<coarse>)
// CHECK: arts.edt <task> <intranode> route({{.*}}) ({{.*}}, {{.*}}) : memref<?xmemref<?xf64>>, memref<?xmemref<?xf64>>

module {
  func.func @preserve_planned_mixed_root_dependencies() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index
    %route = arith.constant -1 : i32

    %guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <coarse>] route(%route : i32) sizes[%c1] elementType(f64) elementSizes[%c4] {distributed, db_memory_placement = #arts.db_memory_placement<owner_scattered>, owner_block_shape = [1], owner_map_dims = [0], owner_map_kind = #arts.owner_map_kind<owner_dim_contiguous>, owner_map_version = 1 : i32, planOwnerDims = [0], planPhysicalBlockShape = [1]} : (memref<?xi64>, memref<?xmemref<?xf64>>)
    %block_guid, %block_ptr = arts.db_acquire[<inout>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<?xf64>>) partitioning(<block>), indices[], offsets[%c1], sizes[%c1] {preserve_dep_edge = #arts.preserve_dep_edge} -> (memref<?xi64>, memref<?xmemref<?xf64>>)
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
