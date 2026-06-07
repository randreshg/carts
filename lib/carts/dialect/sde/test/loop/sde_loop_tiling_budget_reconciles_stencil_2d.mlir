// RUN: %carts-compile %s --O3 --arts-config %inputs_dir/arts_64t.cfg \
// RUN:   --start-from sde-planning --pipeline sde-to-codir --mlir-print-ir-after-all 2>&1 \
// RUN:   | %FileCheck %s

// SDE tiling must consume the committed budget grain for out-of-place stencils
// as a real loop/DB block shape. It must not leave the loop at the generic
// worker-factor tile and then expect CODIR/ARTS to repair coarse DBs later.
// Until ARTS can materialize grouped halo compute lanes, the stencil logical
// worker slice remains identical to the physical DB/MU block.

// CHECK-LABEL: // -----// IR Dump After Tiling (tiling) //----- //
// CHECK-LABEL: func.func @budget_reconciles_stencil_2d
// CHECK: sde.su_iterate (%c1{{(_[0-9]+)?}}, %c1{{(_[0-9]+)?}}) to (%c5119{{(_[0-9]+)?}}, %c5119{{(_[0-9]+)?}}) step (%c512{{(_[0-9]+)?}}, %c512{{(_[0-9]+)?}}) classification(<stencil>) {
// CHECK: } {
// CHECK-SAME: logicalWorkerSlice = [512, 512]
// CHECK-SAME: physicalBlockShape = [512, 512]
// CHECK-SAME: physicalHaloShape = [1, 1]
// CHECK-SAME: physicalOwnerDims = [0, 1]

// CHECK-LABEL: // -----// IR Dump After ConvertSdeToCodir (convert-sde-to-codir) //----- //
// CHECK-LABEL: func.func @budget_reconciles_stencil_2d
// CHECK: codir.codelet
// CHECK-SAME: halo_shape = [1, 1]
// CHECK-SAME: logical_worker_slice = [512, 512]
// CHECK-SAME: tile_shape = [512, 512]

module {
  func.func @budget_reconciles_stencil_2d(%A: memref<5120x5120xf64>, %B: memref<5120x5120xf64>) {
    %c1 = arith.constant 1 : index
    %c5119 = arith.constant 5119 : index
    sde.cu_region <parallel> {
      sde.su_iterate (%c1, %c1) to (%c5119, %c5119) step (%c1, %c1) classification(<stencil>) {
      ^bb0(%i: index, %j: index):
        %im1 = arith.subi %i, %c1 : index
        %ip1 = arith.addi %i, %c1 : index
        %jm1 = arith.subi %j, %c1 : index
        %jp1 = arith.addi %j, %c1 : index
        %n = memref.load %A[%im1, %j] : memref<5120x5120xf64>
        %s = memref.load %A[%ip1, %j] : memref<5120x5120xf64>
        %w = memref.load %A[%i, %jm1] : memref<5120x5120xf64>
        %e = memref.load %A[%i, %jp1] : memref<5120x5120xf64>
        %sum0 = arith.addf %n, %s : f64
        %sum1 = arith.addf %w, %e : f64
        %sum = arith.addf %sum0, %sum1 : f64
        memref.store %sum, %B[%i, %j] : memref<5120x5120xf64>
        sde.yield
      } {accessMaxOffsets = [1, 1],
         accessMinOffsets = [-1, -1],
         arrayLayout = [{arrayId = 0 : i64, blockShape = [2560, 2560], budgetBlockShape = [512, 512], budgetMuBlockCount = 100 : i64, commVolumeBytes = 209715200 : i64, kind = "block_parallel", muBlockCount = 4 : i64, ownerDims = [0, 1], role = "read"},
                        {arrayId = 1 : i64, blockShape = [2560, 2560], budgetBlockShape = [512, 512], budgetMuBlockCount = 100 : i64, commVolumeBytes = 0 : i64, kind = "block_parallel", muBlockCount = 4 : i64, ownerDims = [0, 1], role = "write"}],
         commVolumeBytes = 32768 : i64,
         layoutsDisagree = [0],
         ownerDims = [0, 1],
         pattern = #sde.pattern<stencil_tiling_nd>,
         spatialDims = [0, 1],
         writeFootprint = [1, 1]}
      sde.yield
    }
    return
  }
}
