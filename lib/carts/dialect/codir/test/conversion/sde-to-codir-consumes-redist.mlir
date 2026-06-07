// RUN: %carts-compile %s --pass-pipeline='builtin.module(convert-sde-to-codir,verify-codir)' 2>&1 | %FileCheck %s --implicit-check-not=sde.redist

// CODIR consumes the committed sde.redist movement: the reduce_scatter_like
// geometric family becomes a reduce_scatter collective on the reader codelet's
// dependency edge, and the redistribution carrier is erased at the boundary.
// CODIR maps the committed family mechanically; it does not classify movement.

// CHECK-LABEL: func.func @consume_redist
// CHECK: codir.codelet deps(%{{.*}}, %{{.*}} : memref<256xf32>, memref<256x256xf32>)
// CHECK-SAME: dep_collectives = [#codir.collective<none>, #codir.collective<reduce_scatter>]

func.func @consume_redist(%A: memref<256x256xf32>, %R: memref<256xf32>) {
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
      %s = arith.addf %acc, %a : f32
      memref.store %s, %R[%i] : memref<256xf32>
      sde.yield
    } {arrayLayout = [{arrayId = 0 : i64, blockShape = [128, 256], commVolumeBytes = 2097152 : i64, kind = "block_contraction", muBlockCount = 2 : i64, ownerDims = [0], role = "read"}, {arrayId = 1 : i64, blockShape = [256], commVolumeBytes = 0 : i64, kind = "block", muBlockCount = 1 : i64, ownerDims = [0], role = "readwrite"}], layoutsDisagree = [0]}
    sde.yield
  }
  return
}
