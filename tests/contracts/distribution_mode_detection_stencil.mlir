// RUN: %carts-compile %S/../examples/jacobi/for/jacobi-for.mlir --O3 --arts-config %S/../examples/arts.cfg --stop-at edt-distribution | %FileCheck %s --check-prefix=INTRA
// RUN: %carts-compile %S/../examples/jacobi/for/jacobi-for.mlir --O3 --arts-config %S/../../docker/arts-docker-2node.cfg --stop-at edt-distribution | %FileCheck %s --check-prefix=INTER

// INTRA: distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<stencil>
// INTER: distribution_kind = #arts.distribution_kind<two_level>, distribution_pattern = #arts.distribution_pattern<stencil>
