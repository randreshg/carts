// RUN: %carts-compile %s --pass-pipeline='builtin.module(distributed-launch-consistency)' \
// RUN:   | %FileCheck %s

module attributes {arts.runtime_total_nodes = 4 : i64, arts.runtime_total_workers = 256 : i64} {
  func.func @routes_writer_to_owner_dim_contiguous_db() {
    %route = arith.constant 0 : i32
    %c1 = arith.constant 1 : index
    %c6 = arith.constant 6 : index
    %c8 = arith.constant 8 : index
    %guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <block>]
      route(%route : i32) sizes[%c8] elementType(f64) elementSizes[%c1]
      {distributed, owner_block_shape = [1], owner_map_dims = [0],
       owner_map_kind = #arts.owner_map_kind<owner_dim_contiguous>,
       owner_map_version = 1 : i32, planOwnerDims = [0],
       planPhysicalBlockShape = [1]}
      : (memref<?xi64>, memref<?xmemref<?xf64>>)
    %acq_guid, %acq_ptr = arts.db_acquire[<inout>]
      (%guid : memref<?xi64>, %ptr : memref<?xmemref<?xf64>>)
      partitioning(<block>), indices[], offsets[%c6], sizes[%c1]
      -> (memref<?xi64>, memref<?xmemref<?xf64>>)
    arts.edt <task> <internode> route(%route) (%acq_ptr)
        : memref<?xmemref<?xf64>> {
    ^bb0(%dep: memref<?xmemref<?xf64>>):
      arts.yield
    }
    return
  }

  func.func @routes_same_owner_multi_writer_to_canonical_owner() {
    %route = arith.constant 0 : i32
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c6 = arith.constant 6 : index
    %c8 = arith.constant 8 : index
    %value = arith.constant 1.0 : f64
    %guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <block>]
      route(%route : i32) sizes[%c8] elementType(f64) elementSizes[%c1]
      {distributed, owner_block_shape = [1], owner_map_dims = [0],
       owner_map_kind = #arts.owner_map_kind<owner_dim_contiguous>,
       owner_map_version = 1 : i32, planOwnerDims = [0],
       planPhysicalBlockShape = [1]}
      : (memref<?xi64>, memref<?xmemref<?xf64>>)
    %acq_guid0, %acq_ptr0 = arts.db_acquire[<inout>]
      (%guid : memref<?xi64>, %ptr : memref<?xmemref<?xf64>>)
      partitioning(<block>), indices[], offsets[%c6], sizes[%c1]
      -> (memref<?xi64>, memref<?xmemref<?xf64>>)
    %acq_guid1, %acq_ptr1 = arts.db_acquire[<inout>]
      (%guid : memref<?xi64>, %ptr : memref<?xmemref<?xf64>>)
      partitioning(<block>), indices[], offsets[%c6], sizes[%c1]
      -> (memref<?xi64>, memref<?xmemref<?xf64>>)
    arts.edt <task> <internode> route(%route) (%acq_ptr0, %acq_ptr1)
        : memref<?xmemref<?xf64>>, memref<?xmemref<?xf64>> {
    ^bb0(%dep0: memref<?xmemref<?xf64>>, %dep1: memref<?xmemref<?xf64>>):
      %payload0 = arts.db_ref %dep0[%c0] : memref<?xmemref<?xf64>> -> memref<?xf64>
      %payload1 = arts.db_ref %dep1[%c0] : memref<?xmemref<?xf64>> -> memref<?xf64>
      memref.store %value, %payload0[%c0] : memref<?xf64>
      memref.store %value, %payload1[%c0] : memref<?xf64>
      arts.yield
    }
    return
  }

  func.func @routes_partition_offset_writer_to_owner() {
    %route = arith.constant 0 : i32
    %c1 = arith.constant 1 : index
    %c6 = arith.constant 6 : index
    %c8 = arith.constant 8 : index
    %guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <block>]
      route(%route : i32) sizes[%c8] elementType(f64) elementSizes[%c1]
      {distributed, owner_block_shape = [1], owner_map_dims = [0],
       owner_map_kind = #arts.owner_map_kind<owner_dim_contiguous>,
       owner_map_version = 1 : i32, planOwnerDims = [0],
       planPhysicalBlockShape = [1]}
      : (memref<?xi64>, memref<?xmemref<?xf64>>)
    %acq_guid, %acq_ptr = arts.db_acquire[<out>]
      (%guid : memref<?xi64>, %ptr : memref<?xmemref<?xf64>>)
      partitioning(<block>, indices[], offsets[%c6], sizes[%c1]), indices[]
      -> (memref<?xi64>, memref<?xmemref<?xf64>>)
    arts.edt <task> <internode> route(%route) (%acq_ptr)
        : memref<?xmemref<?xf64>> {
    ^bb0(%dep: memref<?xmemref<?xf64>>):
      arts.yield
    }
    return
  }
}

// CHECK-LABEL: func.func @routes_writer_to_owner_dim_contiguous_db
// CHECK: %[[NODES:.*]] = arts.runtime_query <total_nodes> -> i32
// CHECK: %[[NODES_IDX:.*]] = arith.index_cast %[[NODES]] : i32 to index
// CHECK: %[[SCALED:.*]] = arith.muli %{{.*}}, %[[NODES_IDX]] : index
// CHECK: %[[ROUTE_IDX:.*]] = arith.divui %[[SCALED]], %{{.*}} : index
// CHECK: %[[ROUTE:.*]] = arith.index_cast %[[ROUTE_IDX]] : index to i32
// CHECK: arts.edt <task> <internode> route(%[[ROUTE]])

// CHECK-LABEL: func.func @routes_same_owner_multi_writer_to_canonical_owner
// CHECK: %[[MULTI_ROUTE:.*]] = arith.index_cast %{{.*}} : index to i32
// CHECK: arts.edt <task> <internode> route(%[[MULTI_ROUTE]]) (%{{.*}}, %{{.*}})

// CHECK-LABEL: func.func @routes_partition_offset_writer_to_owner
// CHECK: %[[BLOCK_COORD:.*]] = arith.divui %{{.*}}, %{{.*}} : index
// CHECK: %[[SCALED_PARTITION:.*]] = arith.muli %[[BLOCK_COORD]], %{{.*}} : index
// CHECK: %[[PARTITION_ROUTE_IDX:.*]] = arith.divui %[[SCALED_PARTITION]], %{{.*}} : index
// CHECK: %[[PARTITION_ROUTE:.*]] = arith.index_cast %[[PARTITION_ROUTE_IDX]] : index to i32
// CHECK: arts.edt <task> <internode> route(%[[PARTITION_ROUTE]])
