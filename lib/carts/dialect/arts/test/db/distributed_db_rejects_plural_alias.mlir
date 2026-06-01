// RUN: not %carts-compile %s --pipeline post-db-refinement --start-from post-db-refinement --arts-config %inputs_dir/arts_multinode.cfg --distributed-dbs 2>&1 \
// RUN:   | %FileCheck %s

module {
  func.func @plural_distributed_db_flag_is_not_an_alias() {
    return
  }
}

// CHECK: Unknown command line argument '--distributed-dbs'
// CHECK: Did you mean '--distributed-db'?
