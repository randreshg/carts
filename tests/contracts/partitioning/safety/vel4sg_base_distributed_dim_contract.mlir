// RUN: %carts-compile %S/../inputs/vel4sg_base.mlir --arts-config %S/../../../examples/arts.cfg --stop-at concurrency-opt | %FileCheck %s

// CHECK-LABEL: func.func @main
// CHECK-DAG: arts.db_alloc[<in>, <heap>, <read>, <block>, <uniform>]{{.*}}elementSizes[%8, %13, %21]
// CHECK-DAG: arts.db_alloc[<in>, <heap>, <read>, <block>, <uniform>]{{.*}}elementSizes[%21, %13, %c20]
// CHECK-DAG: arts.db_acquire[<in>]
// CHECK-DAG: offsets[%c0], sizes[%30]
// CHECK-DAG: stencil_center_offset = 1 : i64
// CHECK-DAG: arts.db_acquire[<inout>]
// CHECK-DAG: partitioning(<block>, offsets[
