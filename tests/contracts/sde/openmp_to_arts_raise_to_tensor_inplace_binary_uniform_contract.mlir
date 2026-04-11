// RUN: %carts-compile %s --O3 --arts-config %S/../../examples/arts.cfg --pipeline openmp-to-arts --mlir-print-ir-after-all 2>&1 | %FileCheck %s --check-prefix=LINALG
// RUN: %carts-compile %s --O3 --arts-config %S/../../examples/arts.cfg --pipeline openmp-to-arts --mlir-print-ir-after-all 2>&1 | %FileCheck %s --check-prefix=TENSOR

// Verify the SDE tensor carrier flow for an in-place binary elementwise loop:
// RaiseToLinalg first materializes the transient memref-backed linalg.generic
// with the destination in both ins() and outs(), and RaiseToTensor preserves
// that read-modify-write contract by reusing bufferization.to_tensor for the
// destination output instead of switching to tensor.empty.

// LINALG-LABEL: // -----// IR Dump After RaiseToLinalg (raise-to-linalg) //----- //
// LINALG: func.func @main
// LINALG: arts_sde.su_iterate(%c0) to(%c128) step(%c1) classification(<elementwise>) {
// LINALG: linalg.generic
// LINALG-SAME: ins(%arg0, %arg1 : memref<128xf64>, memref<128xf64>)
// LINALG-SAME: outs(%arg1 : memref<128xf64>)

// TENSOR-LABEL: // -----// IR Dump After RaiseToTensor (raise-to-tensor) //----- //
// TENSOR: func.func @main
// TENSOR: arts_sde.su_iterate(%c0) to(%c128) step(%c1) classification(<elementwise>) {
// TENSOR: %[[A:.+]] = bufferization.to_tensor %arg0 : memref<128xf64> to tensor<128xf64>
// TENSOR: %[[B:.+]] = bufferization.to_tensor %arg1 : memref<128xf64> to tensor<128xf64>
// TENSOR: linalg.generic
// TENSOR-SAME: ins(%[[A]], %[[B]] : tensor<128xf64>, tensor<128xf64>)
// TENSOR-SAME: outs(%[[B]] : tensor<128xf64>)
// TENSOR-NOT: tensor.empty

module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i32, dense<32> : vector<2xi64>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi64>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i64>>, llvm.data_layout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128", llvm.target_triple = "aarch64-unknown-linux-gnu"} {
  func.func @main(%A: memref<128xf64>, %B: memref<128xf64>) {
    %c0 = arith.constant 0 : index
    %c128 = arith.constant 128 : index
    %c1 = arith.constant 1 : index
    omp.parallel {
      omp.wsloop {
        omp.loop_nest (%i) : index = (%c0) to (%c128) step (%c1) {
          %a = memref.load %A[%i] : memref<128xf64>
          %b = memref.load %B[%i] : memref<128xf64>
          %sum = arith.addf %b, %a : f64
          memref.store %sum, %B[%i] : memref<128xf64>
          omp.yield
        }
      }
      omp.terminator
    }
    return
  }
}
