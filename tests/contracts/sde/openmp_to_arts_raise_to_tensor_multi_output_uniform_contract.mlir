// RUN: %carts-compile %s --O3 --arts-config %S/../../examples/arts.cfg --pipeline openmp-to-arts --mlir-print-ir-after-all 2>&1 | %FileCheck %s --check-prefix=LINALG
// RUN: %carts-compile %s --O3 --arts-config %S/../../examples/arts.cfg --pipeline openmp-to-arts --mlir-print-ir-after-all 2>&1 | %FileCheck %s --check-prefix=TENSOR
// RUN: %carts-compile %s --O3 --arts-config %S/../../examples/arts.cfg --pipeline openmp-to-arts --mlir-print-ir-after-all 2>&1 | %FileCheck %s --check-prefix=ARTS

// Verify the SDE tensor carrier flow for a raiseable multi-output elementwise
// loop: RaiseToLinalg first materializes the transient memref-backed
// linalg.generic, RaiseToTensor rewrites that carrier into tensor-backed form
// with one tensor destination per output, and ConvertSdeToArts consumes the
// carrier chain so downstream ARTS passes keep seeing only loop/memref IR.

// LINALG-LABEL: // -----// IR Dump After RaiseToLinalg (raise-to-linalg) //----- //
// LINALG: func.func @main
// LINALG: arts_sde.su_iterate(%c0) to(%c128) step(%c1) classification(<elementwise>) {
// LINALG: linalg.generic
// LINALG-SAME: ins(%arg0 : memref<128xf64>)
// LINALG-SAME: outs(%arg1, %arg2 : memref<128xf64>, memref<128xf64>)

// TENSOR-LABEL: // -----// IR Dump After RaiseToTensor (raise-to-tensor) //----- //
// TENSOR: func.func @main
// TENSOR: arts_sde.su_iterate(%c0) to(%c128) step(%c1) classification(<elementwise>) {
// TENSOR: %[[IN:.+]] = bufferization.to_tensor %arg0 : memref<128xf64> to tensor<128xf64>
// TENSOR: %[[OUT0:.+]] = tensor.empty() : tensor<128xf64>
// TENSOR: %[[OUT1:.+]] = tensor.empty() : tensor<128xf64>
// TENSOR: linalg.generic
// TENSOR-SAME: ins(%[[IN]] : tensor<128xf64>)
// TENSOR-SAME: outs(%[[OUT0]], %[[OUT1]] : tensor<128xf64>, tensor<128xf64>)
// TENSOR-NOT: outs(%arg1, %arg2 : memref<128xf64>, memref<128xf64>)

// ARTS-LABEL: // -----// IR Dump After ConvertSdeToArts (convert-sde-to-arts) //----- //
// ARTS: func.func @main
// ARTS: arts.for
// ARTS-NOT: bufferization.to_tensor
// ARTS-NOT: tensor.empty
// ARTS-NOT: linalg.generic

module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i32, dense<32> : vector<2xi64>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi64>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i64>>, llvm.data_layout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128", llvm.target_triple = "aarch64-unknown-linux-gnu"} {
  func.func @main(%A: memref<128xf64>, %B: memref<128xf64>, %C: memref<128xf64>) {
    %c0 = arith.constant 0 : index
    %c128 = arith.constant 128 : index
    %c1 = arith.constant 1 : index
    %cst = arith.constant 2.0 : f64
    omp.parallel {
      omp.wsloop {
        omp.loop_nest (%i) : index = (%c0) to (%c128) step (%c1) {
          %val = memref.load %A[%i] : memref<128xf64>
          %scale = arith.mulf %val, %cst : f64
          %shift = arith.addf %val, %cst : f64
          memref.store %scale, %B[%i] : memref<128xf64>
          memref.store %shift, %C[%i] : memref<128xf64>
          omp.yield
        }
      }
      omp.terminator
    }
    return
  }
}
