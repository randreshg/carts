// RUN: %carts-compile %s --O3 --arts-config %inputs_dir/arts_64t.cfg \
// RUN:   --start-from sde-planning --pipeline sde-planning \
// RUN:   --mlir-print-ir-after-all 2>&1 | %FileCheck %s

// SDE promotes generic affine-disjoint output dimensions, but leaves matmul to
// its specialized physical plan.

// CHECK-LABEL: // -----// IR Dump After PatternAnalysis
// CHECK-LABEL: func.func @nested_elementwise_3d
// CHECK: sde.su_iterate (%c0, %c0, %c0) to (%c8, %c16, %c32) step (%c1, %c1, %c1) classification(<elementwise>)

// CHECK-LABEL: func.func @pooling_like_local_reduction
// CHECK: sde.su_iterate (%c0, %c0, %c0, %c0) to (%c2, %c3, %c4, %c5) step (%c1, %c1, %c1, %c1) classification(<reduction>)
// CHECK: scf.for

// CHECK-LABEL: func.func @matmul_column_split
// CHECK: sde.su_iterate (%c0) to (%c8) step (%c1) classification(<matmul>)
// CHECK: scf.for

// CHECK-LABEL: // -----// IR Dump After DistributionPlanning
// CHECK-LABEL: func.func @matmul_column_split
// CHECK: sde.su_iterate
// CHECK: iterationTopology = #sde.iteration_topology<owner_strip>
// CHECK-SAME: physicalOwnerDims = [0]

module {
  func.func @nested_elementwise_3d(%A: memref<8x16x32xf64>,
                                  %B: memref<8x16x32xf64>) {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c8 = arith.constant 8 : index
    %c16 = arith.constant 16 : index
    %c32 = arith.constant 32 : index
    %one = arith.constant 1.000000e+00 : f64
    omp.parallel {
      omp.wsloop {
        omp.loop_nest (%i) : index = (%c0) to (%c8) step (%c1) {
          scf.for %j = %c0 to %c16 step %c1 {
            scf.for %k = %c0 to %c32 step %c1 {
              %v = memref.load %B[%i, %j, %k] : memref<8x16x32xf64>
              %sum = arith.addf %v, %one : f64
              memref.store %sum, %A[%i, %j, %k] : memref<8x16x32xf64>
            }
          }
          omp.yield
        }
      }
      omp.terminator
    }
    return
  }

  func.func @pooling_like_local_reduction(%I: memref<2x3x4x5xf32>,
                                          %O: memref<2x3x4x5xf32>) {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c2 = arith.constant 2 : index
    %c3 = arith.constant 3 : index
    %c4 = arith.constant 4 : index
    %c5 = arith.constant 5 : index
    %zero = arith.constant 0.000000e+00 : f32
    omp.parallel {
      omp.wsloop {
        omp.loop_nest (%n) : index = (%c0) to (%c2) step (%c1) {
          scf.for %c = %c0 to %c3 step %c1 {
            scf.for %h = %c0 to %c4 step %c1 {
              scf.for %w = %c0 to %c5 step %c1 {
                %acc = memref.alloca() : memref<f32>
                memref.store %zero, %acc[] : memref<f32>
                scf.for %r = %c0 to %c2 step %c1 {
                  %old = memref.load %acc[] : memref<f32>
                  %v = memref.load %I[%n, %c, %h, %w]
                    : memref<2x3x4x5xf32>
                  %next = arith.addf %old, %v : f32
                  memref.store %next, %acc[] : memref<f32>
                }
                %out = memref.load %acc[] : memref<f32>
                memref.store %out, %O[%n, %c, %h, %w]
                  : memref<2x3x4x5xf32>
              }
            }
          }
          omp.yield
        }
      }
      omp.terminator
    }
    return
  }

  func.func @matmul_column_split(%A: memref<8x4xf64>, %B: memref<4x16xf64>,
                                 %C: memref<8x16xf64>) {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index
    %c8 = arith.constant 8 : index
    %c16 = arith.constant 16 : index
    %zero = arith.constant 0.000000e+00 : f64
    omp.parallel {
      omp.wsloop {
        omp.loop_nest (%i) : index = (%c0) to (%c8) step (%c1) {
          scf.for %j = %c0 to %c16 step %c1 {
            %acc = memref.alloca() : memref<f64>
            memref.store %zero, %acc[] : memref<f64>
            scf.for %k = %c0 to %c4 step %c1 {
              %old = memref.load %acc[] : memref<f64>
              %a = memref.load %A[%i, %k] : memref<8x4xf64>
              %b = memref.load %B[%k, %j] : memref<4x16xf64>
              %prod = arith.mulf %a, %b : f64
              %next = arith.addf %old, %prod : f64
              memref.store %next, %acc[] : memref<f64>
            }
            %out = memref.load %acc[] : memref<f64>
            memref.store %out, %C[%i, %j] : memref<8x16xf64>
          }
          omp.yield
        }
      }
      omp.terminator
    }
    return
  }

}
