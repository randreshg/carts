// RUN: %carts-compile %s --O3 --arts-config %S/../../examples/arts.cfg --pipeline openmp-to-arts --mlir-print-ir-after-all 2>&1 | %FileCheck %s --check-prefix=TENSOR
// RUN: %carts-compile %s --O3 --arts-config %S/../../examples/arts.cfg --pipeline openmp-to-arts --mlir-print-ir-after-all 2>&1 | %FileCheck %s --check-prefix=OPT

// Verify that SdeTensorOptimization now accepts a narrow matmul carrier and
// rewrites the authoritative SDE loop with a cost-model-driven outer tile on
// the leading parallel dimension while dropping transient tensor carriers from
// the executable body.

// TENSOR-LABEL: // -----// IR Dump After RaiseToTensor (raise-to-tensor) //----- //
// TENSOR: arts_sde.su_iterate(%c0) to(%c32) step(%c1) classification(<matmul>) {
// TENSOR: %[[A:.+]] = bufferization.to_tensor %arg0 : memref<32x32xf32> to tensor<32x32xf32>
// TENSOR: %[[B:.+]] = bufferization.to_tensor %arg1 : memref<32x32xf32> to tensor<32x32xf32>
// TENSOR: %[[C:.+]] = bufferization.to_tensor %arg2 : memref<32x32xf32> to tensor<32x32xf32>
// TENSOR: linalg.generic
// TENSOR-SAME: ins(%[[A]], %[[B]], %[[C]] : tensor<32x32xf32>, tensor<32x32xf32>, tensor<32x32xf32>)
// TENSOR-SAME: outs(%[[C]] : tensor<32x32xf32>)

// OPT-LABEL: // -----// IR Dump After SdeTensorOptimization (sde-tensor-optimization) //----- //
// OPT: %c4 = arith.constant 4 : index
// OPT: %[[TSTEP:.+]] = arith.muli %c1, %c4 : index
// OPT: arts_sde.su_iterate(%c0) to(%c32) step(%[[TSTEP]]) classification(<matmul>) {
// OPT: ^bb0(%[[BASE:.+]]: index):
// OPT: %[[LIMIT:.+]] = arith.addi %[[BASE]], %[[TSTEP]] : index
// OPT: %[[TILE_UB:.+]] = arith.minui %[[LIMIT]], %c32 : index
// OPT: scf.for %[[I:.+]] = %[[BASE]] to %[[TILE_UB]] step %c1 {
// OPT: scf.for %[[J:.+]] = %c0 to %c32 step %c1 {
// OPT: scf.for %[[K:.+]] = %c0 to %c32 step %c1 {
// OPT: memref.load %arg0[%[[I]], %[[K]]] : memref<32x32xf32>
// OPT: memref.load %arg1[%[[K]], %[[J]]] : memref<32x32xf32>
// OPT: memref.load %arg2[%[[I]], %[[J]]] : memref<32x32xf32>
// OPT: arith.mulf
// OPT: arith.addf
// OPT: memref.store {{.+}}, %arg2[%[[I]], %[[J]]] : memref<32x32xf32>
// OPT-NOT: bufferization.to_tensor
// OPT-NOT: tensor.empty
// OPT-NOT: linalg.generic

module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f32, dense<32> : vector<2xi64>>, #dlti.dl_entry<i64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i32, dense<32> : vector<2xi64>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi64>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i64>>, llvm.data_layout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128", llvm.target_triple = "aarch64-unknown-linux-gnu"} {
  func.func @main(%A: memref<32x32xf32>, %B: memref<32x32xf32>, %C: memref<32x32xf32>) {
    %c0 = arith.constant 0 : index
    %c32 = arith.constant 32 : index
    %c1 = arith.constant 1 : index
    omp.parallel {
      omp.wsloop {
        omp.loop_nest (%i) : index = (%c0) to (%c32) step (%c1) {
          scf.for %j = %c0 to %c32 step %c1 {
            scf.for %k = %c0 to %c32 step %c1 {
              %a = memref.load %A[%i, %k] : memref<32x32xf32>
              %b = memref.load %B[%k, %j] : memref<32x32xf32>
              %c = memref.load %C[%i, %j] : memref<32x32xf32>
              %prod = arith.mulf %a, %b : f32
              %sum = arith.addf %c, %prod : f32
              memref.store %sum, %C[%i, %j] : memref<32x32xf32>
            }
          }
          omp.yield
        }
      }
      omp.terminator
    }
    return
  }
}
