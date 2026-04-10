// RUN: not %carts-compile %s --arts-config %S/../../examples/arts.cfg --start-from post-db-refinement --pipeline post-db-refinement 2>&1 | %FileCheck %s

// CHECK: 'arts.lowering_contract' op supported_block_halo requires a stencil-family dep_pattern

module {
  func.func @main() {
    %alloc = memref.alloc() : memref<16xf32>
    arts.lowering_contract(%alloc : memref<16xf32>) pattern(<depPattern = <uniform>>) contract(<supportedBlockHalo = true>)
    return
  }
}
