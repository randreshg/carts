// RUN: not %carts-compile %s --pass-pipeline='builtin.module(verify-sde-redistribute)' 2>&1 | %FileCheck %s

// COMPLETENESS gate: a committed cross-owner reduction edge with no sde.redist
// (verify run WITHOUT sde-redistribute first) is rejected — the verifier
// independently demands the redistribution structure rather than trusting that
// the pass ran.

// CHECK: error: {{.*}}is not represented as sde.redist

func.func @unrepresented(%T: memref<256x256xf32>, %E: memref<256x256xf32>,
                         %G: memref<256x256xf32>) {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c256 = arith.constant 256 : index
  %cst = arith.constant 0.0 : f32
  sde.su_iterate (%c0, %c0) to (%c256, %c256) step (%c1, %c1) {
  ^bb0(%i: index, %j: index):
    sde.array_layout_root write %T : memref<256x256xf32> array_id(0)
    sde.cu_region <single> {
      memref.store %cst, %T[%i, %j] : memref<256x256xf32>
      sde.yield
    }
  } {arrayLayout = [{arrayId = 0 : i64, kind = "block_contraction", ownerDims = [0], blockShape = [128, 256], muBlockCount = 2 : i64, role = "write", commVolumeBytes = 0 : i64}]}
  sde.su_iterate (%c0, %c0) to (%c256, %c256) step (%c1, %c1) {
  ^bb0(%i: index, %j: index):
    sde.array_layout_root read %T : memref<256x256xf32> array_id(0)
    sde.cu_region <single> {
      scf.for %k = %c0 to %c256 step %c1 {
        %e = memref.load %E[%i, %k] : memref<256x256xf32>
        %t = memref.load %T[%k, %j] : memref<256x256xf32>
        %g = memref.load %G[%i, %j] : memref<256x256xf32>
        %p = arith.mulf %e, %t : f32
        %s = arith.addf %g, %p : f32
        memref.store %s, %G[%i, %j] : memref<256x256xf32>
    }
      sde.yield
    }
  } {arrayLayout = [{arrayId = 0 : i64, kind = "block_contraction", ownerDims = [0], blockShape = [128, 256], muBlockCount = 2 : i64, role = "read", commVolumeBytes = 2097152 : i64}], layoutsDisagree = [0]}
  return
}
