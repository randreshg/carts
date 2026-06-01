// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-pattern-analysis)' --mlir-print-ir-after-all 2>&1 | %FileCheck %s

// A committed CU/MU partition plan freezes the SDE graph partition shape.
// PatternAnalysis may not refresh the structured facts and silently turn the
// precommitted stencil contract into a newly rediscovered uniform loop.

// CHECK-LABEL: // -----// IR Dump After PatternAnalysis (sde-pattern-analysis) //----- //
// CHECK-LABEL: func.func @pattern_keeps_committed_partition_plan
// CHECK: sde.su_iterate
// CHECK-SAME: classification(<stencil>)
// CHECK: } {
// CHECK-SAME: partitionGraph = [
// CHECK-SAME: partitionScore = {
// CHECK-NOT: pattern =

module {
  func.func @pattern_keeps_committed_partition_plan(%A: memref<64x64xf32>) {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c64 = arith.constant 64 : index
    sde.cu_region <parallel> {
      sde.su_iterate (%c0) to (%c64) step (%c1) classification(<stencil>) {
      ^bb0(%i: index):
        scf.for %j = %c0 to %c64 step %c1 {
          %v = memref.load %A[%i, %j] : memref<64x64xf32>
          memref.store %v, %A[%i, %j] : memref<64x64xf32>
        }
        sde.yield
      } {iterationTopology = #sde.iteration_topology<owner_strip>,
         logicalWorkerSlice = [16, 64],
         partitionGraph = [{blockShape = [16, 64],
                            edgeClass = "aligned",
                            edgeCommBytes = 0 : i64,
                            layoutKind = "owner_block",
                            muBlockCount = 4 : i64,
                            muId = 0 : i64,
                            ownerDims = [0],
                            role = "write",
                            tilePayloadBytes = 4096 : i64}],
         partitionScore = {blockShape = [16, 64],
                           chosenCuCount = 4 : i64,
                           chosenTileBytes = 4096 : i64,
                           commVolumeBytes = 0 : i64,
                           exposedCuCount = 4 : i64,
                           minTileBytes = 0 : i64,
                           muBlockCount = 4 : i64,
                           objective = "max_concurrency_comm_aware",
                           ownerDims = [0],
                           requestedCuCount = 4 : i64,
                           targetLogicalWorkers = 4 : i64},
         physicalBlockShape = [16, 64],
         physicalOwnerDims = [0]}
      sde.yield
    }
    return
  }
}
