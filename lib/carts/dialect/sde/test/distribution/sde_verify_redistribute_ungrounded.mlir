// RUN: not %carts-compile %s --pass-pipeline='builtin.module(canonicalize)' 2>&1 | %FileCheck %s

// GROUNDING gate: a well-formed sde.su_reduce_scatter whose array has no
// committed writer layout/provenance is rejected before ARTS.

// CHECK: error: {{.*}}not grounded in committed SDE layout

func.func @ungrounded() {
  %c0 = arith.constant 0 : index
  %c128 = arith.constant 128 : index
  %A = sde.mu_alloc : memref<128x64xf32>
  sde.su_distribute <owner_compute> {
    sde.su_reduce_scatter %A : memref<128x64xf32> array_id(0) owner [0] block [16, 64] reduce 0 kind <add>
    sde.su_iterate (%c0) to (%c128) step (%c128) {
    ^bb0(%i: index):
      sde.array_layout_root read %A : memref<128x64xf32> array_id(0)
      sde.cu_region <single> {
        sde.mu_access_window read %A : memref<128x64xf32> array_id(0)
        sde.yield
      }
      sde.yield
    } {arrayLayout = [{arrayId = 0 : i64, kind = "block_contraction", ownerDims = [0], blockShape = [16, 64], muBlockCount = 8 : i64, role = "read", commVolumeBytes = 0 : i64}], partialReduction, partialReductionDims = [0], partialReductionOwnerDims = [0]}
  }
  return
}
