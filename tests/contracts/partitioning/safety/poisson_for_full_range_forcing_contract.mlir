// RUN: sh -c 'CARTS_COMPILE_WORKDIR=%t.compile %carts compile %S/../../../../external/carts-benchmarks/kastors-jacobi/poisson-for/poisson-for.c --pipeline db-partitioning --arts-config %S/../../../examples/arts.cfg >/dev/null && cat %t.compile/poisson-for.db-partitioning.mlir' | %FileCheck %s

// Verify that the poisson-for benchmark lowers forcing-array reads to
// stencil-partitioned halo slices with explicit element windows, while the
// owned writeback acquire remains block-partitioned and carries the stencil
// center offset contract.
// CHECK-LABEL: func.func @main
// CHECK: arts.db_alloc[<inout>, <heap>, <write>, <block>, <stencil>]
// CHECK: arts.db_acquire[<in>]
// CHECK-SAME: partitioning(<block>)
// CHECK-SAME: element_offsets[
// CHECK-SAME: element_sizes[
// CHECK: arts.db_acquire[<out>]
// CHECK-SAME: partitioning(<block>)
// CHECK: centerOffset = 1 : i64
