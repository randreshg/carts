// RUN: %carts-compile %s --arts-config %S/../examples/arts.cfg --start-from post-db-refinement --pipeline post-db-refinement | %FileCheck %s

// Verify that a contract missing owner_dims does NOT get owner_dim_reachability
// or partition_access_mapping proven (they should be false).

// CHECK: arts.lowering_contract
// CHECK-NOT: arts.proof.owner_dim_reachability = true
// CHECK-NOT: arts.proof.partition_access_mapping = true

module {
  func.func @main() {
    %c8 = arith.constant 8 : index
    %alloc = memref.alloc() : memref<32xf64>
    arts.lowering_contract(%alloc : memref<32xf64>)
      pattern(<depPattern = <stencil>>)
      block_shape[%c8]
    return
  }
}
