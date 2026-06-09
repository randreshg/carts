// RUN: %carts-compile %s --pass-pipeline='builtin.module(convert-sde-to-codir,verify-codir)' 2>&1 \
// RUN:   | %FileCheck %s --implicit-check-not=sde.redist --implicit-check-not=sde.mu_access_window

// A committed read access window is SDE structure, not an unknown readwrite
// body access. CODIR must keep the redist consumer read-only and record the
// movement on that read edge.

// CHECK-LABEL: func.func @redist_window_read_stays_read
// CHECK: memref.subview %{{.*}}[%{{.*}}, 0] [1, 64] [1, 1]
// CHECK: codir.codelet deps(%{{.*}} : memref<1x64xf32
// CHECK-SAME: dep_modes = [#codir.access_mode<write>]
// CHECK-SAME: dep_owner_dims = {{\[\[}}0]]
// CHECK-SAME: dep_storage_views = [#codir.storage_view<compute_block>]
// CHECK: codir.codelet deps(%{{.*}}, %{{.*}} : memref<1x64xf32{{.*}}, memref<1x64xf32
// CHECK-SAME: dep_collectives = [#codir.collective<reduce_scatter>, #codir.collective<none>]
// CHECK-SAME: dep_modes = [#codir.access_mode<read>, #codir.access_mode<write>]
// CHECK-SAME: dep_owner_dims = {{\[\[}}0], [0]]
// CHECK-SAME: dep_storage_views = [#codir.storage_view<compute_block>, #codir.storage_view<compute_block>]

func.func @redist_window_read_stays_read() {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c4 = arith.constant 4 : index
  %c64 = arith.constant 64 : index
  %cst = arith.constant 1.0 : f32
  %A = sde.mu_alloc : memref<4x64xf32>
  %R = sde.mu_alloc : memref<4x64xf32>
  sde.cu_region <parallel> {
    sde.su_iterate (%c0) to (%c4) step (%c1) {
    ^bb0(%g: index):
      sde.cu_region <parallel> {
        sde.mu_access_window write %A : memref<4x64xf32> owner_dims(1) block_lo [0] block_hi [4] valid [64]
        scf.for %i = %c0 to %c64 step %c1 {
          memref.store %cst, %A[%g, %i] : memref<4x64xf32>
        }
        sde.yield
      }
      sde.yield
    } {arrayLayout = [{arrayId = 0 : i64, blockShape = [1, 64], commVolumeBytes = 0 : i64, kind = "owner_block", muBlockCount = 4 : i64, ownerDims = [0], role = "write"}], physicalBlockShape = [1, 64], physicalOwnerDims = [0]}
    sde.redist <reduce_scatter_like> %A : memref<4x64xf32> from owner [0] block [1, 64] to owner [0] block [1, 64] cost 1024
    sde.su_iterate (%c0) to (%c4) step (%c1) {
    ^bb0(%g: index):
      sde.cu_region <parallel> {
        sde.mu_access_window read %A : memref<4x64xf32> owner_dims(1) block_lo [0] block_hi [4] valid [64]
        sde.mu_access_window write %R : memref<4x64xf32> owner_dims(1) block_lo [0] block_hi [4] valid [64]
        scf.for %i = %c0 to %c64 step %c1 {
          %v = memref.load %A[%g, %i] : memref<4x64xf32>
          memref.store %v, %R[%g, %i] : memref<4x64xf32>
        }
        sde.yield
      }
      sde.yield
    } {arrayLayout = [{arrayId = 0 : i64, blockShape = [1, 64], commVolumeBytes = 1024 : i64, kind = "owner_block", muBlockCount = 4 : i64, ownerDims = [0], role = "read"}, {arrayId = 1 : i64, blockShape = [1, 64], commVolumeBytes = 0 : i64, kind = "owner_block", muBlockCount = 4 : i64, ownerDims = [0], role = "write"}], layoutsDisagree = [0], physicalBlockShape = [1, 64], physicalOwnerDims = [0]}
    sde.yield
  }
  return
}
