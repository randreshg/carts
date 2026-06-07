// RUN: not %carts-compile %s --pass-pipeline='builtin.module(sde-redistribute)' 2>&1 | %FileCheck %s

// A redistribution edge on a pure input (read-only, no committed writer home)
// has no source layout to redistribute from; the pass fails closed.

// CHECK: error: {{.*}}no committed writer layout to redistribute from

func.func @diagnose_no_writer(%T: memref<256x256xf32>, %E: memref<256x256xf32>,
                              %G: memref<256x256xf32>) {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c256 = arith.constant 256 : index
  sde.cu_region <parallel> {
    // %T is read on its contraction axis but is never written: no home. It is
    // loaded first so it is arrayId 0 (the disagree edge target).
    sde.su_iterate (%c0, %c0) to (%c256, %c256) step (%c1, %c1) {
    ^bb0(%i: index, %j: index):
      scf.for %k = %c0 to %c256 step %c1 {
        %t = memref.load %T[%k, %j] : memref<256x256xf32>
        %e = memref.load %E[%i, %k] : memref<256x256xf32>
        %g = memref.load %G[%i, %j] : memref<256x256xf32>
        %p = arith.mulf %e, %t : f32
        %s = arith.addf %g, %p : f32
        memref.store %s, %G[%i, %j] : memref<256x256xf32>
      }
      sde.yield
    } {arrayLayout = [{arrayId = 0 : i64, kind = "block_contraction", ownerDims = [0], blockShape = [128, 256], muBlockCount = 2 : i64, role = "read", commVolumeBytes = 2097152 : i64}], layoutsDisagree = [0]}
    sde.yield
  }
  return
}
