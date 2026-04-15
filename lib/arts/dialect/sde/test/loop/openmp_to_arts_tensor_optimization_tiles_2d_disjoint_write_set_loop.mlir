// RUN: %carts-compile %s --O3 --arts-config %arts_config --pipeline openmp-to-arts --mlir-print-ir-after-all 2>&1 | %FileCheck %s --check-prefix=TENSOR
// RUN: %carts-compile %s --O3 --arts-config %arts_config --pipeline openmp-to-arts --mlir-print-ir-after-all 2>&1 | %FileCheck %s --check-prefix=OPT
// RUN: %carts-compile %s --O3 --arts-config %arts_config --pipeline openmp-to-arts --mlir-print-ir-after-all 2>&1 | %FileCheck %s --check-prefix=ARTS

// Verify that Tiling now tiles a narrow multi-dimensional
// elementwise carrier when the executable body writes a disjoint output set.
// The pass still rewrites only the outer SDE loop, sized from the active
// SDECostModel, and preserves the inner memref loop nest for later lowering.

// TENSOR-LABEL: // -----// IR Dump After RaiseToLinalg (raise-to-linalg) //----- //
// TENSOR: arts_sde.su_iterate (%c0) to (%c64) step (%c1) classification(<elementwise>) {
// TENSOR: arts_sde.cu_region <parallel>
// TENSOR: %[[IN:.+]] = arts_sde.mu_memref_to_tensor %arg0 : memref<64x64xf64> -> tensor<64x64xf64>
// TENSOR: %[[OUT0:.+]] = arts_sde.mu_memref_to_tensor %arg1 : memref<64x64xf64> -> tensor<64x64xf64>
// TENSOR: %[[OUT1:.+]] = arts_sde.mu_memref_to_tensor %arg2 : memref<64x64xf64> -> tensor<64x64xf64>
// TENSOR: linalg.generic
// TENSOR-SAME: ins(%[[IN]] : tensor<64x64xf64>)
// TENSOR-SAME: outs(%[[OUT0]], %[[OUT1]] : tensor<64x64xf64>, tensor<64x64xf64>)

// OPT-LABEL: // -----// IR Dump After Tiling (tiling) //----- //
// OPT: arts_sde.cu_region <parallel> {
// OPT: %c8 = arith.constant 8 : index
// OPT: %[[TSTEP:.+]] = arith.muli %c1, %c8 : index
// OPT: arts_sde.su_iterate (%c0) to (%c64) step (%[[TSTEP]]) classification(<elementwise>) {
// OPT: arts_sde.mu_memref_to_tensor %arg0 : memref<64x64xf64>
// OPT: arts_sde.mu_memref_to_tensor %arg1 : memref<64x64xf64>
// OPT: arts_sde.mu_memref_to_tensor %arg2 : memref<64x64xf64>
// OPT: tensor.extract_slice
// OPT: tensor.extract_slice
// OPT: tensor.extract_slice
// OPT: linalg.generic
// OPT: arith.mulf
// OPT: arith.addf
// OPT: linalg.yield
// OPT: tensor.insert_slice
// OPT: tensor.insert_slice
// OPT-NOT: bufferization.to_tensor

// ARTS-LABEL: // -----// IR Dump After ConvertSdeToArts (convert-sde-to-arts) //----- //
// ARTS: arts.for(%c0) to(%c64) step(%{{.*}})
// ARTS: memref.subview
// ARTS: scf.for
// ARTS-NOT: bufferization.to_tensor
// ARTS-NOT: arts_sde.mu_memref_to_tensor
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
