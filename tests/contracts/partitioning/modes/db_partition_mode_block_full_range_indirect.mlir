// RUN: %carts-compile %S/../../../examples/mixed_access/mixed_access.mlir --O3 --arts-config %S/../../../examples/arts.cfg --stop-at concurrency-opt | %FileCheck %s

// CHECK-LABEL: func.func @main
// CHECK: arts.db_alloc[{{.*}}<block>, <indexed>{{.*}}]
// CHECK: arts.db_acquire[<in>] ({{.*}}) partitioning(<block>, offsets[
// CHECK-SAME: ]), offsets[%c0], sizes[
