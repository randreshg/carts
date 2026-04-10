// RUN: sh -c 'CARTS_COMPILE_WORKDIR=%t.compile %carts compile %S/../../../../../external/carts-benchmarks/sw4lite/vel4sg-base/vel4sg_base.c --pipeline db-partitioning --arts-config %S/../../../../examples/arts.cfg >/dev/null && cat %t.compile/vel4sg_base.db-partitioning.mlir' | %FileCheck %s

// Verify vel4sg keeps its worker-local blocked reads and writes on the
// distributed k-owner dimension even though the backing allocations stay
// coarse until outlining.
// CHECK-LABEL: func.func @main
// CHECK: arts.db_alloc[{{.*}}<coarse>{{.*}}]
// CHECK: arts.db_acquire[<in>]
// CHECK: partitioning(<block>, offsets[
// CHECK: arts.db_acquire[<inout>]
// CHECK: partitioning(<block>, offsets[
// CHECK-NOT: offsets[%c0]
