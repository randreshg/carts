// RUN: %carts-compile %S/../../../examples/rows/chunks/rowchunk.mlir --O3 --arts-config %S/../../../examples/arts.cfg --pipeline db-partitioning | %FileCheck %s

// CHECK-LABEL: func.func @main
// CHECK: %[[IN_GUID:.*]], %[[IN_PTR:.*]] = arts.db_alloc[{{.*}}<block>, <indexed>{{.*}}]
// CHECK: %[[OUT_GUID:.*]], %[[OUT_PTR:.*]] = arts.db_alloc[{{.*}}<block>, <indexed>{{.*}}]
// CHECK: %[[IN_ACQ_GUID:.*]], %[[IN_ACQ_PTR:.*]] = arts.db_acquire[<in>] (%[[IN_GUID]] : memref<?xi64>, %[[IN_PTR]] : memref<?xmemref<?x?xf32>>) partitioning(<coarse>), offsets[
// CHECK-SAME: ], sizes[
// CHECK: %[[OUT_ACQ_GUID:.*]], %[[OUT_ACQ_PTR:.*]] = arts.db_acquire[<out>] (%[[OUT_GUID]] : memref<?xi64>, %[[OUT_PTR]] : memref<?xmemref<?x?xf32>>) partitioning(<block>, offsets[
// CHECK-SAME: ]), offsets[
