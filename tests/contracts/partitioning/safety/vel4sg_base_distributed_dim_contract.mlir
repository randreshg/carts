// RUN: %carts-compile %S/../inputs/vel4sg_base.mlir --arts-config %S/../../../examples/arts.cfg --stop-at concurrency-opt | %FileCheck %s

// CHECK-LABEL: func.func @main
// CHECK-DAG: arts.db_alloc[<in>, <heap>, <read>, <block>, <uniform>]{{.*}}elementSizes[%8, %13, %21]
// CHECK-DAG: arts.db_alloc[<in>, <heap>, <read>, <stencil>, <uniform>]{{.*}}elementSizes[%21, %13, %c20]
// CHECK: arts.db_acquire[<in>] {{.*}} partitioning(<block>), offsets[%c0], sizes[%30] {{.*}}stencil_center_offset = 1 : i64
// CHECK: arts.db_acquire[<in>] {{.*}} partitioning(<block>, offsets[%{{.*}}], sizes[%{{.*}}]), offsets[%{{.*}}], sizes[%{{.*}}] {stencil_center_offset = 1 : i64}
// CHECK: arts.db_acquire[<inout>] {{.*}} partitioning(<block>, offsets[%{{.*}}], sizes[%{{.*}}]), offsets[%{{.*}}], sizes[%{{.*}}]
