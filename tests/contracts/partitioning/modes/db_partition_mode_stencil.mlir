// RUN: %carts-compile %S/../../../examples/jacobi/for/jacobi-for.mlir --O3 --arts-config %S/../../../examples/arts.cfg --pipeline db-partitioning | %FileCheck %s

// CHECK-DAG: arts.db_alloc[{{.*}}<stencil>, <stencil>{{.*}}]
// CHECK: arts.db_acquire[<in>] {{.*}}partitioning(<block>), offsets[
// CHECK-SAME: element_offsets[
// CHECK-SAME: element_sizes[
// CHECK: arts.lowering_contract
// CHECK-SAME: centerOffset = 1 : i64
