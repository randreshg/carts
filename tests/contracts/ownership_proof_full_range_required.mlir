// RUN: %carts-compile %s --arts-config %S/../examples/arts.cfg --start-from post-db-refinement --pipeline post-db-refinement | %FileCheck %s

// Verify that a stencil contract without offsets gets halo_legality = false,
// meaning downstream passes must use full-range fallback instead of
// trusting a proven halo shape.

// CHECK: arts.lowering_contract
// CHECK-SAME: arts.proof.halo_legality = false

module {
  func.func @main() {
    %c8 = arith.constant 8 : index
    %alloc = memref.alloc() : memref<32x32xf64>
    arts.lowering_contract(%alloc : memref<32x32xf64>)
      pattern(<depPattern = <stencil>>)
      block_shape[%c8]
      contract(<ownerDims = [0]>)
    return
  }
}
