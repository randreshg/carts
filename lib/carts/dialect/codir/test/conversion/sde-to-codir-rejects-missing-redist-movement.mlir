// RUN: not %carts-compile %s --pass-pipeline='builtin.module(convert-sde-to-codir)' 2>&1 | %FileCheck %s

// When SDE structure is being consumed (an sde.redist is present), a recorded
// layout disagreement for an array root with no committed sde.redist movement
// is a boundary error: CODIR refuses to classify or invent the movement. Here
// array 0 has a committed redistribution but array 1 does not.

// CHECK: error: 'codir.codelet' op consumes committed SDE structure but array 1 has a recorded layout disagreement with no sde.redist movement

func.func @reject_missing_movement(%A: memref<256x256xf32>, %B: memref<256x256xf32>, %R: memref<256xf32>) {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c256 = arith.constant 256 : index
  %cst = arith.constant 0.0 : f32
  sde.cu_region <parallel> {
    sde.su_iterate (%c0, %c0) to (%c256, %c256) step (%c1, %c1) {
    ^bb0(%i: index, %j: index):
      memref.store %cst, %A[%i, %j] : memref<256x256xf32>
      sde.yield
    } {arrayLayout = [{arrayId = 0 : i64, blockShape = [128, 256], commVolumeBytes = 0 : i64, kind = "block_contraction", muBlockCount = 2 : i64, ownerDims = [0], role = "write"}]}
    sde.redist <reduce_scatter_like> %A : memref<256x256xf32> from owner [0] block [128, 256] to owner [0] block [128, 256] cost 2097152
    sde.su_iterate (%c0) to (%c256) step (%c1) {
    ^bb0(%i: index):
      %acc = memref.load %R[%i] : memref<256xf32>
      %a = memref.load %A[%i, %i] : memref<256x256xf32>
      %b = memref.load %B[%i, %i] : memref<256x256xf32>
      %p = arith.mulf %a, %b : f32
      %s = arith.addf %acc, %p : f32
      memref.store %s, %R[%i] : memref<256xf32>
      sde.yield
    } {arrayLayout = [{arrayId = 0 : i64, blockShape = [128, 256], commVolumeBytes = 2097152 : i64, kind = "block_contraction", muBlockCount = 2 : i64, ownerDims = [0], role = "read"}, {arrayId = 1 : i64, blockShape = [128, 256], commVolumeBytes = 2097152 : i64, kind = "block_contraction", muBlockCount = 2 : i64, ownerDims = [0], role = "read"}, {arrayId = 2 : i64, blockShape = [256], commVolumeBytes = 0 : i64, kind = "block", muBlockCount = 1 : i64, ownerDims = [0], role = "readwrite"}], layoutsDisagree = [0, 1]}
    sde.yield
  }
  return
}
