// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-rank-expand-mu,convert-sde-to-codir,verify-codir)' 2>&1 | %FileCheck %s

// Block-localized reads must join committed facts by logical size when SSA
// identity is unavailable. Dependency order is (out, small, big), while the
// committed read facts are ordered (big, small).

// CHECK-LABEL: func.func @dep_ids_by_size
// CHECK: codir.codelet deps(%{{[0-9]+}}, %{{.*}}, %{{.*}} : memref<4x256xf32>, memref<512xf32>, memref<1024x512xf32>)
// CHECK-SAME: dep_array_ids = [0, 2, 1]

func.func @dep_ids_by_size(%big: memref<1024x512xf32>, %small: memref<512xf32>) {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c512 = arith.constant 512 : index
  %c1024 = arith.constant 1024 : index
  %cst = arith.constant 0.0 : f32
  %out = sde.mu_alloc : memref<1024xf32>
  sde.cu_region <parallel> {
    sde.su_iterate (%c0) to (%c1024) step (%c1) reduction_strategy(<local_accumulate>) classification(<elementwise_pipeline>) {
    ^bb0(%i: index):
      memref.store %cst, %out[%i] : memref<1024xf32>
      scf.for %j = %c0 to %c512 step %c1 {
        %s = memref.load %small[%j] : memref<512xf32>
        %b = memref.load %big[%i, %j] : memref<1024x512xf32>
        %acc = memref.load %out[%i] : memref<1024xf32>
        %p = arith.mulf %b, %s : f32
        %a = arith.addf %acc, %p : f32
        memref.store %a, %out[%i] : memref<1024xf32>
      }
      sde.yield
    } {physicalOwnerDims = [0], physicalBlockShape = [256], arrayLayout = [{arrayId = 0 : i64, kind = "block_parallel", ownerDims = [0], blockShape = [256], muBlockCount = 4 : i64, role = "write", commVolumeBytes = 0 : i64}, {arrayId = 1 : i64, kind = "block_parallel", ownerDims = [0], blockShape = [256, 512], muBlockCount = 4 : i64, role = "read", commVolumeBytes = 0 : i64}, {arrayId = 2 : i64, kind = "replicated", ownerDims = [], blockShape = [512], muBlockCount = 1 : i64, role = "read", commVolumeBytes = 0 : i64}]}
    sde.yield
  }
  return
}
