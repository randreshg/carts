// RUN: %carts-compile %s --O3 --arts-config %inputs_dir/arts_64t.cfg --start-from sde-planning --pipeline sde-planning --mlir-print-ir-after-all 2>&1 | %FileCheck %s

// N-node-general distribution, Step 0 (non-behavioral oracle).
//
// LayoutAssignment commits TWO grains on the per-array arrayLayout:
//   - the abstract grain (blockShape/muBlockCount) sized by the fixed
//     kAbstractBlockFactor=2 proxy: for a 2-owner-dim array it is ALWAYS
//     muBlockCount = 2^2 = 4, independent of problem size or node count. This is
//     the grain that caps owner-block redistribution at ~4 nodes.
//   - the budget grain (budgetBlockShape/budgetMuBlockCount) sized by a
//     node-agnostic target block-byte budget: a function of problem size only,
//     so the block COUNT grows with the array and can be redistributed across
//     any N at runtime via the owner map. For a 4096x4096 f64 array (128 MiB) at
//     a ~2 MiB budget this is 64 blocks of [512, 512] -- 16x the abstract 4.
//
// The budget grain is emitted but not yet consumed; this test pins it so a
// regression back to a node-count-tied grain is caught. See
// distribution-architecture-n-node-general-2026-06-01.md.

// CHECK: // -----// IR Dump After LayoutAssignment (sde-layout-assignment) //----- //
// CHECK: arrayLayout = [{
// Abstract grain: fixed at 2^owner-dims = 4, regardless of the 4096x4096 size.
// CHECK-SAME: blockShape = [2048, 2048]
// Budget grain: many fine blocks sized by problem size, NOT node count.
// CHECK-SAME: budgetBlockShape = [512, 512]
// CHECK-SAME: budgetMuBlockCount = 64 : i64
// CHECK-SAME: muBlockCount = 4 : i64
// CHECK-SAME: ownerDims = [0, 1]

module {
  func.func @main(%A: memref<4096x4096xf64>) {
    %c0 = arith.constant 0 : index
    %c4096 = arith.constant 4096 : index
    %c1 = arith.constant 1 : index
    %cst = arith.constant 1.000000e+00 : f64
    sde.cu_region <parallel> {
      sde.su_iterate (%c0, %c0) to (%c4096, %c4096) step (%c1, %c1) {
      ^bb0(%i: index, %j: index):
        memref.store %cst, %A[%i, %j] : memref<4096x4096xf64>
        sde.yield
      }
      sde.yield
    }
    return
  }
}
