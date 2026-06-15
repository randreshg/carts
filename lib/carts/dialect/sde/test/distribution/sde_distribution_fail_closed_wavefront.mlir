// RUN: not %carts-compile %s --O3 --arts-config %inputs_dir/arts_64t.cfg --pipeline=sde-planning 2>&1 | %FileCheck %s

// Dedicated coverage for the A7 sde-distribution-fail-closed gate. A diagonal
// in-place self-read neighborhood stencil (Gauss-Seidel family) with
// loop-carried neighbor offsets on both spatial dims cannot be distributed
// across multiple workers without an SDE wavefront/skew transform, which is not
// implemented. The distribution chain must fail closed with a citing diagnostic
// instead of stamping halo/distribution facts that downstream layers could
// miscompile. (Cost-model passes cannot be invoked standalone via
// builtin.module(...), so this exercises the gate through sde-planning.)

// CHECK: in-place self-read stencil (Gauss-Seidel family) has loop-carried neighbor offsets
// CHECK-SAME: exposing legal parallelism requires an SDE wavefront/skew
// CHECK-SAME: preserving the order is serial

func.func @fail_closed_diagonal_in_place_self_read() {
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
      %jm1 = arith.subi %j, %c1 : index
      %diag = memref.load %A[%im1, %jm1] : memref<64x64xf32>
      memref.store %diag, %A[%i, %j] : memref<64x64xf32>
      sde.yield
    }
    sde.yield
  } {accessMinOffsets = [-1, -1], accessMaxOffsets = [0, 0],
     ownerDims = [0, 1], spatialDims = [0, 1], writeFootprint = [0, 0],
     inPlaceSharedState}
  return
}
