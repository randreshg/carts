// RUN: %carts-compile %S/../../../inputs/snapshots/vel4sg_base_default_openmp_to_arts.mlir --pipeline db-partitioning --arts-config %S/../../../../examples/arts.cfg | %FileCheck %s

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
