// RUN: not %carts-compile %s --pass-pipeline='builtin.module(sde-redistribute)' 2>&1 | %FileCheck %s

// Same-owner re-tiling is a BlockGrainPlan responsibility, not an all_to_all
// movement. If stale same-owner reader grain reaches redistribution, SDE must
// fail closed instead of leaving a mismatched read fact for downstream repair.

// CHECK-NOT: sde.su_all_to_all
// CHECK: same-owner block-grain mismatch must be reconciled before redistribution
// CHECK-NOT: sde.su_all_to_all

func.func @same_owner_retile_rejected(%T: memref<256x256xf32>,
                                      %U: memref<256x256xf32>) {
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
  } {arrayLayout = [{arrayId = 0 : i64, kind = "block_parallel", ownerDims = [0], blockShape = [128, 256], muBlockCount = 2 : i64, role = "write"}]}
  sde.su_distribute <owner_compute> {
  sde.su_iterate (%c0, %c0) to (%c256, %c256) step (%c1, %c1) {
  ^bb0(%i: index, %j: index):
    sde.array_layout_root read %T : memref<256x256xf32> array_id(0)
    sde.array_layout_root write %U : memref<256x256xf32> array_id(1)
    sde.cu_region <single> {
      %v = memref.load %T[%i, %j] : memref<256x256xf32>
      memref.store %v, %U[%i, %j] : memref<256x256xf32>
      sde.yield
    }
  } {arrayLayout = [{arrayId = 0 : i64, kind = "block_parallel", ownerDims = [0], blockShape = [64, 256], muBlockCount = 4 : i64, role = "read"}, {arrayId = 1 : i64, kind = "block_parallel", ownerDims = [0], blockShape = [64, 256], muBlockCount = 4 : i64, role = "write"}]}
  }
  return
}
