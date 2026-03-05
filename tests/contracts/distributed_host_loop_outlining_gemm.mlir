// RUN: %carts-compile %S/../../external/carts-benchmarks/polybench/gemm/gemm.mlir --O3 --arts-config %S/../../docker/arts-docker-2node.cfg --distributed-db --stop-at loop-reordering --debug-only=distributed_host_loop_outlining 2>&1 | %FileCheck %s --check-prefix=OUTLINE
// RUN: %carts-compile %S/../../external/carts-benchmarks/polybench/gemm/gemm.mlir --O3 --arts-config %S/../../docker/arts-docker-2node.cfg --distributed-db --stop-at concurrency-opt --debug-only=distributed_db_ownership 2>&1 | %FileCheck %s --check-prefix=OWNERSHIP

// OUTLINE: DistributedHostLoopOutlining outlined 2 / 2 eligible host loop(s)
// OWNERSHIP: DistributedDbOwnership marked 2 / {{[1-9][0-9]*}} DbAlloc operations

module {
  // RUN/FileCheck contract only.
}
