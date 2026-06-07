// RUN: not %carts-compile %s --pass-pipeline='builtin.module(sde-redistribute)' 2>&1 | %FileCheck %s

// A disagreement edge whose consumer access is a transpose (a parallel read on a
// non-home axis), not a cross-owner reduction, is not representable from the
// committed facts: `sde-layout-assignment` discards the consumer's required
// target layout, so emitting a redistribution would mean inventing it. The pass
// fails closed instead.

// CHECK: error: {{.*}}not a cross-owner reduction

func.func @diagnose_unrepresentable(%T: memref<256x256xf32>,
                                    %G: memref<256x256xf32>) {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c256 = arith.constant 256 : index
  %cst = arith.constant 0.0 : f32
  sde.cu_region <parallel> {
    sde.su_iterate (%c0, %c0) to (%c256, %c256) step (%c1, %c1) {
    ^bb0(%i: index, %j: index):
      memref.store %cst, %T[%i, %j] : memref<256x256xf32>
      sde.yield
    } {arrayLayout = [{arrayId = 0 : i64, kind = "block_parallel", ownerDims = [0], blockShape = [128, 256], muBlockCount = 2 : i64, role = "write", commVolumeBytes = 0 : i64}]}
    // Transposed parallel read of %T (owner axis disagrees), not a reduction.
    sde.su_iterate (%c0, %c0) to (%c256, %c256) step (%c1, %c1) {
    ^bb0(%i: index, %j: index):
      %t = memref.load %T[%j, %i] : memref<256x256xf32>
      memref.store %t, %G[%i, %j] : memref<256x256xf32>
      sde.yield
    } {arrayLayout = [{arrayId = 0 : i64, kind = "block_parallel", ownerDims = [0], blockShape = [128, 256], muBlockCount = 2 : i64, role = "read", commVolumeBytes = 2097152 : i64}, {arrayId = 1 : i64, kind = "block_parallel", ownerDims = [0], blockShape = [128, 256], muBlockCount = 2 : i64, role = "write", commVolumeBytes = 0 : i64}], layoutsDisagree = [0]}
    sde.yield
  }
  return
}
