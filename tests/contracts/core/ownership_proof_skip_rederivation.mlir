// RUN: %carts-compile %s --arts-config %S/../../examples/arts.cfg --start-from post-db-refinement --pipeline post-db-refinement | %FileCheck %s

// Verify that a uniform contract with complete proof dimensions gets
// partition_access_mapping proven, which allows DbPartitioning to skip
// re-derivation of access mapping evidence.

// CHECK: arts.lowering_contract
// CHECK-SAME: arts.proof.dep_slice_soundness = true
// CHECK-SAME: arts.proof.partition_access_mapping = true

module {
  func.func @main() {
    %c8 = arith.constant 8 : index
    %alloc = memref.alloc() : memref<32xf64>
    arts.lowering_contract(%alloc : memref<32xf64>)
      pattern(<depPattern = <uniform>>)
      block_shape[%c8]
      contract(<ownerDims = [0]>)
    return
  }
}
