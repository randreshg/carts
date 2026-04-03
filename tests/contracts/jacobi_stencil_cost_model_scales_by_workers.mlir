// RUN: %carts-compile %S/../examples/jacobi/for/jacobi-for.mlir --O3 --arts-config %S/inputs/arts_64t.cfg --pipeline db-partitioning | %FileCheck %s --check-prefix=CHECK-64
// RUN: %carts-compile %S/../examples/jacobi/for/jacobi-for.mlir --O3 --arts-config %S/inputs/arts_2t.cfg --pipeline db-partitioning | %FileCheck %s --check-prefix=CHECK-2

// The stencil owned-strip policy must scale with available workers. High
// worker counts should coarsen the strip; low worker counts should keep the
// natural block shape instead of forcing an extra partition hint.

// CHECK-64: %c12 = arith.constant 12 : index
// CHECK-64: %c9 = arith.constant 9 : index
// CHECK-64-NOT: arts.runtime_query <total_workers>
// CHECK-64: arts.db_acquire[<in>] {{.*}} {arts.partition_hint = {blockSize = 9 : i64, mode = 1 : i8},

// CHECK-2: arts.runtime_query <total_workers>
// CHECK-2: arts.db_alloc[<inout>, <heap>, <write>, <stencil>, <stencil>] route(%c-1_i32 : i32) sizes[%{{.+}}] elementType(f64) elementSizes[%{{.+}}, %c100]
// CHECK-2-NOT: arts.partition_hint = {blockSize =

module {
}
