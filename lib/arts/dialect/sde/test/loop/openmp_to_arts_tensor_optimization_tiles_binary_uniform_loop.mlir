// RUN: %carts-compile %s --O3 --arts-config %arts_config --pipeline openmp-to-arts --mlir-print-ir-after-all 2>&1 | %FileCheck %s --check-prefix=TENSOR
// RUN: %carts-compile %s --O3 --arts-config %arts_config --pipeline openmp-to-arts --mlir-print-ir-after-all 2>&1 | %FileCheck %s --check-prefix=OPT
// RUN: %carts-compile %s --O3 --arts-config %arts_config --pipeline openmp-to-arts --mlir-print-ir-after-all 2>&1 | %FileCheck %s --check-prefix=ARTS

// Verify that tensor optimization now accepts a multi-input executable
// elementwise loop, not only the unary carrier subset.

// TENSOR-LABEL: // -----// IR Dump After RaiseToLinalg (raise-to-linalg) //----- //
// TENSOR: arts_sde.su_iterate (%c0) to (%c128) step (%c1) classification(<elementwise>) {
// TENSOR: arts_sde.cu_region <parallel>
// TENSOR: %[[A:.+]] = arts_sde.mu_memref_to_tensor %arg0 : memref<128xf64> -> tensor<128xf64>
// TENSOR: %[[B:.+]] = arts_sde.mu_memref_to_tensor %arg1 : memref<128xf64> -> tensor<128xf64>
// TENSOR: %[[OUT:.+]] = arts_sde.mu_memref_to_tensor %arg2 : memref<128xf64> -> tensor<128xf64>
// TENSOR: linalg.generic
// TENSOR-SAME: ins(%[[A]], %[[B]] : tensor<128xf64>, tensor<128xf64>)
// TENSOR-SAME: outs(%[[OUT]] : tensor<128xf64>)

// OPT-LABEL: // -----// IR Dump After Tiling (tiling) //----- //
// OPT: arts_sde.cu_region <parallel> {
// OPT: %c16 = arith.constant 16 : index
// OPT: %[[TSTEP:.+]] = arith.muli %c1, %c16 : index
// OPT: arts_sde.su_iterate (%c0) to (%c128) step (%[[TSTEP]]) classification(<elementwise>) {
// OPT: arts_sde.mu_memref_to_tensor %arg0 : memref<128xf64>
// OPT: arts_sde.mu_memref_to_tensor %arg1 : memref<128xf64>
// OPT: arts_sde.mu_memref_to_tensor %arg2 : memref<128xf64>
// OPT: tensor.extract_slice
// OPT: tensor.extract_slice
// OPT: tensor.extract_slice
// OPT: linalg.generic
// OPT: arith.addf
// OPT: linalg.yield
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
  func.func @main(%A: memref<128xf64>, %B: memref<128xf64>, %C: memref<128xf64>) {
    %c0 = arith.constant 0 : index
    %c128 = arith.constant 128 : index
    %c1 = arith.constant 1 : index
    omp.parallel {
      omp.wsloop {
        omp.loop_nest (%i) : index = (%c0) to (%c128) step (%c1) {
          %lhs = memref.load %A[%i] : memref<128xf64>
          %rhs = memref.load %B[%i] : memref<128xf64>
          %sum = arith.addf %lhs, %rhs : f64
          memref.store %sum, %C[%i] : memref<128xf64>
          omp.yield
        }
      }
      omp.terminator
    }
    return
  }
}
