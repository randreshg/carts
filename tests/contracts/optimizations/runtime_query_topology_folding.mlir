// RUN: %carts-compile %S/../../examples/parallel/parallel.mlir --O3 --arts-config %S/../../examples/arts.cfg --stop-at concurrency | %FileCheck %s

// CHECK: module attributes {
// CHECK-SAME: arts.runtime_total_nodes = 1 : i64
// CHECK-SAME: arts.runtime_total_workers = 8 : i64
// CHECK-NOT: arts.runtime_query <total_workers>
// CHECK-NOT: arts.runtime_query <total_nodes>
// CHECK: arts.runtime_query <current_worker>

module {}
