// RUN: %carts-compile %S/../examples/jacobi/for/jacobi-for.mlir --O3 --arts-config %S/inputs/arts_64t.cfg --pipeline concurrency-opt | %FileCheck %s

// CHECK: arts.db_alloc[<in>, <heap>, <read>, <stencil>, <uniform>] route(%c0_i32 : i32) sizes[%{{.+}}] elementType(f64) elementSizes[%{{.+}}, %c100]
// CHECK: arts.epoch attributes {depPattern = #arts.dep_pattern<jacobi_alternating_buffers>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<stencil>
// CHECK: scf.for %{{.*}} = %c0 to %c12 step %c1 {
// CHECK: arith.minui %{{.+}}, %c12 : index
// CHECK-NOT: scf.for %{{.*}} = %c0 to %c64 step %c1 {

module {
}
