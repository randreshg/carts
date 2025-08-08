// RUN: %carts opt %s --edt -debug-only=edt-graph 2>&1 | %filecheck %s --check-prefix=EDTAN

// Verify EdtGraph build emits expected debug tracing on a simple task pattern

module {
  func.func @edt_test(%A: memref<4xi32>) {
    %c0 = arith.constant 0 : index
    arts.edt "t1" {
      %v = arith.constant 7 : i32
      memref.store %v, %A[%c0] : memref<4xi32>
      arts.edt_terminator
    }
    return
  }
}

// EDTAN: Creating EDT graph for function: edt_test
// EDTAN: Building EDT graph

