// RUN: %carts-compile %s --O3 --arts-config %S/../../examples/arts.cfg --pipeline openmp-to-arts --mlir-print-ir-after-all 2>&1 | %FileCheck %s --check-prefix=TENSOR
// RUN: %carts-compile %s --O3 --arts-config %S/../../examples/arts.cfg --pipeline openmp-to-arts --mlir-print-ir-after-all 2>&1 | %FileCheck %s --check-prefix=OPT
// RUN: %carts-compile %s --O3 --arts-config %S/../../examples/arts.cfg --pipeline openmp-to-arts --mlir-print-ir-after-all 2>&1 | %FileCheck %s --check-prefix=ARTS

// Verify that executable SDE tensor tiling also handles write-only elementwise
// loops. RaiseToTensor materializes a tensor-backed carrier with no inputs,
// SdeTensorOptimization rewrites the surrounding sde.su_iterate into a tiled
// loop nest, and the tiled shape survives into arts.for lowering.

// TENSOR-LABEL: // -----// IR Dump After RaiseToTensor (raise-to-tensor) //----- //
// TENSOR: arts_sde.su_iterate(%c0) to(%c128) step(%c1) classification(<elementwise>) {
// TENSOR: %[[OUT:.+]] = tensor.empty() : tensor<128xf64>
// TENSOR: linalg.generic
// TENSOR-SAME: outs(%[[OUT]] : tensor<128xf64>)

// OPT-LABEL: // -----// IR Dump After SdeTensorOptimization (sde-tensor-optimization) //----- //
// OPT: %c16 = arith.constant 16 : index
// OPT: %[[TSTEP:.+]] = arith.muli %c1, %c16 : index
// OPT: arts_sde.su_iterate(%c0) to(%c128) step(%[[TSTEP]]) classification(<elementwise>) {
// OPT: ^bb0(%[[TILE:.+]]: index):
// OPT: %[[LIMIT:.+]] = arith.addi %[[TILE]], %[[TSTEP]] : index
// OPT: %[[TILE_UB:.+]] = arith.minui %[[LIMIT]], %c128 : index
// OPT: scf.for %[[IV:.+]] = %[[TILE]] to %[[TILE_UB]] step %c1 {
// OPT: memref.store %cst, %arg0[%[[IV]]] : memref<128xf64>
// OPT-NOT: bufferization.to_tensor
// OPT-NOT: tensor.empty
// OPT-NOT: linalg.generic

// ARTS-LABEL: // -----// IR Dump After ConvertSdeToArts (convert-sde-to-arts) //----- //
// ARTS: arts.for(%c0) to(%c128) step(%{{.*}})
// ARTS: scf.for
// ARTS-NOT: bufferization.to_tensor
// ARTS-NOT: tensor.empty
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
