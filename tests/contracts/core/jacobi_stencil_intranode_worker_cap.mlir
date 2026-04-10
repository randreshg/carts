// RUN: %carts-compile %S/../../examples/jacobi/for/jacobi-for.mlir --O3 --arts-config %S/../inputs/arts_64t.cfg --pipeline db-partitioning | %FileCheck %s

// CHECK-DAG: %c12 = arith.constant 12 : index
// CHECK: arts.db_alloc[<in>, <heap>, <read>, <stencil>, <stencil>] route(%c-1_i32 : i32) sizes[%{{.+}}] elementType(f64) elementSizes[%{{.+}}, %c100]
// CHECK: arts.epoch attributes {arts.pattern_revision = 1 : i64
// CHECK-SAME: depPattern = #arts.dep_pattern<jacobi_alternating_buffers>
// CHECK-SAME: distribution_kind = #arts.distribution_kind<block>
// CHECK-SAME: distribution_pattern = #arts.distribution_pattern<stencil>
// CHECK-NOT: arts.runtime_query <total_workers>
// CHECK-NOT: arith.minui %{{.+}}, %c64 : index

module {
}
