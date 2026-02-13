// RUN: %carts-compile %S/../examples/addition/addition.mlir --O3 --arts-config %S/../examples/arts.cfg --stop-at concurrency-opt | %FileCheck %s

// CHECK: arts.db_alloc[{{.*}}<block>, <indexed>{{.*}}]
// CHECK: partitioning(<block>{{.*}})
