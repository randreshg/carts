// RUN: %carts-compile %s --O3 --arts-config %S/../examples/arts.cfg --pipeline pattern-pipeline | %FileCheck %s

// CHECK-LABEL: func.func @main
// CHECK: arts.edt <parallel>
// CHECK: depPattern = #arts.dep_pattern<higher_order_stencil>
// CHECK: stencil_max_offsets = [2, 2, 2]
// CHECK: stencil_min_offsets = [-2, -2, -2]
// CHECK: stencil_owner_dims = [0, 1, 2]
// CHECK: stencil_supported_block_halo

module {
  func.func @main(%A: memref<32x32x32xf64>, %B: memref<32x32x32xf64>) {
    %c1 = arith.constant 1 : index
    %c2 = arith.constant 2 : index
    %c30 = arith.constant 30 : index
    omp.parallel {
      omp.wsloop for (%i) : index = (%c2) to (%c30) step (%c1) {
        scf.for %j = %c2 to %c30 step %c1 {
          scf.for %k = %c2 to %c30 step %c1 {
            %im2 = arith.subi %i, %c2 : index
            %ip2 = arith.addi %i, %c2 : index
            %jm2 = arith.subi %j, %c2 : index
            %jp2 = arith.addi %j, %c2 : index
            %km2 = arith.subi %k, %c2 : index
            %kp2 = arith.addi %k, %c2 : index
            %x0 = memref.load %A[%im2, %j, %k] : memref<32x32x32xf64>
            %x1 = memref.load %A[%ip2, %j, %k] : memref<32x32x32xf64>
            %y0 = memref.load %A[%i, %jm2, %k] : memref<32x32x32xf64>
            %y1 = memref.load %A[%i, %jp2, %k] : memref<32x32x32xf64>
            %z0 = memref.load %A[%i, %j, %km2] : memref<32x32x32xf64>
            %z1 = memref.load %A[%i, %j, %kp2] : memref<32x32x32xf64>
            %s0 = arith.addf %x0, %x1 : f64
            %s1 = arith.addf %y0, %y1 : f64
            %s2 = arith.addf %z0, %z1 : f64
            %s3 = arith.addf %s0, %s1 : f64
            %sum = arith.addf %s2, %s3 : f64
            memref.store %sum, %B[%i, %j, %k] : memref<32x32x32xf64>
          }
        }
        omp.yield
      }
      omp.terminator
    }
    return
  }
}
