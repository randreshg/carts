// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-cu-normalization,sde-scalar-block-reduction,raise-to-mu-access-window,verify-sde-mu-access-window,verify-sde-mu-layout,verify-sde)' 2>&1 | %FileCheck %s

// CHECK-LABEL: func.func @rank_expanded_source_checksum_partials
// CHECK: %[[PARTIAL:.*]] = sde.mu_alloc : memref<8x1x16x1xf64>
// CHECK: sde.su_iterate (%{{.*}}) to (%c8) step (%{{.*}}) classification(<elementwise>)
// CHECK: sde.array_layout_root write %[[PARTIAL]] : memref<8x1x16x1xf64> array_id(2)
// CHECK: sde.mu_access_window write %[[PARTIAL]] : memref<8x1x16x1xf64> array_id(2)
// CHECK: memref.store %{{.*}}, %[[PARTIAL]][%{{.*}}, %{{.*}}, %{{.*}}, %{{.*}}] : memref<8x1x16x1xf64>
// CHECK: arrayLayout = [{arrayId = 2 : i64, blockShape = [1, 16, 1], kind = "block_parallel"
// CHECK-SAME: ownerDims = [0]
// CHECK: sde.mu_access_window read %[[PARTIAL]] : memref<8x1x16x1xf64> array_id(2)
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
    sde.array_layout_root write %A : memref<8x16xf64> array_id(0)
    sde.cu_region <single> {
      %block = arith.divui %i, %c16 : index
      %tile = arith.remui %i, %c16 : index
      memref.store %one, %A[%block, %tile] : memref<8x16xf64>
      sde.yield
    } {serialReason = #sde.serial_reason<residual_source>}
  } {arrayLayout = [{arrayId = 0 : i64, kind = "block_parallel", ownerDims = [0], blockShape = [16], muBlockCount = 8 : i64, role = "write", commVolumeBytes = 0 : i64}]}
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

// CHECK-LABEL: func.func @rank_expanded_nonleading_owner_checksum_partials
// CHECK: %[[PARTIAL:.*]] = sde.mu_alloc : memref<4x1x2x1xf64>
// CHECK: sde.su_iterate (%{{.*}}) to (%{{.*}}) step (%{{.*}}) classification(<elementwise>)
// CHECK: memref.load %{{.*}}[%{{.*}}, %{{.*}}, %{{.*}}, %{{.*}}] : memref<4x8x2x16xf64>
// CHECK: memref.store %{{.*}}, %[[PARTIAL]][%{{.*}}, %{{.*}}, %{{.*}}, %{{.*}}] : memref<4x1x2x1xf64>
// CHECK: arrayLayout = [{arrayId = 3 : i64, blockShape = [1, 2, 1], kind = "block_parallel"
// CHECK-SAME: ownerDims = [0]
// CHECK: sde.mu_access_window read %[[PARTIAL]] : memref<4x1x2x1xf64> array_id(3)

func.func @rank_expanded_nonleading_owner_checksum_partials() -> f64 {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c2 = arith.constant 2 : index
  %c4 = arith.constant 4 : index
  %c8 = arith.constant 8 : index
  %c16 = arith.constant 16 : index
  %zero = arith.constant 0.0 : f64
  %one = arith.constant 1.0 : f64
  %A = sde.mu_alloc : memref<4x8x2x16xf64>
  %sum = memref.alloca() : memref<f64>
  memref.store %zero, %sum[] : memref<f64>
  sde.su_iterate (%c0, %c0, %c0) to (%c8, %c8, %c16) step (%c8, %c2, %c16) classification(<elementwise>) {
  ^bb0(%i: index, %j: index, %k: index):
    sde.array_layout_root write %A : memref<4x8x2x16xf64> array_id(1)
    sde.cu_region <single> {
      %block = arith.divui %j, %c2 : index
      %tile = arith.remui %j, %c2 : index
      memref.store %one, %A[%block, %i, %tile, %k] : memref<4x8x2x16xf64>
      sde.yield
    } {serialReason = #sde.serial_reason<residual_source>}
  } {arrayLayout = [{arrayId = 1 : i64, kind = "block_parallel", ownerDims = [1], blockShape = [8, 2, 16], muBlockCount = 4 : i64, role = "write", commVolumeBytes = 0 : i64}]}
  sde.cu_region <single> {
    scf.for %i = %c0 to %c8 step %c1 {
      %old = memref.load %sum[] : memref<f64>
      %block = arith.divui %i, %c2 : index
      %tile = arith.remui %i, %c2 : index
      %v = memref.load %A[%block, %i, %tile, %i] : memref<4x8x2x16xf64>
      %next = arith.addf %old, %v : f64
      memref.store %next, %sum[] : memref<f64>
    }
    sde.yield
  }
  %result = memref.load %sum[] : memref<f64>
  return %result : f64
}
