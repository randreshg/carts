// RUN: not %carts-compile %s --pass-pipeline='builtin.module(sde-distribution-fail-closed)' 2>&1 | %FileCheck %s

// Multi-worker in-place Gauss-Seidel stencils have loop-carried self-RAW
// dependences. Until SDE emits a real wavefront/skew transform, planning must
// fail closed instead of stamping halo/distribution facts.

// CHECK: in-place self-read stencil (Gauss-Seidel family) has loop-carried neighbor offsets
// CHECK-SAME: exposing legal parallelism requires an SDE wavefront/skew
// CHECK-SAME: preserving the order is serial

module attributes {carts.logical_total_workers = 64 : i64} {
func.func @seidel_in_place_wavefront_rejected() {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c64 = arith.constant 64 : index
  %A = sde.mu_alloc : memref<64x64xf32>
  sde.su_iterate (%c0, %c0) to (%c64, %c64) step (%c1, %c1)
      classification(<stencil>) {
  ^bb0(%i: index, %j: index):
    sde.array_layout_root read %A : memref<64x64xf32> array_id(0)
    sde.array_layout_root write %A : memref<64x64xf32> array_id(0)
    sde.cu_region <parallel> {
      %im1 = arith.subi %i, %c1 : index
      %left = memref.load %A[%im1, %j] : memref<64x64xf32>
      memref.store %left, %A[%i, %j] : memref<64x64xf32>
      sde.yield
    }
    sde.yield
  } {accessMinOffsets = [-1, 0], accessMaxOffsets = [0, 0],
     ownerDims = [0, 1], spatialDims = [0, 1], writeFootprint = [0, 0],
     inPlaceSharedState}
  return
}
}
