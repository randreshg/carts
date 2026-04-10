// RUN: %carts-compile %S/../../../../examples/mixed_access/mixed_access.mlir --O3 --arts-config %S/../../../../examples/arts.cfg --pipeline db-partitioning | %FileCheck %s

// CHECK-LABEL: func.func @main
// CHECK: %[[GUID:.*]], %[[PTR:.*]] = arts.db_alloc[{{.*}}<block>, <indexed>{{.*}}] route(%c-1_i32 : i32) sizes[%{{.*}}] elementType(i32)
// CHECK: %[[ACQ_GUID:.*]], %[[ACQ_PTR:.*]] = arts.db_acquire[<in>] (%[[GUID]] : memref<?xi64>, %[[PTR]] : memref<?xmemref<?x?xi32>>) partitioning(<coarse>), offsets[
// CHECK-SAME: ], sizes[
// CHECK-SAME: distribution_kind = #arts.distribution_kind<block>
