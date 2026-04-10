// RUN: %carts-compile %s --arts-config %S/../../examples/arts.cfg --start-from post-db-refinement --pipeline post-db-refinement | %FileCheck %s

// CHECK: arts.lowering_contract
// CHECK-SAME: supportedBlockHalo = true

module {
  func.func @main() {
    %c4 = arith.constant 4 : index
    %c8 = arith.constant 8 : index
    %cm1 = arith.constant -1 : index
    %c1 = arith.constant 1 : index
    %c0 = arith.constant 0 : index
    %alloc = memref.alloc() : memref<16x16xf64>
    arts.lowering_contract(%alloc : memref<16x16xf64>) pattern(<depPattern = <stencil>>) block_shape[%c4, %c8] min_offsets[%cm1, %cm1] max_offsets[%c1, %c1] write_footprint[%c0, %c0] contract(<ownerDims = [0, 1], spatialDims = [0, 1], supportedBlockHalo = true>)
    return
  }
}
