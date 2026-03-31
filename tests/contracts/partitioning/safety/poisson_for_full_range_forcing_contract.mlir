// RUN: sh -c 'CARTS_COMPILE_WORKDIR=%t.compile %S/../../../../tools/carts compile %S/../../../../external/carts-benchmarks/kastors-jacobi/poisson-for/poisson-for.c --pipeline concurrency-opt --arts-config %S/../../../examples/arts.cfg >/dev/null && cat %t.compile/poisson-for.concurrency-opt.mlir' | %FileCheck %s

// Verify that the poisson-for benchmark lowers forcing-array reads to
// stencil-partitioned halo slices with explicit element windows, while the
// owned writeback acquire remains block-partitioned and carries the stencil
// center offset contract.
// CHECK-LABEL: func.func @main
// CHECK: arts.db_alloc[<inout>, <heap>, <write>, <block>, <uniform>]
// CHECK: arts.db_acquire[<in>]
// CHECK-SAME: partitioning(<stencil>)
// CHECK-SAME: element_offsets[
// CHECK-SAME: element_sizes[
// CHECK: arts.db_acquire[<out>]
// CHECK-SAME: partitioning(<block>)
// CHECK-SAME: stencil_center_offset = 1 : i64
