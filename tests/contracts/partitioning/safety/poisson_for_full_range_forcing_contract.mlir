// RUN: sh -c 'CARTS_COMPILE_WORKDIR=%t.compile %carts compile %S/../../../../external/carts-benchmarks/kastors-jacobi/poisson-for/poisson-for.c --pipeline db-partitioning --arts-config %S/../../../examples/arts.cfg >/dev/null && cat %t.compile/poisson-for.db-partitioning.mlir' | %FileCheck %s

// Verify that the poisson-for benchmark lowers forcing-array reads to
// stencil-partitioned halo slices with explicit element windows, while the
// owned writeback acquire remains block-partitioned with the same explicit
// element window contract.
// CHECK-LABEL: func.func @main
// CHECK: arts.db_alloc[<inout>, <heap>, <write>, <block>, <stencil>]
// CHECK: arts.db_acquire[<in>]
// CHECK-SAME: partitioning(<block>
// CHECK-SAME: element_offsets[
// CHECK-SAME: element_sizes[
// CHECK-SAME: stencil_owner_dims = [0]
// CHECK: arts.lowering_contract(%{{.*}} : memref<?xmemref<?x?xf64>>)
// CHECK-SAME: contract(<ownerDims = [0]>)
// CHECK: arts.db_acquire[<out>]
// CHECK-SAME: partitioning(<block>)
// CHECK-SAME: element_offsets[
// CHECK-SAME: element_sizes[
// CHECK-SAME: stencil_owner_dims = [0]
// CHECK: arts.lowering_contract(%{{.*}} : memref<?xmemref<?x?xf64>>)
// CHECK-SAME: contract(<ownerDims = [0]>)
