// RUN: %carts-compile %s --O3 --arts-config %inputs_dir/arts_64t.cfg \
// RUN:   --start-from sde-planning --pipeline sde-to-codir \
// RUN:   --mlir-print-ir-after-all 2>&1 | %FileCheck %s

// An explicit stencil contract is not a committed physical plan. Pattern
// analysis must still promote the nested owner loop into a real 2-D SDE
// scheduling unit before tiling stamps owner-tile facts.

// CHECK-LABEL: // -----// IR Dump After PatternAnalysis (sde-pattern-analysis) //----- //
// CHECK: func.func @nested_explicit_jacobi
// CHECK: sde.su_iterate (%c1, %c1) to (%c255, %c255) step
// CHECK-SAME: classification(<stencil>)
// CHECK: } {accessMaxOffsets = [1, 1]
// CHECK-SAME: ownerDims = [0, 1]
// CHECK-SAME: pattern = #sde.pattern<stencil_tiling_nd>
// CHECK-LABEL: // -----// IR Dump After Tiling (tiling) //----- //
// CHECK: func.func @nested_explicit_jacobi
// CHECK: sde.su_iterate
// CHECK-SAME: classification(<stencil>)
// CHECK: } {accessMaxOffsets
// CHECK-SAME: iterationTopology = #sde.iteration_topology<owner_tile>
// CHECK-SAME: logicalWorkerSlice = [32, 32]
// CHECK-SAME: physicalBlockShape = [32, 32]
// CHECK-SAME: physicalHaloShape = [1, 1]
// CHECK-SAME: physicalOwnerDims = [0, 1]
// CHECK-LABEL: // -----// IR Dump After ConvertSdeToCodir (convert-sde-to-codir) //----- //
// CHECK: func.func @nested_explicit_jacobi
// CHECK: codir.codelet
// CHECK-SAME: tile_owner_dims = [0, 1]
// CHECK-SAME: tile_shape = [32, 32]

module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i1, dense<8> : vector<2xi64>>, #dlti.dl_entry<i64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i32, dense<32> : vector<2xi64>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi64>>, #dlti.dl_entry<"dlti.endianness", "little">>, llvm.data_layout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128", llvm.target_triple = "aarch64-unknown-linux-gnu"} {
  func.func @nested_explicit_jacobi(%A: memref<256x256xf64>, %B: memref<256x256xf64>, %flag: memref<i1>) {
    %true = arith.constant true
    %c1 = arith.constant 1 : index
    %c255 = arith.constant 255 : index
    sde.cu_region <parallel> {
      sde.su_iterate (%c1) to (%c255) step (%c1) classification(<stencil>) {
      ^bb0(%i: index):
        memref.store %true, %flag[] : memref<i1>
        scf.for %j = %c1 to %c255 step %c1 {
          %im1 = arith.subi %i, %c1 : index
          %ip1 = arith.addi %i, %c1 : index
          %jm1 = arith.subi %j, %c1 : index
          %jp1 = arith.addi %j, %c1 : index
          %x0 = memref.load %A[%im1, %j] : memref<256x256xf64>
          %x1 = memref.load %A[%ip1, %j] : memref<256x256xf64>
          %y0 = memref.load %A[%i, %jm1] : memref<256x256xf64>
          %y1 = memref.load %A[%i, %jp1] : memref<256x256xf64>
          %s0 = arith.addf %x0, %x1 : f64
          %s1 = arith.addf %y0, %y1 : f64
          %sum = arith.addf %s0, %s1 : f64
          memref.store %sum, %B[%i, %j] : memref<256x256xf64>
        }
        sde.yield
      } {accessMaxOffsets = [1, 1],
         accessMinOffsets = [-1, -1],
         ownerDims = [0, 1],
         pattern = #sde.pattern<stencil_tiling_nd>,
         spatialDims = [0, 1],
         writeFootprint = [1, 1]}
      sde.yield
    }
    return
  }
}
