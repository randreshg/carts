// RUN: %carts-compile %s --pass-pipeline="builtin.module(arts-scalar-replacement)" | %FileCheck %s

module {
  // CHECK-LABEL: func.func @single_access_reduction_promotes
  // CHECK: scf.for {{.*}} iter_args
  // CHECK-SAME: -> (f32)
  // CHECK: scf.yield {{.*}} : f32
  // CHECK: memref.store {{.*}} : memref<f32>
  func.func @single_access_reduction_promotes(%input: memref<32xf32>, %acc: memref<f32>) {
    %c0 = arith.constant 0 : index
    %c32 = arith.constant 32 : index
    %c1 = arith.constant 1 : index
    scf.for %i = %c0 to %c32 step %c1 {
      %old = memref.load %acc[] : memref<f32>
      %value = memref.load %input[%i] : memref<32xf32>
      %next = arith.addf %old, %value : f32
      memref.store %next, %acc[] : memref<f32>
    }
    return
  }

  // CHECK-LABEL: func.func @multi_read_accumulator_does_not_promote
  // CHECK-NOT: scf.for {{.*}} iter_args
  // CHECK: [[VISIBLE:%[0-9]+]] = memref.load %{{.*}}[] : memref<i64>
  // CHECK: memref.store [[VISIBLE]], %{{.*}}[%{{.*}}] : memref<32xi64>
  // CHECK: [[OLD:%[0-9]+]] = memref.load %{{.*}}[] : memref<i64>
  // CHECK: arith.addi [[OLD]], {{.*}} : i64
  // CHECK: memref.store {{.*}}, %{{.*}}[] : memref<i64>
  // CHECK: return
  func.func @multi_read_accumulator_does_not_promote(%out: memref<32xi64>, %idx: memref<i64>) {
    %c0 = arith.constant 0 : index
    %c32 = arith.constant 32 : index
    %c1 = arith.constant 1 : index
    %one_i64 = arith.constant 1 : i64
    scf.for %i = %c0 to %c32 step %c1 {
      %visible = memref.load %idx[] : memref<i64>
      memref.store %visible, %out[%i] : memref<32xi64>
      %old = memref.load %idx[] : memref<i64>
      %next = arith.addi %old, %one_i64 : i64
      memref.store %next, %idx[] : memref<i64>
    }
    return
  }
}
