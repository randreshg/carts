// RUN: %carts opt %s --edt --create-dbs --db -debug-only=arts-graph-manager 2>&1 | %filecheck %s --check-prefix=GRAPHS

// Verify ArtsGraphManager builds DbGraph and EdtGraph and can export DOT

module {
  func.func @graphs_test(%A: memref<4xi32>) {
    %c0 = arith.constant 0 : index
    arts.edt "use_db" {
      %v = arith.constant 1 : i32
      memref.store %v, %A[%c0] : memref<4xi32>
      arts.edt_terminator
    }
    return
  }
}

// GRAPHS: Creating ArtsGraphManager
// GRAPHS: Building graphs via ArtsGraphManager

