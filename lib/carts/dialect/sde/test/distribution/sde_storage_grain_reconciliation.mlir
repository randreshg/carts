// RUN: %carts-compile %s --pass-pipeline='builtin.module(storage-grain-reconciliation)' --mlir-print-ir-after-all 2>&1 | %FileCheck %s

// StorageGrainReconciliation shares one DB grain across the multiple su_iterate
// writers of one array. When structurally different writers (a data-parallel
// init and a pipeline kernel) commit divergent physicalBlockShape, the array
// would coarsen to a host_whole DB; this pass reconciles them to the per-dim GCD
// of their blocks (rewriting physicalBlockShape + the committed partition
// evidence) so the array stays block-native. Fail-closed: single-writer,
// already-coherent, and stencil arrays are left untouched. (arrayId is
// module-stable, so each array under test uses a distinct id.)

// CHECK-LABEL: // -----// IR Dump After StorageGrainReconciliation (storage-grain-reconciliation) //----- //

// The two writers of array id 0 (init [4,64], pipeline [8,64]) converge to the
// GCD [4,64], and muBlockCount is rescaled consistently (16 and 8*2 = 16).
// CHECK-LABEL: func.func @reconciles_divergent_writers
// CHECK: sde.su_iterate
// CHECK: } {
// CHECK-SAME: muBlockCount = 16
// CHECK-SAME: physicalBlockShape = [4, 64]
// CHECK: sde.su_iterate
// CHECK: } {
// CHECK-SAME: muBlockCount = 16
// CHECK-SAME: physicalBlockShape = [4, 64]

// A single writer is never reconciled (nothing to share a grain with).
// CHECK-LABEL: func.func @skips_single_writer
// CHECK: sde.su_iterate
// CHECK: } {
// CHECK-SAME: physicalBlockShape = [8, 64]

// Stencil writers own a bespoke owner_tile + halo grain and are excluded even
// when divergent.
// CHECK-LABEL: func.func @skips_stencil_writers
// CHECK: sde.su_iterate
// CHECK: } {
// CHECK-SAME: physicalBlockShape = [4, 64]
// CHECK: sde.su_iterate
// CHECK: } {
// CHECK-SAME: physicalBlockShape = [8, 64]

module {
  func.func @reconciles_divergent_writers(%A: memref<64x64xf32>) {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c64 = arith.constant 64 : index
    %zero = arith.constant 0.000000e+00 : f32
    sde.cu_region <parallel> {
      // init writer: elementwise, tiled to step 4 -> physicalBlockShape [4,64].
      sde.su_iterate (%c0) to (%c64) step (%c1) classification(<elementwise>) {
      ^bb0(%i: index):
        memref.store %zero, %A[%i, %i] : memref<64x64xf32>
        sde.yield
      } {arrayLayout = [{arrayId = 0 : i64, blockShape = [32, 64], budgetBlockShape = [64, 64], budgetMuBlockCount = 1 : i64, commVolumeBytes = 0 : i64, kind = "block_parallel", muBlockCount = 2 : i64, ownerDims = [0], role = "write"}],
         iterationTopology = #sde.iteration_topology<owner_strip>,
         logicalWorkerSlice = [4, 64],
         partitionGraph = [{blockShape = [4, 64], edgeClass = "aligned", edgeCommBytes = 0 : i64, layoutKind = "owner_block", muBlockCount = 16 : i64, muId = 0 : i64, ownerDims = [0], role = "write", tilePayloadBytes = 1024 : i64}],
         partitionScore = {blockShape = [4, 64], chosenCuCount = 16 : i64, chosenTileBytes = 1024 : i64, commVolumeBytes = 0 : i64, exposedCuCount = 16 : i64, minTileBytes = 0 : i64, muBlockCount = 16 : i64, objective = "max_concurrency_comm_aware", ownerDims = [0], requestedCuCount = 16 : i64, targetLogicalWorkers = 16 : i64},
         physicalBlockShape = [4, 64],
         physicalOwnerDims = [0]}
      // pipeline kernel writer: elementwise_pipeline + in-place, step 8 -> [8,64].
      sde.su_iterate (%c0) to (%c64) step (%c1) classification(<elementwise_pipeline>) {
      ^bb0(%i: index):
        memref.store %zero, %A[%i, %i] : memref<64x64xf32>
        sde.yield
      } {arrayLayout = [{arrayId = 0 : i64, blockShape = [32, 64], budgetBlockShape = [64, 64], budgetMuBlockCount = 1 : i64, commVolumeBytes = 0 : i64, kind = "block_parallel", muBlockCount = 2 : i64, ownerDims = [0], role = "write"}],
         inPlaceSafe,
         iterationTopology = #sde.iteration_topology<owner_strip>,
         logicalWorkerSlice = [8, 64],
         partitionGraph = [{blockShape = [8, 64], edgeClass = "aligned", edgeCommBytes = 0 : i64, layoutKind = "owner_block", muBlockCount = 8 : i64, muId = 0 : i64, ownerDims = [0], role = "write", tilePayloadBytes = 2048 : i64}],
         partitionScore = {blockShape = [8, 64], chosenCuCount = 8 : i64, chosenTileBytes = 2048 : i64, commVolumeBytes = 0 : i64, exposedCuCount = 8 : i64, minTileBytes = 0 : i64, muBlockCount = 8 : i64, objective = "max_concurrency_comm_aware", ownerDims = [0], requestedCuCount = 8 : i64, targetLogicalWorkers = 8 : i64},
         physicalBlockShape = [8, 64],
         physicalOwnerDims = [0]}
      sde.yield
    }
    return
  }

  func.func @skips_single_writer(%A: memref<64x64xf32>) {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c64 = arith.constant 64 : index
    %zero = arith.constant 0.000000e+00 : f32
    sde.cu_region <parallel> {
      sde.su_iterate (%c0) to (%c64) step (%c1) classification(<elementwise>) {
      ^bb0(%i: index):
        memref.store %zero, %A[%i, %i] : memref<64x64xf32>
        sde.yield
      } {arrayLayout = [{arrayId = 1 : i64, blockShape = [32, 64], budgetBlockShape = [64, 64], budgetMuBlockCount = 1 : i64, commVolumeBytes = 0 : i64, kind = "block_parallel", muBlockCount = 2 : i64, ownerDims = [0], role = "write"}],
         iterationTopology = #sde.iteration_topology<owner_strip>,
         logicalWorkerSlice = [8, 64],
         physicalBlockShape = [8, 64],
         physicalOwnerDims = [0]}
      sde.yield
    }
    return
  }

  func.func @skips_stencil_writers(%A: memref<64x64xf32>, %B: memref<64x64xf32>) {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c64 = arith.constant 64 : index
    sde.cu_region <parallel> {
      sde.su_iterate (%c0) to (%c64) step (%c1) classification(<stencil>) {
      ^bb0(%i: index):
        %v = memref.load %B[%i, %i] : memref<64x64xf32>
        memref.store %v, %A[%i, %i] : memref<64x64xf32>
        sde.yield
      } {arrayLayout = [{arrayId = 2 : i64, blockShape = [32, 64], budgetBlockShape = [64, 64], budgetMuBlockCount = 1 : i64, commVolumeBytes = 0 : i64, kind = "block_parallel", muBlockCount = 2 : i64, ownerDims = [0], role = "write"}],
         iterationTopology = #sde.iteration_topology<owner_strip>,
         logicalWorkerSlice = [4, 64],
         physicalBlockShape = [4, 64],
         physicalOwnerDims = [0]}
      sde.su_iterate (%c0) to (%c64) step (%c1) classification(<stencil>) {
      ^bb0(%i: index):
        %v = memref.load %B[%i, %i] : memref<64x64xf32>
        memref.store %v, %A[%i, %i] : memref<64x64xf32>
        sde.yield
      } {arrayLayout = [{arrayId = 2 : i64, blockShape = [32, 64], budgetBlockShape = [64, 64], budgetMuBlockCount = 1 : i64, commVolumeBytes = 0 : i64, kind = "block_parallel", muBlockCount = 2 : i64, ownerDims = [0], role = "write"}],
         iterationTopology = #sde.iteration_topology<owner_strip>,
         logicalWorkerSlice = [8, 64],
         physicalBlockShape = [8, 64],
         physicalOwnerDims = [0]}
      sde.yield
    }
    return
  }
}
