// RUN: %carts-compile %S/../inputs/vel4sg_base.mlir --arts-config %S/../../../examples/arts.cfg --stop-at concurrency-opt | %FileCheck %s

// CHECK-LABEL: func.func @main
// CHECK: arts.db_alloc[<in>, <heap>, <read>, <block>, <uniform>]
// CHECK: arts.db_acquire[<in>]
// CHECK-SAME: partitioning(<block>, offsets[
// CHECK-SAME: stencil_center_offset = 1 : i64
// CHECK: arts.db_acquire[<inout>]
// CHECK-SAME: partitioning(<block>, offsets[
