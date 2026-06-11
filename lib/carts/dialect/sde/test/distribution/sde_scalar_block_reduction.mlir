// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-cu-normalization,sde-scalar-block-reduction,verify-sde)' 2>&1 | %FileCheck %s
// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-cu-normalization,sde-scalar-block-reduction,verify-sde-mu-layout,verify-sde)' 2>&1 | %FileCheck %s --check-prefix=EXP

// SDE owns this rewrite: a legal scalar checksum loop becomes real owner-block
// partial work before SDE/ARTS. The FP case is order-preserving because the
// sample stride is at least the committed block extent, so each block
// contributes at most one element to the final scalar combine.

// CHECK-LABEL: func.func @fp_stride_checksum_block_partials
// CHECK: memref.alloc() : memref<64xf64>
// CHECK: sde.su_iterate (%{{.*}}) to (%c64) step (%{{.*}}) classification(<elementwise>)
// CHECK: sde.cu_region <parallel>
// CHECK: arith.remui
// CHECK: arith.select
// CHECK: scf.for %{{.*}} = %{{.*}} to %{{.*}} step %c128
// CHECK: memref.store %{{.*}}, %{{.*}}[%{{.*}}] : memref<64xf64>
// CHECK: iterationTopology = #sde.iteration_topology<owner_strip>
// CHECK-SAME: logicalWorkerSlice = [1]
// CHECK-SAME: physicalBlockShape = [1]
// CHECK-SAME: physicalOwnerDims = [0]
// CHECK: scf.for %{{.*}} = %{{.*}} to %{{.*}} step %{{.*}}
// CHECK: memref.load %{{.*}}[%{{.*}}] : memref<64xf64>

func.func @fp_stride_checksum_block_partials(%A: memref<1024xf64>) -> f64 {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c16 = arith.constant 16 : index
  %c128 = arith.constant 128 : index
  %c1024 = arith.constant 1024 : index
  %zero = arith.constant 0.0 : f64
  %one = arith.constant 1.0 : f64
  %sum = memref.alloca() : memref<f64>
  memref.store %zero, %sum[] : memref<f64>
  sde.su_iterate (%c0) to (%c1024) step (%c1) classification(<elementwise>) {
  ^bb0(%i: index):
    sde.cu_region <single> {
      memref.store %one, %A[%i] : memref<1024xf64>
      sde.yield
    }
  } {physicalOwnerDims = [0], physicalBlockShape = [16]}
  sde.cu_region <single> {
    scf.for %i = %c0 to %c1024 step %c128 {
      %old = memref.load %sum[] : memref<f64>
      %v = memref.load %A[%i] : memref<1024xf64>
      %next = arith.addf %old, %v : f64
      memref.store %next, %sum[] : memref<f64>
    }
    sde.yield
  }
  %result = memref.load %sum[] : memref<f64>
  return %result : f64
}

// CHECK-LABEL: func.func @strict_fp_dense_checksum_ordered_partials
// CHECK: memref.alloc() : memref<8x16x1xf64>
// CHECK: sde.su_iterate (%{{.*}}) to (%c8) step (%{{.*}}) classification(<elementwise>)
// CHECK: scf.for %{{.*}} = %{{.*}} to %{{.*}} step %{{.*}}
// CHECK: memref.store %{{.*}}, %{{.*}}[%{{.*}}, %{{.*}}, %{{.*}}] : memref<8x16x1xf64>
// CHECK: scf.for %{{.*}} = %{{.*}} to %{{.*}} step %{{.*}}
// CHECK: scf.for %{{.*}} = %{{.*}} to %{{.*}} step %{{.*}}

func.func @strict_fp_dense_checksum_ordered_partials(%A: memref<128xf64>) -> f64 {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c16 = arith.constant 16 : index
  %c128 = arith.constant 128 : index
  %zero = arith.constant 0.0 : f64
  %one = arith.constant 1.0 : f64
  %sum = memref.alloca() : memref<f64>
  memref.store %zero, %sum[] : memref<f64>
  sde.su_iterate (%c0) to (%c128) step (%c1) classification(<elementwise>) {
  ^bb0(%i: index):
    sde.cu_region <single> {
      memref.store %one, %A[%i] : memref<128xf64>
      sde.yield
    }
  } {physicalOwnerDims = [0], physicalBlockShape = [16]}
  sde.cu_region <single> {
    scf.for %i = %c0 to %c128 step %c1 {
      %old = memref.load %sum[] : memref<f64>
      %v = memref.load %A[%i] : memref<128xf64>
      %next = arith.addf %old, %v : f64
      memref.store %next, %sum[] : memref<f64>
    }
    sde.yield
  }
  %result = memref.load %sum[] : memref<f64>
  return %result : f64
}

// CHECK-LABEL: func.func @integer_dense_checksum_block_partials
// CHECK: memref.alloc() : memref<8xi32>
// CHECK: sde.su_iterate (%{{.*}}) to (%c8) step (%{{.*}}) classification(<elementwise>)
// CHECK: scf.for %{{.*}} = %{{.*}} to %{{.*}} step %c1
// CHECK: scf.for %{{.*}} = %{{.*}} to %{{.*}} step %{{.*}}

func.func @integer_dense_checksum_block_partials(%A: memref<128xi32>) -> i32 {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c16 = arith.constant 16 : index
  %c128 = arith.constant 128 : index
  %zero = arith.constant 0 : i32
  %one = arith.constant 1 : i32
  %sum = memref.alloca() : memref<i32>
  memref.store %zero, %sum[] : memref<i32>
  sde.su_iterate (%c0) to (%c128) step (%c1) classification(<elementwise>) {
  ^bb0(%i: index):
    sde.cu_region <single> {
      memref.store %one, %A[%i] : memref<128xi32>
      sde.yield
    }
  } {physicalOwnerDims = [0], physicalBlockShape = [16]}
  sde.cu_region <single> {
    scf.for %i = %c0 to %c128 step %c1 {
      %old = memref.load %sum[] : memref<i32>
      %v = memref.load %A[%i] : memref<128xi32>
      %next = arith.addi %old, %v : i32
      memref.store %next, %sum[] : memref<i32>
    }
    sde.yield
  }
  %result = memref.load %sum[] : memref<i32>
  return %result : i32
}

// EXP-LABEL: func.func @rank_expanded_source_checksum_partials
// EXP: %[[PARTIAL:.*]] = sde.mu_alloc : memref<8x1x16x1xf64>
// EXP: sde.su_iterate (%{{.*}}) to (%c8) step (%{{.*}}) classification(<elementwise>)
// EXP: memref.store %{{.*}}, %[[PARTIAL]][%{{.*}}, %{{.*}}, %{{.*}}, %{{.*}}] : memref<8x1x16x1xf64>
// EXP: physicalBlockShape = [1, 16, 1]
// EXP-SAME: physicalOwnerDims = [0]
// EXP: memref.load %[[PARTIAL]][%{{.*}}, %{{.*}}, %{{.*}}, %{{.*}}] : memref<8x1x16x1xf64>

func.func @rank_expanded_source_checksum_partials() -> f64 {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c16 = arith.constant 16 : index
  %c128 = arith.constant 128 : index
  %zero = arith.constant 0.0 : f64
  %one = arith.constant 1.0 : f64
  %A = sde.mu_alloc : memref<8x16xf64>
  %sum = memref.alloca() : memref<f64>
  memref.store %zero, %sum[] : memref<f64>
  sde.su_iterate (%c0) to (%c128) step (%c1) classification(<elementwise>) {
  ^bb0(%i: index):
    sde.cu_region <single> {
      %block = arith.divui %i, %c16 : index
      %tile = arith.remui %i, %c16 : index
      memref.store %one, %A[%block, %tile] : memref<8x16xf64>
      sde.yield
    }
  } {physicalOwnerDims = [0], physicalBlockShape = [16]}
  sde.cu_region <single> {
    scf.for %i = %c0 to %c128 step %c1 {
      %old = memref.load %sum[] : memref<f64>
      %block = arith.divui %i, %c16 : index
      %tile = arith.remui %i, %c16 : index
      %v = memref.load %A[%block, %tile] : memref<8x16xf64>
      %next = arith.addf %old, %v : f64
      memref.store %next, %sum[] : memref<f64>
    }
    sde.yield
  }
  %result = memref.load %sum[] : memref<f64>
  return %result : f64
}
