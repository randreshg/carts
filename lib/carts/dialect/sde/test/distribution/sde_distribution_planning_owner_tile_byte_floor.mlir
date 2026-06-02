// RUN: %carts-compile %s --O3 --arts-config %inputs_dir/arts_multinode.cfg \
// RUN:   --start-from sde-planning --pipeline sde-to-codir \
// RUN:   --mlir-print-ir-after-all 2>&1 \
// RUN:   | %FileCheck %s --check-prefix=BASE
// RUN: %carts-compile %s --O3 --arts-config %inputs_dir/arts_multinode.cfg \
// RUN:   --start-from sde-planning --pipeline sde-to-codir \
// RUN:   --min-distributed-tile-bytes=1048576 \
// RUN:   --mlir-print-ir-after-all 2>&1 \
// RUN:   | %FileCheck %s --check-prefix=COARSE

// Multidimensional owner-tile elementwise plans are upstream SDE block-layout
// decisions. DistributionPlanning may add graph evidence around the realized
// block shape, but it must not coarsen or rewrite an already tiled owner plan.

// BASE-LABEL: // -----// IR Dump After DistributionPlanning (distribution-planning) //----- //
// BASE: func.func @elementwise_inplace_2d_owner_tile
// BASE: iterationTopology = #sde.iteration_topology<owner_tile>
// BASE-SAME: logicalWorkerSlice = [86, 86, 16]
// BASE-SAME: partitionGraph = [
// BASE-SAME: blockShape = [86, 86, 16]
// BASE-SAME: muBlockCount = 36 : i64
// BASE-SAME: partitionScore = {
// BASE-SAME: blockShape = [86, 86, 16]
// BASE-SAME: exposedCuCount = 16 : i64
// BASE-SAME: muBlockCount = 36 : i64
// BASE-SAME: physicalBlockShape = [86, 86, 16]
// BASE-SAME: physicalOwnerDims = [0, 1]
// BASE: func.func @precommitted_owner_tile
// BASE: iterationTopology = #sde.iteration_topology<owner_tile>
// BASE-SAME: logicalWorkerSlice = [64, 128, 16]
// BASE-SAME: physicalBlockShape = [64, 128, 16]
// BASE-SAME: physicalOwnerDims = [0, 1]
// BASE: func.func @physical_only_owner_tile
// BASE: iterationTopology = #sde.iteration_topology<owner_tile>
// BASE-SAME: logicalWorkerSlice = [64, 128, 16]
// BASE-SAME: physicalBlockShape = [64, 128, 16]
// BASE-SAME: physicalOwnerDims = [0, 1]

// COARSE-LABEL: // -----// IR Dump After DistributionPlanning (distribution-planning) //----- //
// COARSE: func.func @elementwise_inplace_2d_owner_tile
// COARSE: iterationTopology = #sde.iteration_topology<owner_tile>
// COARSE-SAME: logicalWorkerSlice = [86, 86, 16]
// COARSE-SAME: partitionGraph = [{blockShape = [86, 86, 16]
// COARSE-SAME: muBlockCount = 36 : i64
// COARSE-SAME: partitionScore = {blockShape = [86, 86, 16]
// COARSE-SAME: cuGroupCount = 18 : i64
// COARSE-SAME: cuGroupSize = 2 : i64
// COARSE-SAME: minTileBytes = 1048576 : i64
// COARSE-SAME: muBlockCount = 36 : i64
// COARSE-SAME: physicalBlockShape = [86, 86, 16]
// COARSE-SAME: physicalOwnerDims = [0, 1]
// COARSE: func.func @precommitted_owner_tile
// COARSE: iterationTopology = #sde.iteration_topology<owner_tile>
// COARSE-SAME: logicalWorkerSlice = [64, 128, 16]
// COARSE-SAME: partitionGraph = [
// COARSE-SAME: blockShape = [64, 128, 16]
// COARSE-SAME: partitionScore = {
// COARSE-SAME: blockShape = [64, 128, 16]
// COARSE-SAME: physicalBlockShape = [64, 128, 16]
// COARSE-SAME: physicalOwnerDims = [0, 1]
// COARSE: func.func @physical_only_owner_tile
// COARSE: iterationTopology = #sde.iteration_topology<owner_tile>
// COARSE-SAME: logicalWorkerSlice = [64, 128, 16]
// COARSE-SAME: physicalBlockShape = [64, 128, 16]
// COARSE-SAME: physicalOwnerDims = [0, 1]
// COARSE-LABEL: // -----// IR Dump After VerifySdeCpsPlan (verify-sde-cps-plan) //----- //
// COARSE: func.func @precommitted_owner_tile
// COARSE: iterationTopology = #sde.iteration_topology<owner_tile>
// COARSE-SAME: logicalWorkerSlice = [64, 128, 16]
// COARSE-SAME: partitionGraph = [
// COARSE-SAME: blockShape = [64, 128, 16]
// COARSE-SAME: partitionScore = {
// COARSE-SAME: blockShape = [64, 128, 16]
// COARSE-SAME: physicalBlockShape = [64, 128, 16]
// COARSE-SAME: physicalOwnerDims = [0, 1]
// COARSE: func.func @physical_only_owner_tile
// COARSE: iterationTopology = #sde.iteration_topology<owner_tile>
// COARSE-SAME: logicalWorkerSlice = [64, 128, 16]
// COARSE-SAME: physicalBlockShape = [64, 128, 16]
// COARSE-SAME: physicalOwnerDims = [0, 1]

module attributes {
  dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f32, dense<32> : vector<2xi64>>, #dlti.dl_entry<i64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i32, dense<32> : vector<2xi64>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi64>>, #dlti.dl_entry<"dlti.endianness", "little">>,
  llvm.data_layout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128",
  llvm.target_triple = "aarch64-unknown-linux-gnu"
} {
  func.func @elementwise_inplace_2d_owner_tile(%A: memref<512x512x16xf32>, %B: memref<512x512x16xf32>) {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c16 = arith.constant 16 : index
    %c512 = arith.constant 512 : index
    sde.cu_region <parallel> {
      sde.su_iterate (%c0, %c0) to (%c512, %c512) step (%c1, %c1) classification(<elementwise>) {
      ^bb0(%b: index, %c: index):
        scf.for %i = %c0 to %c16 step %c1 {
          %old = memref.load %A[%b, %c, %i] : memref<512x512x16xf32>
          %bias = memref.load %B[%b, %c, %i] : memref<512x512x16xf32>
          %next = arith.addf %old, %bias : f32
          memref.store %next, %A[%b, %c, %i] : memref<512x512x16xf32>
        }
        sde.yield
      }
      sde.yield
    }
    return
  }

  func.func @precommitted_owner_tile(%A: memref<512x512x16xf32>, %B: memref<512x512x16xf32>) {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c16 = arith.constant 16 : index
    %c512 = arith.constant 512 : index
    sde.cu_region <parallel> {
      sde.su_iterate (%c0, %c0) to (%c512, %c512) step (%c1, %c1) classification(<elementwise>) {
      ^bb0(%b: index, %c: index):
        scf.for %i = %c0 to %c16 step %c1 {
          %old = memref.load %A[%b, %c, %i] : memref<512x512x16xf32>
          %bias = memref.load %B[%b, %c, %i] : memref<512x512x16xf32>
          %next = arith.addf %old, %bias : f32
          memref.store %next, %A[%b, %c, %i] : memref<512x512x16xf32>
        }
        sde.yield
      } {iterationTopology = #sde.iteration_topology<owner_tile>,
         logicalWorkerSlice = [64, 128, 16],
         partitionGraph = [{blockShape = [64, 128, 16],
                            edgeClass = "aligned",
                            edgeCommBytes = 0 : i64,
                            layoutKind = "owner_block",
                            muBlockCount = 64 : i64,
                            muId = 0 : i64,
                            ownerDims = [0, 1],
                            role = "write",
                            tilePayloadBytes = 524288 : i64}],
         partitionScore = {blockShape = [64, 128, 16],
                           chosenCuCount = 64 : i64,
                           chosenTileBytes = 524288 : i64,
                           commVolumeBytes = 0 : i64,
                           exposedCuCount = 64 : i64,
                           minTileBytes = 0 : i64,
                           muBlockCount = 64 : i64,
                           objective = "max_concurrency_comm_aware",
                           ownerDims = [0, 1],
                           requestedCuCount = 64 : i64,
                           targetLogicalWorkers = 64 : i64},
         physicalBlockShape = [64, 128, 16],
         physicalOwnerDims = [0, 1]}
      sde.yield
    }
    return
  }

  func.func @physical_only_owner_tile(%A: memref<512x512x16xf32>, %B: memref<512x512x16xf32>) {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c16 = arith.constant 16 : index
    %c512 = arith.constant 512 : index
    sde.cu_region <parallel> {
      sde.su_iterate (%c0, %c0) to (%c512, %c512) step (%c1, %c1) classification(<elementwise>) {
      ^bb0(%b: index, %c: index):
        scf.for %i = %c0 to %c16 step %c1 {
          %old = memref.load %A[%b, %c, %i] : memref<512x512x16xf32>
          %bias = memref.load %B[%b, %c, %i] : memref<512x512x16xf32>
          %next = arith.addf %old, %bias : f32
          memref.store %next, %A[%b, %c, %i] : memref<512x512x16xf32>
        }
        sde.yield
      } {iterationTopology = #sde.iteration_topology<owner_tile>,
         logicalWorkerSlice = [64, 128, 16],
         physicalBlockShape = [64, 128, 16],
         physicalOwnerDims = [0, 1]}
      sde.yield
    }
    return
  }
}
