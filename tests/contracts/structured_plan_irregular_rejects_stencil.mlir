// RUN: %carts-compile %s --O3 --arts-config %S/../examples/arts.cfg --pipeline pattern-pipeline | %FileCheck %s

// Test that an indirect-access loop does NOT produce stencil family attrs.
// Uses a level-of-indirection array to index into the main array, which
// prevents pattern discovery from classifying it as a stencil.
// Verifies:
//   1. The loop is NOT classified as a stencil
//   2. No stencil_owner_dims appear
//   3. No stencil_supported_block_halo appears

// CHECK-LABEL: func.func @main
// CHECK-NOT: distribution_pattern = #arts.distribution_pattern<stencil>
// CHECK-NOT: stencil_owner_dims
// CHECK-NOT: stencil_supported_block_halo

module {
  func.func @main(%data: memref<64xf64>, %idx: memref<64xi32>,
                  %out: memref<64xf64>) {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c64 = arith.constant 64 : index
    omp.parallel {
      omp.wsloop {
        omp.loop_nest (%i) : index = (%c0) to (%c64) step (%c1) {
          %raw = memref.load %idx[%i] : memref<64xi32>
          %j = arith.index_cast %raw : i32 to index
          %val = memref.load %data[%j] : memref<64xf64>
          memref.store %val, %out[%i] : memref<64xf64>
          omp.yield
        }
      }
      omp.terminator
    }
    return
  }
}
