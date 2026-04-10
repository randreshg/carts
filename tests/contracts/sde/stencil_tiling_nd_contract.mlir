// RUN: %carts-compile %s --O3 --arts-config %S/../../examples/arts.cfg --pipeline pattern-pipeline | %FileCheck %s

// CHECK-LABEL: func.func @main
// CHECK: arts.edt <parallel>
// CHECK: depPattern = #arts.dep_pattern<stencil_tiling_nd>
// CHECK: stencil_max_offsets = [1, 1]
// CHECK: stencil_min_offsets = [-1, -1]
// CHECK: stencil_owner_dims = [0, 1]
// CHECK: stencil_supported_block_halo

module {
  func.func @main(%A: memref<16x16xf64>, %B: memref<16x16xf64>) {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c15 = arith.constant 15 : index
    omp.parallel {
      omp.wsloop {
        omp.loop_nest (%i) : index = (%c1) to (%c15) step (%c1) {
        scf.for %j = %c1 to %c15 step %c1 {
          %im1 = arith.subi %i, %c1 : index
          %ip1 = arith.addi %i, %c1 : index
          %jm1 = arith.subi %j, %c1 : index
          %jp1 = arith.addi %j, %c1 : index
          %a0 = memref.load %A[%im1, %j] : memref<16x16xf64>
          %a1 = memref.load %A[%i, %jm1] : memref<16x16xf64>
          %a2 = memref.load %A[%i, %j] : memref<16x16xf64>
          %a3 = memref.load %A[%i, %jp1] : memref<16x16xf64>
          %a4 = memref.load %A[%ip1, %j] : memref<16x16xf64>
          %s0 = arith.addf %a0, %a1 : f64
          %s1 = arith.addf %a2, %a3 : f64
          %s2 = arith.addf %s0, %s1 : f64
          %sum = arith.addf %s2, %a4 : f64
          memref.store %sum, %B[%i, %j] : memref<16x16xf64>
        }
        omp.yield
      }
      }
      omp.terminator
    }
    return
  }
}
