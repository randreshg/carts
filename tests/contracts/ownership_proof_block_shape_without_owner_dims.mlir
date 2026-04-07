// RUN: %carts-compile %s --arts-config %S/../examples/arts.cfg --start-from post-db-refinement --pipeline post-db-refinement | %FileCheck %s

// CHECK: arts.lowering_contract

module {
  func.func @main() {
    %c8 = arith.constant 8 : index
    %alloc = memref.alloc() : memref<16xf32>
    arts.lowering_contract(%alloc : memref<16xf32>) block_shape[%c8]
    return
  }
}
