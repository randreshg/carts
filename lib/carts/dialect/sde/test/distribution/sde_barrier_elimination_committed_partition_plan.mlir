// RUN: %carts-compile %s --pass-pipeline='builtin.module(barrier-elimination)' --mlir-print-ir-after-all 2>&1 | %FileCheck %s

// BarrierElimination may stamp timestep/CPS intent, but it must not coarsen a
// physical owner-tile shape. DB/MU grain is set by SDE tiling/distribution
// planning; sync planning may only preserve or reject it.

// CHECK-LABEL: // -----// IR Dump After BarrierElimination (barrier-elimination) //----- //
// CHECK-LABEL: func.func @barrier_keeps_committed_owner_tile_shape
// CHECK: sde.su_iterate
// CHECK: } {
// CHECK-SAME: asyncStrategy = #sde.async_strategy<advance_stage>
// CHECK-SAME: logicalWorkerSlice = [8, 16]
// CHECK-SAME: partitionGraph = [
// CHECK-SAME: blockShape = [8, 16]
// CHECK-SAME: partitionScore = {
// CHECK-SAME: blockShape = [8, 16]
// CHECK-SAME: physicalBlockShape = [8, 16]
// CHECK-SAME: repetitionStructure = #sde.repetition_structure<full_timestep>
// CHECK: sde.su_barrier
// CHECK-SAME: barrierReason = #sde.barrier_reason<timestep_stage_boundary>
// CHECK: sde.su_iterate
// CHECK: } {
// CHECK-SAME: asyncStrategy = #sde.async_strategy<advance_stage>
// CHECK-SAME: logicalWorkerSlice = [8, 16]
// CHECK-SAME: partitionGraph = [
// CHECK-SAME: blockShape = [8, 16]
// CHECK-SAME: partitionScore = {
// CHECK-SAME: blockShape = [8, 16]
// CHECK-SAME: physicalBlockShape = [8, 16]
// CHECK-SAME: repetitionStructure = #sde.repetition_structure<full_timestep>
// CHECK-LABEL: func.func @barrier_keeps_uncommitted_owner_tile_shape
// CHECK: sde.su_iterate
// CHECK: } {
// CHECK-SAME: asyncStrategy = #sde.async_strategy<advance_stage>
// CHECK-SAME: logicalWorkerSlice = [8, 16]
// CHECK-SAME: physicalBlockShape = [8, 16]
// CHECK-SAME: repetitionStructure = #sde.repetition_structure<full_timestep>
// CHECK: sde.su_barrier
// CHECK-SAME: barrierReason = #sde.barrier_reason<timestep_stage_boundary>
// CHECK: sde.su_iterate
// CHECK: } {
// CHECK-SAME: asyncStrategy = #sde.async_strategy<advance_stage>
// CHECK-SAME: logicalWorkerSlice = [8, 16]
// CHECK-SAME: physicalBlockShape = [8, 16]
// CHECK-SAME: repetitionStructure = #sde.repetition_structure<full_timestep>

module {
  func.func @barrier_keeps_committed_owner_tile_shape(%A: memref<64x64xf64>, %B: memref<64x64xf64>) {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c64 = arith.constant 64 : index
    sde.cu_region <parallel> {
      sde.su_iterate (%c0, %c0) to (%c64, %c64) step (%c1, %c1) classification(<stencil>) {
      ^bb0(%i: index, %j: index):
        %v = memref.load %B[%i, %j] : memref<64x64xf64>
        memref.store %v, %A[%i, %j] : memref<64x64xf64>
        sde.yield
      } {iterationTopology = #sde.iteration_topology<owner_tile>,
         logicalWorkerSlice = [8, 16],
         partitionGraph = [{blockShape = [8, 16],
                            edgeClass = "aligned",
                            edgeCommBytes = 0 : i64,
                            layoutKind = "owner_block",
                            muBlockCount = 32 : i64,
                            muId = 0 : i64,
                            ownerDims = [0, 1],
                            role = "write",
                            tilePayloadBytes = 1024 : i64}],
         partitionScore = {blockShape = [8, 16],
                           chosenCuCount = 32 : i64,
                           chosenTileBytes = 1024 : i64,
                           commVolumeBytes = 0 : i64,
                           exposedCuCount = 32 : i64,
                           minTileBytes = 0 : i64,
                           muBlockCount = 32 : i64,
                           objective = "max_concurrency_comm_aware",
                           ownerDims = [0, 1],
                           requestedCuCount = 32 : i64,
                           targetLogicalWorkers = 32 : i64},
         pattern = #sde.pattern<stencil_tiling_nd>,
         physicalBlockShape = [8, 16],
         physicalOwnerDims = [0, 1]}
      sde.su_barrier
      sde.su_iterate (%c0, %c0) to (%c64, %c64) step (%c1, %c1) classification(<stencil>) {
      ^bb0(%i: index, %j: index):
        %v = memref.load %A[%i, %j] : memref<64x64xf64>
        memref.store %v, %B[%i, %j] : memref<64x64xf64>
        sde.yield
      } {iterationTopology = #sde.iteration_topology<owner_tile>,
         logicalWorkerSlice = [8, 16],
         partitionGraph = [{blockShape = [8, 16],
                            edgeClass = "aligned",
                            edgeCommBytes = 0 : i64,
                            layoutKind = "owner_block",
                            muBlockCount = 32 : i64,
                            muId = 0 : i64,
                            ownerDims = [0, 1],
                            role = "write",
                            tilePayloadBytes = 1024 : i64}],
         partitionScore = {blockShape = [8, 16],
                           chosenCuCount = 32 : i64,
                           chosenTileBytes = 1024 : i64,
                           commVolumeBytes = 0 : i64,
                           exposedCuCount = 32 : i64,
                           minTileBytes = 0 : i64,
                           muBlockCount = 32 : i64,
                           objective = "max_concurrency_comm_aware",
                           ownerDims = [0, 1],
                           requestedCuCount = 32 : i64,
                           targetLogicalWorkers = 32 : i64},
         pattern = #sde.pattern<stencil_tiling_nd>,
         physicalBlockShape = [8, 16],
         physicalOwnerDims = [0, 1]}
      sde.yield
    }
    return
  }

  func.func @barrier_keeps_uncommitted_owner_tile_shape(%A: memref<64x64xf64>, %B: memref<64x64xf64>) {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c64 = arith.constant 64 : index
    sde.cu_region <parallel> {
      sde.su_iterate (%c0, %c0) to (%c64, %c64) step (%c1, %c1) classification(<stencil>) {
      ^bb0(%i: index, %j: index):
        %v = memref.load %B[%i, %j] : memref<64x64xf64>
        memref.store %v, %A[%i, %j] : memref<64x64xf64>
        sde.yield
      } {iterationTopology = #sde.iteration_topology<owner_tile>,
         logicalWorkerSlice = [8, 16],
         pattern = #sde.pattern<stencil_tiling_nd>,
         physicalBlockShape = [8, 16],
         physicalOwnerDims = [0, 1]}
      sde.su_barrier
      sde.su_iterate (%c0, %c0) to (%c64, %c64) step (%c1, %c1) classification(<stencil>) {
      ^bb0(%i: index, %j: index):
        %v = memref.load %A[%i, %j] : memref<64x64xf64>
        memref.store %v, %B[%i, %j] : memref<64x64xf64>
        sde.yield
      } {iterationTopology = #sde.iteration_topology<owner_tile>,
         logicalWorkerSlice = [8, 16],
         pattern = #sde.pattern<stencil_tiling_nd>,
         physicalBlockShape = [8, 16],
         physicalOwnerDims = [0, 1]}
      sde.yield
    }
    return
  }
}
