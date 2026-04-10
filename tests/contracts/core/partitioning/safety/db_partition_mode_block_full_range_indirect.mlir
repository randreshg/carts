// RUN: %carts-compile %S/../../../../examples/mixed_access/mixed_access.mlir --O3 --arts-config %S/../../../../examples/arts.cfg --pipeline db-partitioning | %FileCheck %s

// Indirect indexed reads (e.g., nodeData[nodelist[e][j]]) through a
// block-allocated i32 array should preserve block distribution kind
// on the acquire, even when the access pattern is full-range.
// CHECK-LABEL: func.func @main
// CHECK: %[[GUID:.*]], %[[PTR:.*]] = arts.db_alloc[{{.*}}<block>, <indexed>{{.*}}] route(%c-1_i32 : i32) sizes[%{{.*}}] elementType(i32)
// CHECK: arts.db_acquire[<in>] (%[[GUID]] : memref<?xi64>, %[[PTR]] : memref<?xmemref<?x?xi32>>) partitioning(<block>
// CHECK-SAME: distribution_kind = #arts.distribution_kind<block>
