// RUN: sh -c '%S/../../../../tools/carts compile %S/../../../../external/carts-benchmarks/sw4lite/vel4sg-base/vel4sg_base.c --stop-at concurrency-opt --arts-config %S/../../../examples/arts.cfg || true' | %FileCheck %s

// Verify vel4sg gets block partitioning for both read and write acquires.
// CHECK-LABEL: func.func @main
// CHECK: arts.db_alloc[{{.*}}<block>{{.*}}]
// CHECK: arts.db_acquire[<in>]
// CHECK: partitioning(<block>, offsets[
// CHECK: arts.db_acquire[<inout>]
// CHECK: partitioning(<block>, offsets[
