// RUN: %carts-compile %S/../examples/jacobi/for/jacobi-for.mlir --O3 --arts-config %S/../examples/arts.cfg --pipeline concurrency-opt | %FileCheck %s --check-prefix=STENCIL-CONC
// RUN: %carts-compile %S/../examples/jacobi/for/jacobi-for.mlir --O3 --arts-config %S/../examples/arts.cfg --pipeline pre-lowering | %FileCheck %s --check-prefix=STENCIL-PRE
// RUN: %carts-compile %S/inputs/uniform_block.mlir --O3 --arts-config %S/../examples/arts.cfg --pipeline concurrency-opt | %FileCheck %s --check-prefix=UNIFORM-CONC
// RUN: %carts-compile %S/inputs/uniform_block.mlir --O3 --arts-config %S/../examples/arts.cfg --pipeline pre-lowering | %FileCheck %s --check-prefix=UNIFORM-PRE

// STENCIL-CONC: arts.lowering_contract({{.*}}) dep_pattern(<jacobi_alternating_buffers>) distribution_pattern(<stencil>) {{.*}}post_db_refined

// UNIFORM-CONC: arts.lowering_contract({{.*}}) block_shape[
// UNIFORM-CONC-SAME: owner_dims = array<i64: 0>, post_db_refined

// STENCIL-PRE: arts.db_acquire[<in>] {{.*}}partitioning(<block>)
// STENCIL-PRE: arts.db_acquire[<inout>] {{.*}}partitioning(<block>
// STENCIL-PRE-NOT: partitioning(<coarse>)

// UNIFORM-PRE: arts.db_acquire[<inout>] {{.*}}partitioning(<block>
// UNIFORM-PRE-NOT: partitioning(<coarse>)
