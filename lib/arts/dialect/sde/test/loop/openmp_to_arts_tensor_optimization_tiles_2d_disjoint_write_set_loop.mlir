// RUN: %carts-compile %s --O3 --arts-config %arts_config --pipeline openmp-to-arts --mlir-print-ir-after-all 2>&1 | %FileCheck %s --check-prefix=TENSOR
// RUN: %carts-compile %s --O3 --arts-config %arts_config --pipeline openmp-to-arts --mlir-print-ir-after-all 2>&1 | %FileCheck %s --check-prefix=OPT
// RUN: %carts-compile %s --O3 --arts-config %arts_config --pipeline openmp-to-arts --mlir-print-ir-after-all 2>&1 | %FileCheck %s --check-prefix=ARTS

// Verify that TensorOpt now tiles a narrow multi-dimensional
// elementwise carrier when the executable body writes a disjoint output set.
// The pass still rewrites only the outer SDE loop, sized from the active
// SDECostModel, and preserves the inner memref loop nest for later lowering.

// TENSOR-LABEL: // -----// IR Dump After RaiseToTensor (raise-to-tensor) //----- //
// TENSOR: arts_sde.su_iterate (%c0) to (%c64) step (%c1) classification(<elementwise>) {
// TENSOR: arts_sde.cu_region <parallel>
// TENSOR: %[[IN:.+]] = bufferization.to_tensor %arg0 : memref<64x64xf64> to tensor<64x64xf64>
// TENSOR: %[[OUT0:.+]] = bufferization.to_tensor %arg1 : memref<64x64xf64> to tensor<64x64xf64>
// TENSOR: %[[OUT1:.+]] = bufferization.to_tensor %arg2 : memref<64x64xf64> to tensor<64x64xf64>
// TENSOR: linalg.generic
// TENSOR-SAME: ins(%[[IN]] : tensor<64x64xf64>)
// TENSOR-SAME: outs(%[[OUT0]], %[[OUT1]] : tensor<64x64xf64>, tensor<64x64xf64>)

// OPT-LABEL: // -----// IR Dump After TensorOpt (tensor-opt) //----- //
// OPT: arts_sde.cu_region <parallel> {
// OPT: %c8 = arith.constant 8 : index
// OPT: %[[TSTEP:.+]] = arith.muli %c1, %c8 : index
// OPT: arts_sde.su_iterate (%c0) to (%c64) step (%[[TSTEP]]) classification(<elementwise>) {
// OPT: %[[LIMIT:.+]] = arith.addi %arg3, %[[TSTEP]] : index
// OPT: %[[TILE_UB:.+]] = arith.minui %[[LIMIT]], %c64 : index
// OPT: scf.for %[[I:.+]] = %arg3 to %[[TILE_UB]] step %c1 {
// OPT: scf.for %[[J:.+]] = %c0 to %c64 step %c1 {
// OPT: %[[VAL:.+]] = memref.load %arg0[%[[I]], %[[J]]] : memref<64x64xf64>
// OPT: %[[SCALE:.+]] = arith.mulf %[[VAL]], %cst : f64
// OPT: %[[SHIFT:.+]] = arith.addf %[[VAL]], %cst : f64
// OPT: memref.store %[[SCALE]], %arg1[%[[I]], %[[J]]] : memref<64x64xf64>
// OPT: memref.store %[[SHIFT]], %arg2[%[[I]], %[[J]]] : memref<64x64xf64>
// OPT-NOT: bufferization.to_tensor
// OPT-NOT: tensor.empty
// OPT-NOT: linalg.generic

// ARTS-LABEL: // -----// IR Dump After ConvertSdeToArts (convert-sde-to-arts) //----- //
// ARTS: arts.for(%c0) to(%c64) step(%{{.*}})
// ARTS: scf.for
// ARTS-NOT: bufferization.to_tensor
// ARTS-NOT: tensor.empty
// ARTS-NOT: linalg.generic

module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i32, dense<32> : vector<2xi64>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi64>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i64>>, llvm.data_layout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128", llvm.target_triple = "aarch64-unknown-linux-gnu"} {
  func.func @main(%A: memref<64x64xf64>, %B: memref<64x64xf64>, %C: memref<64x64xf64>) {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c64 = arith.constant 64 : index
    %cst = arith.constant 2.0 : f64
    omp.parallel {
      omp.wsloop {
        omp.loop_nest (%i) : index = (%c0) to (%c64) step (%c1) {
          scf.for %j = %c0 to %c64 step %c1 {
            %val = memref.load %A[%i, %j] : memref<64x64xf64>
            %scale = arith.mulf %val, %cst : f64
            %shift = arith.addf %val, %cst : f64
            memref.store %scale, %B[%i, %j] : memref<64x64xf64>
            memref.store %shift, %C[%i, %j] : memref<64x64xf64>
          }
          omp.yield
        }
      }
      omp.terminator
    }
    return
  }
}
