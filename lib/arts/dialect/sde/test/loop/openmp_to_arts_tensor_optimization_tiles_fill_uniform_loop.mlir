// RUN: %carts-compile %s --O3 --arts-config %arts_config --pipeline openmp-to-arts --mlir-print-ir-after-all 2>&1 | %FileCheck %s --check-prefix=TENSOR
// RUN: %carts-compile %s --O3 --arts-config %arts_config --pipeline openmp-to-arts --mlir-print-ir-after-all 2>&1 | %FileCheck %s --check-prefix=OPT
// RUN: %carts-compile %s --O3 --arts-config %arts_config --pipeline openmp-to-arts --mlir-print-ir-after-all 2>&1 | %FileCheck %s --check-prefix=ARTS

// Verify that executable SDE tensor tiling also handles write-only elementwise
// loops. RaiseToTensor materializes a tensor-backed carrier with no inputs,
// Tiling rewrites the surrounding sde.su_iterate into a tiled
// loop nest, and the tiled shape survives into arts.for lowering.

// TENSOR-LABEL: // -----// IR Dump After RaiseToLinalg (raise-to-linalg) //----- //
// TENSOR: arts_sde.su_iterate (%c0) to (%c128) step (%c1) classification(<elementwise>) {
// TENSOR: arts_sde.cu_region <parallel>
// TENSOR: %[[OUT:.+]] = arts_sde.mu_memref_to_tensor %arg0 : memref<128xf64> -> tensor<128xf64>
// TENSOR: linalg.generic
// TENSOR-SAME: outs(%[[OUT]] : tensor<128xf64>)

// OPT-LABEL: // -----// IR Dump After Tiling (tiling) //----- //
// OPT: arts_sde.cu_region <parallel> {
// OPT: %c16 = arith.constant 16 : index
// OPT: %[[TSTEP:.+]] = arith.muli %c1, %c16 : index
// OPT: arts_sde.su_iterate (%c0) to (%c128) step (%[[TSTEP]]) classification(<elementwise>) {
// OPT: arts_sde.mu_memref_to_tensor %arg0 : memref<128xf64>
// OPT: tensor.extract_slice
// OPT: linalg.generic
// OPT: linalg.yield %cst
// OPT: tensor.insert_slice
// OPT-NOT: bufferization.to_tensor

// ARTS-LABEL: // -----// IR Dump After ConvertSdeToArts (convert-sde-to-arts) //----- //
// ARTS: arts.for(%c0) to(%c128) step(%{{.*}})
// ARTS: memref.subview
// ARTS: scf.for
// ARTS-NOT: bufferization.to_tensor
// ARTS-NOT: arts_sde.mu_memref_to_tensor
// ARTS-NOT: linalg.generic

module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i32, dense<32> : vector<2xi64>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi64>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i64>>, llvm.data_layout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128", llvm.target_triple = "aarch64-unknown-linux-gnu"} {
  func.func @main(%B: memref<128xf64>) {
    %c0 = arith.constant 0 : index
    %c128 = arith.constant 128 : index
    %c1 = arith.constant 1 : index
    %cst = arith.constant 2.0 : f64
    omp.parallel {
      omp.wsloop {
        omp.loop_nest (%i) : index = (%c0) to (%c128) step (%c1) {
          memref.store %cst, %B[%i] : memref<128xf64>
          omp.yield
        }
      }
      omp.terminator
    }
    return
  }
}
