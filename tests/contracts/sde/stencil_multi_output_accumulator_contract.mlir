// RUN: %carts-compile %s --O3 --arts-config %S/../../examples/arts.cfg --pipeline pattern-pipeline | %FileCheck %s --check-prefix=PATTERN
// RUN: %carts-compile %s --O3 --arts-config %S/../../examples/arts.cfg --pipeline edt-distribution | %FileCheck %s --check-prefix=POLICY

// PATTERN: depPattern = #arts.dep_pattern<stencil_tiling_nd>
// PATTERN: distribution_pattern = #arts.distribution_pattern<stencil>

// POLICY-DAG: arts.db_acquire[<in>] {{.*}}partitioning(<coarse>{{.*}}depPattern = #arts.dep_pattern<stencil_tiling_nd>{{.*}}distribution_pattern = #arts.distribution_pattern<stencil>
// POLICY-DAG: arts.db_acquire[<inout>] {{.*}}partitioning(<coarse>{{.*}}depPattern = #arts.dep_pattern<stencil_tiling_nd>{{.*}}distribution_pattern = #arts.distribution_pattern<stencil>
// POLICY-DAG: arts.epoch attributes {{.*}}depPattern = #arts.dep_pattern<stencil_tiling_nd>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<stencil>
// POLICY-DAG: arts.edt <task> <intranode> route(%{{.*}}) {{.*}}attributes {{.*}}depPattern = #arts.dep_pattern<stencil_tiling_nd>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<stencil>

module attributes {llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu"} {
  func.func @main() -> f64 {
    %c1 = arith.constant 1 : index
    %c31 = arith.constant 31 : index
    %A = memref.alloc() : memref<32x32x32xf64>
    %B = memref.alloc() : memref<32x32x32xf64>
    %Out0 = memref.alloc() : memref<32x32x32xf64>
    %Out1 = memref.alloc() : memref<32x32x32xf64>
    omp.parallel {
      omp.wsloop {
        omp.loop_nest (%i) : index = (%c1) to (%c31) step (%c1) {
        scf.for %j = %c1 to %c31 step %c1 {
          scf.for %k = %c1 to %c31 step %c1 {
            %im1 = arith.subi %i, %c1 : index
            %ip1 = arith.addi %i, %c1 : index
            %jm1 = arith.subi %j, %c1 : index
            %jp1 = arith.addi %j, %c1 : index
            %km1 = arith.subi %k, %c1 : index
            %kp1 = arith.addi %k, %c1 : index

            %a0 = memref.load %A[%im1, %j, %k] : memref<32x32x32xf64>
            %a1 = memref.load %A[%ip1, %j, %k] : memref<32x32x32xf64>
            %b0 = memref.load %B[%i, %jm1, %k] : memref<32x32x32xf64>
            %b1 = memref.load %B[%i, %jp1, %k] : memref<32x32x32xf64>
            %b2 = memref.load %B[%i, %j, %km1] : memref<32x32x32xf64>
            %b3 = memref.load %B[%i, %j, %kp1] : memref<32x32x32xf64>

            %s0 = arith.addf %a0, %a1 : f64
            %s1 = arith.addf %b0, %b1 : f64
            %s2 = arith.addf %b2, %b3 : f64
            %s3 = arith.addf %s0, %s1 : f64
            %s4 = arith.addf %s2, %s3 : f64
            memref.store %s4, %Out0[%i, %j, %k] : memref<32x32x32xf64>
            %t0 = arith.addf %a0, %b0 : f64
            %t1 = arith.addf %a1, %b3 : f64
            %t2 = arith.addf %t0, %t1 : f64
            memref.store %t2, %Out1[%i, %j, %k] : memref<32x32x32xf64>
          }
        }
        omp.yield
      }
      }
      omp.terminator
    }
    omp.parallel {
      omp.wsloop {
        omp.loop_nest (%i2) : index = (%c1) to (%c31) step (%c1) {
        scf.for %j2 = %c1 to %c31 step %c1 {
          scf.for %k2 = %c1 to %c31 step %c1 {
            %v = memref.load %Out0[%i2, %j2, %k2] : memref<32x32x32xf64>
            memref.store %v, %Out1[%i2, %j2, %k2] : memref<32x32x32xf64>
          }
        }
        omp.yield
      }
      }
      omp.terminator
    }
    %result = memref.load %Out1[%c1, %c1, %c1] : memref<32x32x32xf64>
    memref.dealloc %Out1 : memref<32x32x32xf64>
    memref.dealloc %Out0 : memref<32x32x32xf64>
    memref.dealloc %B : memref<32x32x32xf64>
    memref.dealloc %A : memref<32x32x32xf64>
    return %result : f64
  }
}
