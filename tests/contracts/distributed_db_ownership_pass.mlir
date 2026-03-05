// RUN: %carts-compile %S/inputs/uniform_block.mlir --O3 --arts-config %S/../../docker/arts-docker-2node.cfg --distributed-db --stop-at concurrency-opt --debug-only=distributed_db_ownership 2>&1 | %FileCheck %s

// CHECK: DistributedDbOwnership marked 0 / {{[1-9][0-9]*}} DbAlloc operations
// CHECK: arts.db_alloc
