// RUN: %carts-compile %s --O3 --arts-config %arts_config --start-from=sde-planning --pipeline=sde-planning | %FileCheck %s

// Staged SDE planning, not a textual pass pipeline, must produce the production
// structural carriers before SDE-to-ARTS conversion.

// CHECK-LABEL: func.func @production_window_1d
// CHECK: sde.mu_alloc : memref<4x256xf32>
// CHECK: sde.cu_region <single>
// CHECK: sde.mu_access_window write %{{.*}} : memref<4x256xf32> owner_dims(1) block_lo [0] block_hi [4] valid [256]
// CHECK: memref.store %{{.*}}, %{{.*}}[%{{.*}}, %{{.*}}] : memref<4x256xf32>

func.func @production_window_1d() {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c1024 = arith.constant 1024 : index
  %cst = arith.constant 1.0 : f32
  %A = sde.mu_alloc : memref<1024xf32>
  sde.su_iterate (%c0) to (%c1024) step (%c1) classification(<elementwise>) {
  ^bb0(%i: index):
    sde.cu_region <single> {
      memref.store %cst, %A[%i] : memref<1024xf32>
      sde.yield
    }
  } {physicalOwnerDims = [0], physicalBlockShape = [256]}
  return
}
