// RUN: %carts-compile %s --O3 --arts-config %arts_config --pipeline=sde-planning 2>&1 \
// RUN:   | %FileCheck %s --implicit-check-not="sde.mu_alloc : memref<f64>"

// CHECK-LABEL: func.func @private_scratch_transposed_writer
// CHECK: sde.su_iterate (%{{[^)]*}}, %{{[^)]*}}) to (%{{[^)]*}}, %{{[^)]*}}) step
// CHECK-SAME: classification(<elementwise>)
// CHECK: memref.alloca() : memref<f64>
// CHECK: %{{.*}} = arith.index_cast %[[J_IV:arg[0-9]+]] : index to i64
// CHECK: %{{.*}} = arith.index_cast %[[I_IV:arg[0-9]+]] : index to i64
// CHECK: memref.store %{{.*}}, %arg0[%[[I_IV]], %[[J_IV]]] : memref
// CHECK: } {arrayLayout =
// CHECK-SAME: physicalBlockShape =
// CHECK-SAME: physicalOwnerDims = [
// CHECK: sde.su_iterate
// CHECK-SAME: classification(<stencil>)

module {
  func.func @private_scratch_transposed_writer(%rhs: memref<2048x2048xf64>, %u: memref<2048x2048xf64>, %out: memref<2048x2048xf64>) {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c2048 = arith.constant 2048 : index
    %zero = arith.constant 0.000000e+00 : f64
    %one = arith.constant 1.000000e+00 : f64
    %undef = llvm.mlir.undef : f64
    omp.parallel {
      %x = memref.alloca() : memref<f64>
      memref.store %undef, %x[] : memref<f64>
      %y = memref.alloca() : memref<f64>
      memref.store %undef, %y[] : memref<f64>
      omp.wsloop schedule(static) {
        omp.loop_nest (%j) : index = (%c0) to (%c2048) step (%c1) {
          %jf = arith.index_cast %j : index to i64
          %jv = arith.sitofp %jf : i64 to f64
          memref.store %jv, %y[] : memref<f64>
          scf.for %i = %c0 to %c2048 step %c1 {
            %if = arith.index_cast %i : index to i64
            %iv = arith.sitofp %if : i64 to f64
            memref.store %iv, %x[] : memref<f64>
            %vx = memref.load %x[] : memref<f64>
            %vy = memref.load %y[] : memref<f64>
            %sum = arith.addf %vx, %vy : f64
            memref.store %sum, %rhs[%i, %j] : memref<2048x2048xf64>
          }
          omp.yield
        }
      }
      omp.wsloop schedule(static) {
        omp.loop_nest (%i) : index = (%c1) to (%c2048) step (%c1) {
          scf.for %j = %c1 to %c2048 step %c1 {
            %im = arith.subi %i, %c1 : index
            %ip = arith.addi %i, %c1 : index
            %left = memref.load %u[%im, %j] : memref<2048x2048xf64>
            %right = memref.load %u[%ip, %j] : memref<2048x2048xf64>
            %force = memref.load %rhs[%i, %j] : memref<2048x2048xf64>
            %sum0 = arith.addf %left, %right : f64
            %sum1 = arith.addf %sum0, %force : f64
            %val = arith.mulf %sum1, %one : f64
            memref.store %val, %out[%i, %j] : memref<2048x2048xf64>
          }
          omp.yield
        }
      }
      omp.terminator
    }
    memref.store %zero, %out[%c0, %c0] : memref<2048x2048xf64>
    return
  }
}
