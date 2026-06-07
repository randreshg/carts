// RUN: not %carts-compile %s --pass-pipeline='builtin.module(verify-sde-partition-plan)' 2>&1 \
// RUN:   | %FileCheck %s

module {
  func.func @reject_stale_committed_partition_plan(%A: memref<64x64xf32>) {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c64 = arith.constant 64 : index
    sde.cu_region <parallel> {
      sde.su_iterate (%c0) to (%c64) step (%c1) classification(<elementwise>) {
      ^bb0(%i: index):
        scf.for %j = %c0 to %c64 step %c1 {
          %v = memref.load %A[%i, %j] : memref<64x64xf32>
          memref.store %v, %A[%i, %j] : memref<64x64xf32>
        }
        sde.yield
      } {iterationTopology = #sde.iteration_topology<owner_strip>,
         logicalWorkerSlice = [16, 64],
         partitionGraph = [{blockShape = [8, 64],
                            edgeClass = "aligned",
                            edgeCommBytes = 0 : i64,
                            layoutKind = "owner_block",
                            muBlockCount = 8 : i64,
                            muId = 0 : i64,
                            ownerDims = [0],
                            role = "write",
                            tilePayloadBytes = 2048 : i64}],
         partitionScore = {blockShape = [8, 64],
                           chosenCuCount = 8 : i64,
                           chosenTileBytes = 2048 : i64,
                           commVolumeBytes = 0 : i64,
                           exposedCuCount = 8 : i64,
                           minTileBytes = 0 : i64,
                           muBlockCount = 8 : i64,
                           objective = "max_concurrency_comm_aware",
                           ownerDims = [0],
                           requestedCuCount = 8 : i64,
                           targetLogicalWorkers = 8 : i64},
         physicalBlockShape = [16, 64],
         physicalOwnerDims = [0]}
      sde.yield
    }
    return
  }

  func.func @reject_policy_in_partition_graph(%A: memref<64x64xf32>) {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c64 = arith.constant 64 : index
    sde.cu_region <parallel> {
      sde.su_iterate (%c0) to (%c64) step (%c1) classification(<elementwise>) {
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
                            role = "all_gather",
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

  func.func @reject_evidence_without_physical_plan(%A: memref<64x64xf32>) {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c64 = arith.constant 64 : index
    sde.cu_region <parallel> {
      sde.su_iterate (%c0) to (%c64) step (%c1) classification(<elementwise>) {
      ^bb0(%i: index):
        scf.for %j = %c0 to %c64 step %c1 {
          %v = memref.load %A[%i, %j] : memref<64x64xf32>
          memref.store %v, %A[%i, %j] : memref<64x64xf32>
        }
        sde.yield
      } {partitionGraph = [{blockShape = [16, 64],
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
                           targetLogicalWorkers = 4 : i64}}
      sde.yield
    }
    return
  }

  func.func @reject_incomplete_partition_evidence(%A: memref<64x64xf32>) {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c64 = arith.constant 64 : index
    sde.cu_region <parallel> {
      sde.su_iterate (%c0) to (%c64) step (%c1) classification(<elementwise>) {
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
                            role = "write",
                            tilePayloadBytes = 4096 : i64}],
         partitionScore = {chosenCuCount = 4 : i64,
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

  func.func @reject_unrealized_nested_stencil_partition_plan(%A: memref<64x64xf32>, %B: memref<64x64xf32>) {
    %c1 = arith.constant 1 : index
    %c63 = arith.constant 63 : index
    sde.cu_region <parallel> {
      sde.su_iterate (%c1) to (%c63) step (%c1) classification(<stencil>) {
      ^bb0(%i: index):
        scf.for %j = %c1 to %c63 step %c1 {
          %im1 = arith.subi %i, %c1 : index
          %ip1 = arith.addi %i, %c1 : index
          %jm1 = arith.subi %j, %c1 : index
          %jp1 = arith.addi %j, %c1 : index
          %n = memref.load %A[%im1, %j] : memref<64x64xf32>
          %s = memref.load %A[%ip1, %j] : memref<64x64xf32>
          %w = memref.load %A[%i, %jm1] : memref<64x64xf32>
          %e = memref.load %A[%i, %jp1] : memref<64x64xf32>
          %ns = arith.addf %n, %s : f32
          %we = arith.addf %w, %e : f32
          %sum = arith.addf %ns, %we : f32
          memref.store %sum, %B[%i, %j] : memref<64x64xf32>
        }
        sde.yield
      } {accessMaxOffsets = [1, 1],
         accessMinOffsets = [-1, -1],
         iterationTopology = #sde.iteration_topology<owner_strip>,
         logicalWorkerSlice = [16, 64],
         ownerDims = [0, 1],
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
         pattern = #sde.pattern<stencil_tiling_nd>,
         physicalBlockShape = [16, 64],
         physicalHaloShape = [1],
         physicalOwnerDims = [0],
         spatialDims = [0, 1],
         writeFootprint = [1, 1]}
      sde.yield
    }
    return
  }
}

// CHECK: partitionScore.blockShape no longer matches the SDE physical plan
// CHECK: partitionGraph.blockShape no longer matches the SDE physical plan
// CHECK: partitionGraph must stay runtime-neutral
// CHECK: committed CU/MU partition evidence requires physicalOwnerDims
// CHECK: committed CU/MU partition evidence requires physicalBlockShape
// CHECK: partitionScore.blockShape must be a non-empty i64 array attribute
// CHECK: partitionGraph.ownerDims must be a non-empty i64 array attribute
// CHECK: committed physical plan is not realizable by current SDE loop rank
