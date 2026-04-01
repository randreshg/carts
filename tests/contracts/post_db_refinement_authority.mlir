// RUN: %carts-compile %S/../examples/jacobi/for/jacobi-for.mlir --O3 --arts-config %S/../examples/arts.cfg --pipeline concurrency-opt | %FileCheck %s --check-prefix=STENCIL-CONC
// RUN: %carts-compile %S/../examples/jacobi/for/jacobi-for.mlir --O3 --arts-config %S/../examples/arts.cfg --pipeline pre-lowering | %FileCheck %s --check-prefix=STENCIL-PRE
// RUN: %carts-compile %S/inputs/uniform_block.mlir --O3 --arts-config %S/../examples/arts.cfg --pipeline concurrency-opt | %FileCheck %s --check-prefix=UNIFORM-CONC
// RUN: %carts-compile %S/inputs/uniform_block.mlir --O3 --arts-config %S/../examples/arts.cfg --pipeline pre-lowering | %FileCheck %s --check-prefix=UNIFORM-PRE

// STENCIL-CONC: arts.lowering_contract({{.*}}) dep_pattern(<jacobi_alternating_buffers>) distribution_kind(<block>) distribution_pattern(<stencil>) {{.*}}distribution_version = 1 : i64{{.*}}post_db_refined
// STENCIL-CONC-NOT: arts.lowering_contract({{.*}}){{.*}}distribution_version = 2 : i64

// UNIFORM-CONC: arts.lowering_contract({{.*}}) block_shape[
// UNIFORM-CONC-SAME: owner_dims = array<i64: 0>, post_db_refined
// UNIFORM-CONC-NOT: arts.lowering_contract({{.*}}){{.*}}distribution_version = 2 : i64

// STENCIL-PRE: arts.db_acquire[<in>] {{.*}}partitioning(<block>)
// STENCIL-PRE: arts.db_acquire[<inout>] {{.*}}partitioning(<block>
// STENCIL-PRE-NOT: partitioning(<coarse>)
// STENCIL-PRE-NOT: arts.lowering_contract({{.*}}memref<?xmemref<?x?xf64>>)

// UNIFORM-PRE: arts.db_acquire[<out>] {{.*}}partitioning(<block>
// UNIFORM-PRE-NOT: partitioning(<coarse>)
