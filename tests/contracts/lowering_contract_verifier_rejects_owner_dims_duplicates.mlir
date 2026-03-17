// RUN: not %carts-compile %s --arts-config %S/../examples/arts.cfg --start-from concurrency-opt --pipeline concurrency-opt 2>&1 | %FileCheck %s

// CHECK: 'arts.lowering_contract' op owner_dims contains duplicate value: 0

module {
  func.func @main() {
    %c0 = arith.constant 0 : index
    %alloc = memref.alloc() : memref<16xf32>
    arts.lowering_contract(%alloc : memref<16xf32>) block_shape[%c0, %c0] {owner_dims = array<i64: 0, 0>}
    return
  }
}
