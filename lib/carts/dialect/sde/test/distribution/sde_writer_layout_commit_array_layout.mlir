// RUN: %carts-compile %s --O3 --arts-config %arts_config --pipeline=sde-planning 2>&1 | %FileCheck %s

// CHECK-LABEL: func.func @writer_layout_commit_emits_array_layout
// CHECK: sde.array_layout write array_id({{[0-9]+}}) owner
// CHECK: sde.array_layout_root write
func.func @writer_layout_commit_emits_array_layout() {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c1024 = arith.constant 1024 : index
  %cst = arith.constant 1.0 : f32
  %A = sde.mu_alloc : memref<1024x1024xf32>
  sde.su_iterate (%c0, %c0) to (%c1024, %c1024) step (%c1, %c1)
      classification(<elementwise>) {
  ^bb0(%i: index, %j: index):
    sde.cu_region <parallel> {
      memref.store %cst, %A[%i, %j] : memref<1024x1024xf32>
      sde.yield
    }
    sde.yield
  }
  return
}
