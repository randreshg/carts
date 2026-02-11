// RUN: %carts-run %S/../examples/dotproduct/dotproduct.mlir --O3 --arts-config %S/../examples/arts.cfg --stop-at edt-distribution | %FileCheck %s --check-prefix=INTRA
// RUN: %carts-run %S/../examples/dotproduct/dotproduct.mlir --O3 --arts-config %S/../../docker/arts-docker-2node.cfg --stop-at edt-distribution | %FileCheck %s --check-prefix=INTER

// INTRA: distribution_kind = #arts.distribution_kind<tiling_2d>
// INTRA: distribution_pattern = #arts.distribution_pattern<matmul>

// INTER: distribution_kind = #arts.distribution_kind<two_level>
// INTER: distribution_pattern = #arts.distribution_pattern<matmul>

