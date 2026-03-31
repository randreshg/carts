// RUN: %carts-compile %S/../../../examples/jacobi/for/jacobi-for.mlir --O3 --arts-config %S/../../../examples/arts.cfg --pipeline concurrency-opt | %FileCheck %s

// CHECK-DAG: arts.db_alloc[{{.*}}<block>, <stencil>{{.*}}]
// CHECK: arts.db_acquire[<in>] {{.*}}partitioning(<block>), offsets[
// CHECK-SAME: element_offsets[
// CHECK-SAME: element_sizes[
// CHECK-SAME: stencil_center_offset = 1 : i64
