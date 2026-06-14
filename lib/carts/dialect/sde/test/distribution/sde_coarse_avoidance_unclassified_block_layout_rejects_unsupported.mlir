// RUN: not %carts-compile %s --pass-pipeline='builtin.module(sde-coarse-avoidance)' 2>&1 | %FileCheck %s

// Unclassified committed BLOCK layouts with unsupported root uses must fail
// closed with the normal realization diagnostic, not crash on a missing
// structuredClassification attr.

// CHECK: committed block-grid layout cannot be realized as a rank-expanded MU

func.func @coarse_avoidance_unclassified_layout_rejects_unsupported_use() {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c64 = arith.constant 64 : index
  %one = arith.constant 1.0 : f32
  %A = sde.mu_alloc : memref<64xf32>
  %view = memref.cast %A : memref<64xf32> to memref<?xf32>
  sde.su_iterate (%c0) to (%c64) step (%c1) {
  ^bb0(%i: index):
    sde.array_layout_root write %A : memref<64xf32> array_id(0)
    sde.cu_region <parallel> {
      func.call @opaque() : () -> ()
      memref.store %one, %A[%i] : memref<64xf32>
    }
  } {arrayLayout = [{arrayId = 0 : i64, blockShape = [32],
       kind = "block_parallel", muBlockCount = 2 : i64, ownerDims = [0],
       role = "write"}]}
  memref.dealloc %view : memref<?xf32>
  return
}

func.func private @opaque()
