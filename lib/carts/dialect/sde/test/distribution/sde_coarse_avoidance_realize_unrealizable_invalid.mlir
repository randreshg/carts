// RUN: not %carts-compile %s --pass-pipeline='builtin.module(sde-coarse-avoidance)' 2>&1 | %FileCheck %s

// Hard fail-closed: a committed single-owner block plan whose MU root has an
// aliasing (memref.cast) use cannot be realized end to end. The pass attempts the
// realize (the committed plan is in scope) and the rewriter fails, so it
// emitOpErrors rather than leave a partial owner-dim promise — never papered over.

// CHECK: error: {{.*}}committed block-grid layout cannot be realized

func.func @realize_unrealizable() {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c1024 = arith.constant 1024 : index
  %cst = arith.constant 1.0 : f32
  %A = sde.mu_alloc : memref<1024xf32>
  %alias = memref.cast %A : memref<1024xf32> to memref<?xf32>
  sde.cu_region <parallel> {
    sde.su_iterate (%c0) to (%c1024) step (%c1) classification(<elementwise>) {
    ^bb0(%i: index):
      memref.store %cst, %A[%i] : memref<1024xf32>
      sde.yield
    } {physicalOwnerDims = [0], physicalBlockShape = [256]}
    sde.yield
  }
  return
}
