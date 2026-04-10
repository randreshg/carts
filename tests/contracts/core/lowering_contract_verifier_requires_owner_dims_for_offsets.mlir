// RUN: not %carts-compile %s --arts-config %S/../../examples/arts.cfg --start-from post-db-refinement --pipeline post-db-refinement 2>&1 | %FileCheck %s

// CHECK: 'arts.lowering_contract' op min_offsets/max_offsets/write_footprint require owner_dims

module {
  func.func @main() {
    %c0 = arith.constant 0 : index
    %alloc = memref.alloc() : memref<16xf32>
    arts.lowering_contract(%alloc : memref<16xf32>) min_offsets[%c0] max_offsets[%c0]
    return
  }
}
