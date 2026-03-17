// RUN: %carts-compile %S/../../../examples/rows/chunks/rowchunk.mlir --O3 --arts-config %S/../../../examples/arts.cfg --pipeline concurrency-opt | %FileCheck %s

// CHECK-LABEL: func.func @main
// CHECK: arts.db_alloc[{{.*}}<block>, <uniform>{{.*}}]
// CHECK: arts.db_acquire[<in>] ({{.*}}) partitioning(<block>, offsets[
// CHECK-SAME: ]), offsets[
// CHECK: arts.db_acquire[<inout>] ({{.*}}) partitioning(<block>, offsets[
// CHECK-SAME: ]), offsets[
