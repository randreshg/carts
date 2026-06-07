// RUN: %carts-compile %s --O3 --arts-config %inputs_dir/arts_multinode.cfg \
// RUN:   --start-from sde-planning --pipeline sde-to-codir \
// RUN:   --min-distributed-tile-bytes=1048576 \
// RUN:   --mlir-print-ir-after-all 2>&1 \
// RUN:   | awk '/IR Dump After DistributionPlanning/,/IR Dump After IterationSpaceDecomposition/' \
// RUN:   | %FileCheck %s

// DistributionPlanning must add CU/MU partition evidence to an already
// realized guarded stencil owner tile. The nested element loops are the shape
// Tiling creates for out-of-place boundary-guarded stencils; the committed
// outer SU step and physical block facts remain the authority.

// CHECK-LABEL: // -----// IR Dump After DistributionPlanning (distribution-planning) //----- //
// CHECK: func.func @realized_guarded_budget_stencil
// CHECK: sde.su_iterate (%c0, %c0) to (%c64, %c64) step (%c8{{[^,]*}}, %c16{{[^)]*}}) classification(<stencil>) {
// CHECK: scf.for
// CHECK: scf.if
// CHECK: } {
// CHECK-SAME: logicalWorkerSlice = [8, 16]
// CHECK-SAME: partitionGraph = [
// CHECK-SAME: blockShape = [8, 16]
// CHECK-SAME: layoutKind = "owner_block"
// CHECK-SAME: muBlockCount = 32 : i64
// CHECK-SAME: partitionScore = {
// CHECK-SAME: blockShape = [8, 16]
// CHECK-SAME: chosenCuCount = 32 : i64
// CHECK-SAME: muBlockCount = 32 : i64
// CHECK-SAME: physicalBlockShape = [8, 16]
// CHECK-SAME: physicalHaloShape = [1, 1]
// CHECK-SAME: physicalOwnerDims = [0, 1]
// CHECK-NOT: all_gather
// CHECK-NOT: reduce_scatter
// CHECK-NOT: allreduce
// CHECK-NOT: broadcast

module attributes {
  dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i32, dense<32> : vector<2xi64>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi64>>, #dlti.dl_entry<"dlti.endianness", "little">>,
  llvm.data_layout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128",
  llvm.target_triple = "aarch64-unknown-linux-gnu"
} {
  func.func @realized_guarded_budget_stencil(%A: memref<64x64xf64>, %B: memref<64x64xf64>) {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c8 = arith.constant 8 : index
    %c16 = arith.constant 16 : index
    %c63 = arith.constant 63 : index
    %c64 = arith.constant 64 : index
    sde.cu_region <parallel> {
      sde.su_iterate (%c0, %c0) to (%c64, %c64) step (%c8, %c16) classification(<stencil>) {
      ^bb0(%tile_i: index, %tile_j: index):
        %tile_i_end_raw = arith.addi %tile_i, %c8 : index
        %tile_i_end = arith.minui %tile_i_end_raw, %c64 : index
        scf.for %i = %tile_i to %tile_i_end step %c1 {
          %tile_j_end_raw = arith.addi %tile_j, %c16 : index
          %tile_j_end = arith.minui %tile_j_end_raw, %c64 : index
          scf.for %j = %tile_j to %tile_j_end step %c1 {
            %first_i = arith.cmpi eq, %i, %c0 : index
            %last_i = arith.cmpi eq, %i, %c63 : index
            %edge_i = arith.ori %first_i, %last_i : i1
            %first_j = arith.cmpi eq, %j, %c0 : index
            %last_j = arith.cmpi eq, %j, %c63 : index
            %edge_j = arith.ori %first_j, %last_j : i1
            %edge = arith.ori %edge_i, %edge_j : i1
            scf.if %edge {
              %copy = memref.load %A[%i, %j] : memref<64x64xf64>
              memref.store %copy, %B[%i, %j] : memref<64x64xf64>
            } else {
              %im1 = arith.subi %i, %c1 : index
              %ip1 = arith.addi %i, %c1 : index
              %jm1 = arith.subi %j, %c1 : index
              %jp1 = arith.addi %j, %c1 : index
              %n = memref.load %A[%im1, %j] : memref<64x64xf64>
              %s = memref.load %A[%ip1, %j] : memref<64x64xf64>
              %w = memref.load %A[%i, %jm1] : memref<64x64xf64>
              %e = memref.load %A[%i, %jp1] : memref<64x64xf64>
              %sum0 = arith.addf %n, %s : f64
              %sum1 = arith.addf %w, %e : f64
              %sum = arith.addf %sum0, %sum1 : f64
              memref.store %sum, %B[%i, %j] : memref<64x64xf64>
            }
          }
        }
        sde.yield
      } {accessMaxOffsets = [1, 1],
         accessMinOffsets = [-1, -1],
         iterationTopology = #sde.iteration_topology<owner_tile>,
         logicalWorkerSlice = [8, 16],
         ownerDims = [0, 1],
         pattern = #sde.pattern<stencil_tiling_nd>,
         physicalBlockShape = [8, 16],
         physicalHaloShape = [1, 1],
         physicalOwnerDims = [0, 1],
         spatialDims = [0, 1],
         writeFootprint = [1, 1]}
      sde.yield
    }
    return
  }
}
