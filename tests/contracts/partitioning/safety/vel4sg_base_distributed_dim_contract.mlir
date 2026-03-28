// RUN: sh -c 'CARTS_COMPILE_WORKDIR=%t.compile %S/../../../../tools/carts compile %S/../../../../external/carts-benchmarks/sw4lite/vel4sg-base/vel4sg_base.c --pipeline concurrency-opt --stop-after=db-partitioning --arts-config %S/../../../examples/arts.cfg >/dev/null && cat %t.compile/vel4sg_base.concurrency-opt.mlir' | %FileCheck %s

// Verify vel4sg keeps block partitioning on k-owned reads/writes and does not
// widen the k+/-1 read-only stencil inputs back to full-range acquires.
// CHECK-LABEL: func.func @main
// CHECK: arts.db_alloc[{{.*}}<block>{{.*}}]
// CHECK: arts.db_acquire[<in>]
// CHECK: partitioning(<block>, offsets[
// CHECK: arts.db_acquire[<inout>]
// CHECK: partitioning(<block>, offsets[
// CHECK-NOT: offsets[%c0]
