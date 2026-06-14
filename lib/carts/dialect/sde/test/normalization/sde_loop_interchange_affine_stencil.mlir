// RUN: %carts-compile %s --pass-pipeline='builtin.module(loop-interchange)' 2>&1 | %FileCheck %s

// Stencil halo interchange on affine.for nests (S4): wider-halo dim 1 is swapped
// behind dim 2 via upstream permuteLoops without a LowerAffine bridge.

// CHECK-LABEL: func.func @affine_stencil_halo_interchange
// CHECK: classification(<stencil>)
// CHECK: affine.for %[[K:.*]] =
// CHECK: affine.for %[[J:.*]] =
// CHECK: memref.store {{.*}}, %{{.*}}[{{.*}}, %[[J]], %[[K]]]
// CHECK-NOT: scf.for

func.func @affine_stencil_halo_interchange() {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c8 = arith.constant 8 : index
  %v = arith.constant 0.0 : f32
  %A = sde.mu_alloc : memref<8x4x4xf32>

  sde.su_iterate (%c0) to (%c8) step (%c1) classification(<stencil>) {
  ^bb0(%i: index):
    sde.array_layout_root write %A : memref<8x4x4xf32> array_id(0)
    sde.cu_region <single> {
      affine.for %j = 0 to 4 {
        affine.for %k = 0 to 4 {
          memref.store %v, %A[%i, %j, %k] : memref<8x4x4xf32>
        }
      }
      sde.yield
    }
    sde.yield
  } {accessMinOffsets = [0, -2, -1], accessMaxOffsets = [0, 2, 1]}
  return
}
