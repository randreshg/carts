// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-cu-normalization,sde-scalar-block-reduction,raise-to-mu-access-window,verify-sde-mu-access-window,verify-sde-mu-layout,verify-sde)' 2>&1 | %FileCheck %s

// CHECK-LABEL: func.func @rank_expanded_source_checksum_partials
// CHECK: %[[PARTIAL:.*]] = sde.mu_alloc {{.*}} : memref<8x1x16x1xf64>
// CHECK: sde.su_iterate (%{{.*}}) to (%c8) step (%{{.*}}) classification(<elementwise>)
// CHECK: sde.array_layout_root write %[[PARTIAL]] : memref<8x1x16x1xf64>
// CHECK: sde.mu_access_window write %[[PARTIAL]] : memref<8x1x16x1xf64>
// CHECK: memref.store %{{.*}}, %[[PARTIAL]][%{{.*}}, %{{.*}}, %{{.*}}, %{{.*}}] : memref<8x1x16x1xf64>
// CHECK: physicalBlockShape = [1, 16, 1]
// CHECK-SAME: physicalOwnerDims = [0]
// CHECK: sde.mu_access_window read %[[PARTIAL]] : memref<8x1x16x1xf64>
// CHECK: memref.load %[[PARTIAL]][%{{.*}}, %{{.*}}, %{{.*}}, %{{.*}}] : memref<8x1x16x1xf64>

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
