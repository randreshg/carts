// RUN: %carts-compile %s --O3 --arts-config %S/../examples/arts.cfg --pipeline pattern-pipeline | %FileCheck %s

// Test that a 2D jacobi-like stencil (5-point) produces stencil family
// dep_pattern and distribution_pattern attributes at pattern-pipeline.
// Verifies:
//   1. Pattern discovery recognizes the 5-point stencil access
//   2. depPattern contains a stencil family pattern
//   3. distribution_pattern is stencil
//   4. stencil_owner_dims and stencil offsets are computed

// CHECK-LABEL: func.func @main
// CHECK: arts.edt <parallel>
// CHECK: stencil_min_offsets = [-1
// CHECK: stencil_max_offsets = [1
// CHECK: stencil_owner_dims = [0
// CHECK: stencil_supported_block_halo

module {
  func.func @main(%A: memref<32x32xf64>, %B: memref<32x32xf64>) {
    %c1 = arith.constant 1 : index
    %c31 = arith.constant 31 : index
    omp.parallel {
      omp.wsloop {
        omp.loop_nest (%i) : index = (%c1) to (%c31) step (%c1) {
        scf.for %j = %c1 to %c31 step %c1 {
          %im1 = arith.subi %i, %c1 : index
          %ip1 = arith.addi %i, %c1 : index
          %jm1 = arith.subi %j, %c1 : index
          %jp1 = arith.addi %j, %c1 : index
          %n = memref.load %A[%im1, %j] : memref<32x32xf64>
          %s = memref.load %A[%ip1, %j] : memref<32x32xf64>
          %w = memref.load %A[%i, %jm1] : memref<32x32xf64>
          %e = memref.load %A[%i, %jp1] : memref<32x32xf64>
          %c = memref.load %A[%i, %j] : memref<32x32xf64>
          %s0 = arith.addf %n, %s : f64
          %s1 = arith.addf %w, %e : f64
          %s2 = arith.addf %s0, %s1 : f64
          %sum = arith.addf %s2, %c : f64
          memref.store %sum, %B[%i, %j] : memref<32x32xf64>
        }
        omp.yield
      }
      }
      omp.terminator
    }
    return
  }
}
