// RUN: %carts-compile %s --arts-config %S/../../examples/arts.cfg --start-from post-db-refinement --pipeline post-db-refinement | %FileCheck %s

// Verify that a stencil contract with complete owner dims, offsets, and
// supported_block_halo gets all 5 proof dimensions stamped as true.

// CHECK: arts.lowering_contract
// CHECK-SAME: arts.proof.dep_slice_soundness = true
// CHECK-SAME: arts.proof.halo_legality = true
// CHECK-SAME: arts.proof.owner_dim_reachability = true
// CHECK-SAME: arts.proof.partition_access_mapping = true
// CHECK-SAME: arts.proof.relaunch_state_soundness = true

module {
  func.func @main() {
    %c1 = arith.constant 1 : index
    %c8 = arith.constant 8 : index
    %alloc = memref.alloc() : memref<32x32xf64>
    arts.lowering_contract(%alloc : memref<32x32xf64>)
      pattern(<depPattern = <stencil>>)
      block_shape[%c8]
      min_offsets[%c1]
      max_offsets[%c1]
      contract(<ownerDims = [0], supportedBlockHalo = true, narrowableDep = true>)
    return
  }
}
