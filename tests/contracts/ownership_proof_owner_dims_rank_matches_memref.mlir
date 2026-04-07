// RUN: %carts-compile %s --arts-config %S/../examples/arts.cfg --start-from post-db-refinement --pipeline post-db-refinement | %FileCheck %s

// CHECK: arts.lowering_contract

module {
  func.func @main() {
    %c4 = arith.constant 4 : index
    %c8 = arith.constant 8 : index
    %alloc = memref.alloc() : memref<16x16xf64>
    arts.lowering_contract(%alloc : memref<16x16xf64>) block_shape[%c4, %c8] contract(<ownerDims = [0, 1]>)
    return
  }
}
