// RUN: %carts-compile %S/../../../../examples/rows/chunks/rowchunk.mlir --O3 --arts-config %S/../../../../examples/arts.cfg --pipeline db-partitioning | %FileCheck %s

// Verify that uniform direct writes to 2D row-chunked arrays produce
// block-partitioned allocations and block-partitioned acquires for both
// the read input and the write output.
// CHECK-LABEL: func.func @main
// CHECK: %[[IN_GUID:.*]], %[[IN_PTR:.*]] = arts.db_alloc[{{.*}}<block>, <indexed>{{.*}}]
// CHECK: arts.lowering_contract(%[[IN_PTR]] :
// CHECK: %[[OUT_GUID:.*]], %[[OUT_PTR:.*]] = arts.db_alloc[{{.*}}<block>, <indexed>{{.*}}]
// CHECK: arts.lowering_contract(%[[OUT_PTR]] :
// CHECK: arts.db_acquire[<in>] (%[[IN_GUID]] : memref<?xi64>, %[[IN_PTR]] : memref<?xmemref<?x?xf32>>) partitioning(<block>, offsets[
// CHECK-SAME: ]), offsets[
// CHECK: arts.db_acquire[<out>] (%[[OUT_GUID]] : memref<?xi64>, %[[OUT_PTR]] : memref<?xmemref<?x?xf32>>) partitioning(<block>, offsets[
// CHECK-SAME: ]), offsets[
