// RUN: %carts-compile %s --O3 --arts-config %S/../../examples/arts.cfg --pipeline pattern-pipeline | %FileCheck %s

// CHECK-LABEL: func.func @main
// CHECK: arts.edt <parallel>
// CHECK: depPattern = #arts.dep_pattern<cross_dim_stencil_3d>
// CHECK: stencil_owner_dims = [0, 1, 2]
// CHECK: stencil_supported_block_halo

module {
  func.func @main(%A: memref<16x16x16xf64>, %B: memref<16x16x16xf64>) {
    %c1 = arith.constant 1 : index
    %c15 = arith.constant 15 : index
    omp.parallel {
      omp.wsloop {
        omp.loop_nest (%i) : index = (%c1) to (%c15) step (%c1) {
        scf.for %j = %c1 to %c15 step %c1 {
          scf.for %k = %c1 to %c15 step %c1 {
            %im1 = arith.subi %i, %c1 : index
            %ip1 = arith.addi %i, %c1 : index
            %jm1 = arith.subi %j, %c1 : index
            %jp1 = arith.addi %j, %c1 : index
            %km1 = arith.subi %k, %c1 : index
            %kp1 = arith.addi %k, %c1 : index
            %x0 = memref.load %A[%im1, %j, %k] : memref<16x16x16xf64>
            %x1 = memref.load %A[%ip1, %j, %k] : memref<16x16x16xf64>
            %y0 = memref.load %A[%i, %jm1, %k] : memref<16x16x16xf64>
            %y1 = memref.load %A[%i, %jp1, %k] : memref<16x16x16xf64>
            %z0 = memref.load %A[%i, %j, %km1] : memref<16x16x16xf64>
            %z1 = memref.load %A[%i, %j, %kp1] : memref<16x16x16xf64>
            %s0 = arith.addf %x0, %x1 : f64
            %s1 = arith.addf %y0, %y1 : f64
            %s2 = arith.addf %z0, %z1 : f64
            %s3 = arith.addf %s0, %s1 : f64
            %sum = arith.addf %s2, %s3 : f64
            memref.store %sum, %B[%i, %j, %k] : memref<16x16x16xf64>
          }
        }
        omp.yield
      }
      }
      omp.terminator
    }
    return
  }
}
