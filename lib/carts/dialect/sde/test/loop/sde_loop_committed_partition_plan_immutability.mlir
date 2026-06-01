// RUN: %carts-compile %s --pass-pipeline='builtin.module(loop-interchange)' --mlir-print-ir-after-all 2>&1 | %FileCheck %s --check-prefix=INTERCHANGE
// RUN: %carts-compile %s --O3 --arts-config %inputs_dir/arts_64t.cfg --start-from sde-planning --pipeline sde-planning --mlir-print-ir-after-all 2>&1 | %FileCheck %s --check-prefix=TILING

// Loop rewrites must leave precommitted CU/MU partition plans untouched.

// INTERCHANGE-LABEL: // -----// IR Dump After LoopInterchange (loop-interchange) //----- //
// INTERCHANGE-LABEL: func.func @interchange_keeps_committed_matmul
// INTERCHANGE: sde.su_iterate
// INTERCHANGE: scf.for %[[J:[^ ]+]] = %c0 to %c16 step %c1 {
// INTERCHANGE: memref.store %{{.*}}, %{{.*}}[%{{.*}}, %[[J]]] : memref<16x16xf64>
// INTERCHANGE-NEXT: scf.for %[[K:[^ ]+]] = %c0 to %c16 step %c1 {
// INTERCHANGE: memref.load %{{.*}}[%{{.*}}, %[[K]]] : memref<16x16xf64>
// INTERCHANGE: memref.load %{{.*}}[%[[K]], %[[J]]] : memref<16x16xf64>
// INTERCHANGE: } {
// INTERCHANGE-SAME: partitionGraph = [
// INTERCHANGE-SAME: partitionScore = {

// TILING-LABEL: // -----// IR Dump After Tiling (tiling) //----- //
// TILING-LABEL: func.func @tiling_keeps_committed_elementwise
// TILING: sde.su_iterate (%c0) to (%c128) step (%c1) classification(<elementwise>) {
// TILING: } {
// TILING-SAME: logicalWorkerSlice = [16, 64]
// TILING-SAME: partitionGraph = [
// TILING-SAME: partitionScore = {
// TILING-SAME: physicalBlockShape = [16, 64]

module {
  func.func @interchange_keeps_committed_matmul(%A: memref<16x16xf64>, %B: memref<16x16xf64>, %C: memref<16x16xf64>) {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c16 = arith.constant 16 : index
    %zero = arith.constant 0.000000e+00 : f64
    sde.cu_region <parallel> {
      sde.su_iterate (%c0) to (%c16) step (%c1) classification(<matmul>) {
      ^bb0(%i: index):
        scf.for %j = %c0 to %c16 step %c1 {
          memref.store %zero, %C[%i, %j] : memref<16x16xf64>
          scf.for %k = %c0 to %c16 step %c1 {
            %a = memref.load %A[%i, %k] : memref<16x16xf64>
            %b = memref.load %B[%k, %j] : memref<16x16xf64>
            %old = memref.load %C[%i, %j] : memref<16x16xf64>
            %prod = arith.mulf %a, %b : f64
            %next = arith.addf %old, %prod : f64
            memref.store %next, %C[%i, %j] : memref<16x16xf64>
          }
        }
        sde.yield
      } {iterationTopology = #sde.iteration_topology<owner_strip>,
         logicalWorkerSlice = [16, 16],
         partitionGraph = [{blockShape = [16, 16],
                            edgeClass = "aligned",
                            edgeCommBytes = 0 : i64,
                            layoutKind = "owner_block",
                            muBlockCount = 1 : i64,
                            muId = 0 : i64,
                            ownerDims = [0],
                            role = "write",
                            tilePayloadBytes = 2048 : i64}],
         partitionScore = {blockShape = [16, 16],
                           chosenCuCount = 1 : i64,
                           chosenTileBytes = 2048 : i64,
                           commVolumeBytes = 0 : i64,
                           exposedCuCount = 1 : i64,
                           minTileBytes = 0 : i64,
                           muBlockCount = 1 : i64,
                           objective = "max_concurrency_comm_aware",
                           ownerDims = [0],
                           requestedCuCount = 1 : i64,
                           targetLogicalWorkers = 1 : i64},
         physicalBlockShape = [16, 16],
         physicalOwnerDims = [0]}
      sde.yield
    }
    return
  }

  func.func @tiling_keeps_committed_elementwise(%A: memref<128x64xf32>, %C: memref<128x64xf32>) {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c64 = arith.constant 64 : index
    %c128 = arith.constant 128 : index
    sde.cu_region <parallel> {
      sde.su_iterate (%c0) to (%c128) step (%c1) classification(<elementwise>) {
      ^bb0(%i: index):
        scf.for %j = %c0 to %c64 step %c1 {
          %v = memref.load %A[%i, %j] : memref<128x64xf32>
          memref.store %v, %C[%i, %j] : memref<128x64xf32>
        }
        sde.yield
      } {iterationTopology = #sde.iteration_topology<owner_strip>,
         logicalWorkerSlice = [16, 64],
         partitionGraph = [{blockShape = [16, 64],
                            edgeClass = "aligned",
                            edgeCommBytes = 0 : i64,
                            layoutKind = "owner_block",
                            muBlockCount = 8 : i64,
                            muId = 0 : i64,
                            ownerDims = [0],
                            role = "write",
                            tilePayloadBytes = 4096 : i64}],
         partitionScore = {blockShape = [16, 64],
                           chosenCuCount = 8 : i64,
                           chosenTileBytes = 4096 : i64,
                           commVolumeBytes = 0 : i64,
                           exposedCuCount = 8 : i64,
                           minTileBytes = 0 : i64,
                           muBlockCount = 8 : i64,
                           objective = "max_concurrency_comm_aware",
                           ownerDims = [0],
                           requestedCuCount = 8 : i64,
                           targetLogicalWorkers = 8 : i64},
         pattern = #sde.pattern<uniform>,
         physicalBlockShape = [16, 64],
         physicalOwnerDims = [0]}
      sde.yield
    }
    return
  }
}
