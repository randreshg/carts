// RUN: %carts-compile %S/../../../examples/jacobi/for/jacobi-for.mlir --O3 --arts-config %S/../../../examples/arts.cfg --pipeline db-partitioning | %FileCheck %s

// CHECK-DAG: arts.db_alloc[{{.*}}<stencil>, <stencil>{{.*}}]
// CHECK: arts.db_acquire[<in>] {{.*}}partitioning(<block>), offsets[
// CHECK-SAME: element_offsets[
// CHECK-SAME: element_sizes[
// CHECK-SAME: stencil_center_offset = 1 : i64
