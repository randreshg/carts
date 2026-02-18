// RUN: %carts-compile %S/../../external/carts-benchmarks/polybench/gemm/gemm.mlir --O3 --arts-config %S/../examples/arts.cfg --stop-at edt-distribution | %FileCheck %s --check-prefix=INTRA
// RUN: %carts-compile %S/../../external/carts-benchmarks/polybench/gemm/gemm.mlir --O3 --arts-config %S/../../docker/arts-docker-2node.cfg --stop-at edt-distribution | %FileCheck %s --check-prefix=INTER

// INTRA: distribution_kind = #arts.distribution_kind<block>
// INTRA: distribution_pattern = #arts.distribution_pattern<matmul>

// INTER: distribution_kind = #arts.distribution_kind<tiling_2d>
// INTER: distribution_pattern = #arts.distribution_pattern<matmul>
