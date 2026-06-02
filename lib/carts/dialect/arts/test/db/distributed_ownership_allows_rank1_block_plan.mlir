// RUN: %carts-compile %s --pipeline post-db-refinement --start-from post-db-refinement --arts-config %inputs_dir/arts_multinode.cfg --distributed-db | %FileCheck %s --implicit-check-not=local_only --implicit-check-not=distributed_reject_reason

// Rank-1 block-planned DBs are valid distributed storage for vector outputs and
// intermediates. They are common in ATAX/BiCG and normalization kernels, so the
// distributed ownership gate must not reject them as unsupported vectors once
// SDE/CODIR has authored an explicit owner/block plan.

// CHECK-LABEL: func.func @rank1_block_alloc_can_be_distributed
// CHECK: arts.db_alloc
// CHECK-SAME: <block>
// CHECK-SAME: distributed
// CHECK-SAME: owner_block_shape = [16]
// CHECK-SAME: owner_map_dims = [0]
// CHECK-SAME: owner_map_kind = #arts.owner_map_kind<owner_dim_contiguous>
// CHECK-SAME: owner_map_version = 1 : i32
// CHECK-SAME: planLogicalWorkerSlice = [16]
// CHECK-SAME: planOwnerDims = [0]
// CHECK-SAME: planPhysicalBlockShape = [16]
// CHECK: arts.edt <task> <internode>
// CHECK-LABEL: func.func @rank2_tile_alloc_can_be_distributed
// CHECK: arts.db_alloc
// CHECK-SAME: <block>
// CHECK-SAME: distributed
// CHECK-SAME: owner_block_shape = [8, 16]
// CHECK-SAME: owner_map_dims = [0, 1]
// CHECK-SAME: owner_map_kind = #arts.owner_map_kind<owner_dim_contiguous>
// CHECK-SAME: owner_map_version = 1 : i32
// CHECK-SAME: planLogicalWorkerSlice = [8, 16]
// CHECK-SAME: planOwnerDims = [0, 1]
// CHECK-SAME: planPhysicalBlockShape = [8, 16]
// CHECK: arts.edt <task> <internode>
// CHECK-LABEL: func.func @rank1_db_from_nonleading_physical_owner_dim_can_be_distributed
// CHECK: arts.db_alloc
// CHECK-SAME: <block>
// CHECK-SAME: distributed
// CHECK-SAME: owner_block_shape = [16]
// CHECK-SAME: owner_map_dims = [0]
// CHECK-SAME: owner_map_kind = #arts.owner_map_kind<owner_dim_contiguous>
// CHECK-SAME: owner_map_version = 1 : i32
// CHECK-SAME: planLogicalWorkerSlice = [16]
// CHECK-SAME: planOwnerDims = [1]
// CHECK-SAME: planPhysicalBlockShape = [8, 16]
// CHECK: arts.edt <task> <internode>
// CHECK-LABEL: func.func @rank3_transposed_owner_dims_project_full_physical_block_shape
// CHECK: arts.db_alloc
// CHECK-SAME: <block>
// CHECK-SAME: distributed
// CHECK-SAME: owner_block_shape = [32, 16, 8]
// CHECK-SAME: owner_map_dims = [0, 1, 2]
// CHECK-SAME: owner_map_kind = #arts.owner_map_kind<owner_dim_contiguous>
// CHECK-SAME: owner_map_version = 1 : i32
// CHECK-SAME: planLogicalWorkerSlice = [8, 16, 32]
// CHECK-SAME: planOwnerDims = [2, 1, 0]
// CHECK-SAME: planPhysicalBlockShape = [8, 16, 32]
// CHECK: arts.edt <task> <internode>

module {
  func.func @rank1_block_alloc_can_be_distributed() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c8 = arith.constant 8 : index
    %c16 = arith.constant 16 : index
    %route = arith.constant 0 : i32
    %value = arith.constant 1.0 : f64

    %guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <block>] route(%route : i32) sizes[%c8] elementType(f64) elementSizes[%c16] {distribution_kind = #arts.distribution_kind<block>, planLogicalWorkerSlice = [16], planOwnerDims = [0], planPhysicalBlockShape = [16]} : (memref<?xi64>, memref<?xmemref<?xf64>>)
    %acq_guid, %acq_ptr = arts.db_acquire[<inout>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<?xf64>>) partitioning(<block>), indices[], offsets[%c0], sizes[%c1] -> (memref<?xi64>, memref<?xmemref<?xf64>>)

    arts.edt <task> <internode> route(%route) (%acq_ptr) : memref<?xmemref<?xf64>> attributes {distribution_kind = #arts.distribution_kind<block>, planLogicalWorkerSlice = [16], planOwnerDims = [0], planPhysicalBlockShape = [16]} {
    ^bb0(%dep: memref<?xmemref<?xf64>>):
      %payload = arts.db_ref %dep[%c0] : memref<?xmemref<?xf64>> -> memref<?xf64>
      memref.store %value, %payload[%c0] : memref<?xf64>
      arts.db_release(%dep) : memref<?xmemref<?xf64>>
      arts.yield
    }

    arts.db_release(%acq_ptr) : memref<?xmemref<?xf64>>
    return
  }

  func.func @rank2_tile_alloc_can_be_distributed() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c2 = arith.constant 2 : index
    %c4 = arith.constant 4 : index
    %c8 = arith.constant 8 : index
    %c16 = arith.constant 16 : index
    %route = arith.constant 0 : i32
    %value = arith.constant 1.0 : f64

    %guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <block>] route(%route : i32) sizes[%c2, %c4] elementType(f64) elementSizes[%c8, %c16] {distribution_kind = #arts.distribution_kind<block>, planLogicalWorkerSlice = [8, 16], planOwnerDims = [0, 1], planPhysicalBlockShape = [8, 16]} : (memref<?x?xi64>, memref<?x?xmemref<?x?xf64>>)
    %acq_guid, %acq_ptr = arts.db_acquire[<inout>] (%guid : memref<?x?xi64>, %ptr : memref<?x?xmemref<?x?xf64>>) partitioning(<block>), indices[], offsets[%c1, %c2], sizes[%c1, %c1] -> (memref<?x?xi64>, memref<?x?xmemref<?x?xf64>>)

    arts.edt <task> <internode> route(%route) (%acq_ptr) : memref<?x?xmemref<?x?xf64>> attributes {distribution_kind = #arts.distribution_kind<block>, planLogicalWorkerSlice = [8, 16], planOwnerDims = [0, 1], planPhysicalBlockShape = [8, 16]} {
    ^bb0(%dep: memref<?x?xmemref<?x?xf64>>):
      %payload = arts.db_ref %dep[%c0, %c0] : memref<?x?xmemref<?x?xf64>> -> memref<?x?xf64>
      memref.store %value, %payload[%c0, %c0] : memref<?x?xf64>
      arts.db_release(%dep) : memref<?x?xmemref<?x?xf64>>
      arts.yield
    }

    arts.db_release(%acq_ptr) : memref<?x?xmemref<?x?xf64>>
    return
  }

  func.func @rank1_db_from_nonleading_physical_owner_dim_can_be_distributed() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index
    %c8 = arith.constant 8 : index
    %c16 = arith.constant 16 : index
    %route = arith.constant 0 : i32
    %value = arith.constant 1.0 : f64

    %guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <block>] route(%route : i32) sizes[%c4] elementType(f64) elementSizes[%c8, %c16] {distribution_kind = #arts.distribution_kind<block>, planLogicalWorkerSlice = [16], planOwnerDims = [1], planPhysicalBlockShape = [8, 16]} : (memref<?xi64>, memref<?xmemref<?x?xf64>>)
    %acq_guid, %acq_ptr = arts.db_acquire[<inout>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<?x?xf64>>) partitioning(<block>), indices[], offsets[%c1], sizes[%c1] -> (memref<?xi64>, memref<?xmemref<?x?xf64>>)

    arts.edt <task> <internode> route(%route) (%acq_ptr) : memref<?xmemref<?x?xf64>> attributes {distribution_kind = #arts.distribution_kind<block>, planLogicalWorkerSlice = [16], planOwnerDims = [1], planPhysicalBlockShape = [8, 16]} {
    ^bb0(%dep: memref<?xmemref<?x?xf64>>):
      %payload = arts.db_ref %dep[%c0] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
      memref.store %value, %payload[%c0, %c0] : memref<?x?xf64>
      arts.db_release(%dep) : memref<?xmemref<?x?xf64>>
      arts.yield
    }

    arts.db_release(%acq_ptr) : memref<?xmemref<?x?xf64>>
    return
  }

  func.func @rank3_transposed_owner_dims_project_full_physical_block_shape() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c2 = arith.constant 2 : index
    %c4 = arith.constant 4 : index
    %c8 = arith.constant 8 : index
    %c16 = arith.constant 16 : index
    %c32 = arith.constant 32 : index
    %route = arith.constant 0 : i32
    %value = arith.constant 1.0 : f64

    %guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <block>] route(%route : i32) sizes[%c4, %c2, %c1] elementType(f64) elementSizes[%c8, %c16, %c32] {distribution_kind = #arts.distribution_kind<block>, planLogicalWorkerSlice = [8, 16, 32], planOwnerDims = [2, 1, 0], planPhysicalBlockShape = [8, 16, 32]} : (memref<?x?x?xi64>, memref<?x?x?xmemref<?x?x?xf64>>)
    %acq_guid, %acq_ptr = arts.db_acquire[<inout>] (%guid : memref<?x?x?xi64>, %ptr : memref<?x?x?xmemref<?x?x?xf64>>) partitioning(<block>), indices[], offsets[%c0, %c0, %c0], sizes[%c1, %c1, %c1] -> (memref<?x?x?xi64>, memref<?x?x?xmemref<?x?x?xf64>>)

    arts.edt <task> <internode> route(%route) (%acq_ptr) : memref<?x?x?xmemref<?x?x?xf64>> attributes {distribution_kind = #arts.distribution_kind<block>, planLogicalWorkerSlice = [8, 16, 32], planOwnerDims = [2, 1, 0], planPhysicalBlockShape = [8, 16, 32]} {
    ^bb0(%dep: memref<?x?x?xmemref<?x?x?xf64>>):
      %payload = arts.db_ref %dep[%c0, %c0, %c0] : memref<?x?x?xmemref<?x?x?xf64>> -> memref<?x?x?xf64>
      memref.store %value, %payload[%c0, %c0, %c0] : memref<?x?x?xf64>
      arts.db_release(%dep) : memref<?x?x?xmemref<?x?x?xf64>>
      arts.yield
    }

    arts.db_release(%acq_ptr) : memref<?x?x?xmemref<?x?x?xf64>>
    return
  }
}
