// RUN: %carts-compile %s --pass-pipeline='builtin.module()' | %FileCheck %s

module {
  func.func @owner_map_kind_roundtrip() {
    %route = arith.constant 0 : i32
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index
    %guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <block>]
      route(%route : i32) sizes[%c4] elementType(f64) elementSizes[%c1]
      {distributed, owner_block_shape = [1], owner_map_dims = [0],
       owner_map_kind = #arts.owner_map_kind<owner_dim_contiguous>,
       owner_map_version = 1 : i32, planOwnerDims = [0],
       planPhysicalBlockShape = [1]}
      : (memref<?xi64>, memref<?xmemref<?xf64>>)
    return
  }
}

// CHECK: owner_map_kind = #arts.owner_map_kind<owner_dim_contiguous>
