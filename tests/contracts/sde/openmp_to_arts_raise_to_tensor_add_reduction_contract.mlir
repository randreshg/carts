// RUN: %carts-compile %s --O3 --arts-config %S/../../examples/arts.cfg --pipeline openmp-to-arts --mlir-print-ir-after-all 2>&1 | %FileCheck %s --check-prefix=TENSOR
// RUN: %carts-compile %s --O3 --arts-config %S/../../examples/arts.cfg --pipeline edt-distribution | %FileCheck %s --check-prefix=LOWER

// Verify that RaiseToTensor preserves the new narrow reduction carrier as a
// read-modify-write tensor-backed linalg.generic. The reduction accumulator is
// still both an input and an output, so the pass must reuse
// bufferization.to_tensor instead of inventing tensor.empty. Also verify that
// the same SDE-selected atomic reduction contract survives through
// ConvertSdeToArts and is still consumed by the later edt-distribution lowering.

// TENSOR-LABEL: // -----// IR Dump After RaiseToTensor (raise-to-tensor) //----- //
// TENSOR: func.func @main
// TENSOR: arts_sde.su_iterate(%c0) to(%c128) step(%c1)
// TENSOR-SAME: reduction{{\[\[#arts_sde\.reduction_kind<add>\]\]}} (%{{.+}} : memref<?xi32>)
// TENSOR-SAME: reduction_strategy(<atomic>)
// TENSOR-SAME: classification(<reduction>) {
// TENSOR: %[[VALS:.+]] = bufferization.to_tensor %arg0 : memref<128xi32> to tensor<128xi32>
// TENSOR: %[[ACC:.+]] = bufferization.to_tensor %{{.+}} : memref<?xi32> to tensor<?xi32>
// TENSOR: linalg.generic
// TENSOR-SAME: ins(%[[VALS]], %[[ACC]] : tensor<128xi32>, tensor<?xi32>)
// TENSOR-SAME: outs(%[[ACC]] : tensor<?xi32>)
// TENSOR-NOT: tensor.empty

// LOWER-LABEL: func.func @main
// LOWER: arts.edt <parallel> <intranode> route(%c-1_i32)
// LOWER-SAME: attributes {arts.reduction_strategy = "atomic"
// LOWER: arts.edt <task> <intranode>
// LOWER: arts.atomic_add(%{{.+}}, %{{.+}} : memref<?xi32>, i32)
// LOWER-NOT: linalg.generic
// LOWER-NOT: bufferization.to_tensor

module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i32, dense<32> : vector<2xi64>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi64>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i64>>, llvm.data_layout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128", llvm.target_triple = "aarch64-unknown-linux-gnu"} {
  omp.declare_reduction @add_i32 : i32 init {
  ^bb0(%arg0: i32):
    %c0_i32 = arith.constant 0 : i32
    omp.yield(%c0_i32 : i32)
  } combiner {
  ^bb0(%lhs: i32, %rhs: i32):
    %sum = arith.addi %lhs, %rhs : i32
    omp.yield(%sum : i32)
  }

  func.func @main(%A: memref<128xi32>) {
    %c0 = arith.constant 0 : index
    %c128 = arith.constant 128 : index
    %c1 = arith.constant 1 : index
    %sum = memref.alloca() : memref<1xi32>
    %sum_cast = memref.cast %sum : memref<1xi32> to memref<?xi32>
    %c0_i32 = arith.constant 0 : i32
    memref.store %c0_i32, %sum[%c0] : memref<1xi32>
    omp.parallel {
      omp.wsloop reduction(@add_i32 %sum_cast -> %prv : memref<?xi32>) {
        omp.loop_nest (%i) : index = (%c0) to (%c128) step (%c1) {
          %val = memref.load %A[%i] : memref<128xi32>
          %acc = memref.load %prv[%c0] : memref<?xi32>
          %next = arith.addi %acc, %val : i32
          memref.store %next, %prv[%c0] : memref<?xi32>
          omp.yield
        }
      }
      omp.terminator
    }
    return
  }
}
