// RUN: not %carts-compile %s --arts-config %arts_config --start-from create-dbs --pipeline create-dbs 2>&1 \
// RUN:   | %FileCheck %s --implicit-check-not="db_memory_placement = #arts.db_memory_placement<node_local>"

// CreateDbs is only the conservative raw-memref bridge: it may not invent block
// storage. When a raw memref reaches it carrying a committed SDE/CODIR physical
// block layout (planOwnerDims + planPhysicalBlockShape on the consuming task),
// the MU/token materialization upstream did not run. ARTS must fail closed here
// rather than silently lower the partitioned MU to a coarse node-local DB.

// CHECK: error: {{.*}}physical DB layout reached CreateDbs as a raw memref

module attributes {arts.runtime_total_nodes = 4 : i64, arts.runtime_total_workers = 256 : i64} {
  func.func @partitioned_raw_memref_fails_closed() {
    %route = arith.constant 0 : i32
    %scratch = memref.alloca() : memref<16xf64>
    arts.edt <task> <internode> route(%route) attributes {planOwnerDims = [0], planPhysicalBlockShape = [16]} {
      %c0 = arith.constant 0 : index
      %v = arith.constant 1.0 : f64
      memref.store %v, %scratch[%c0] : memref<16xf64>
      arts.yield
    }
    return
  }
}
