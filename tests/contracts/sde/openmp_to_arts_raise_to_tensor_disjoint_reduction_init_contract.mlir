// RUN: %carts-compile %s --O3 --arts-config %S/../../examples/arts.cfg --start-from openmp-to-arts --pipeline openmp-to-arts --mlir-print-ir-after-all 2>&1 | %FileCheck %s

// Verify the narrow disjoint-output generalization in RaiseToTensor: a
// reduction-iterator carrier may still use tensor.empty when the output value
// is never read in the generic body and no other operand aliases that output.

// CHECK-LABEL: // -----// IR Dump After RaiseToTensor (raise-to-tensor) //----- //
// CHECK: func.func @main
// CHECK: arts_sde.su_iterate(%c0) to(%c8) step(%c1) classification(<reduction>) {
// CHECK: %[[IN:.+]] = bufferization.to_tensor %arg0 : memref<8x4xf32> to tensor<8x4xf32>
// CHECK: %[[OUT:.+]] = tensor.empty() : tensor<8xf32>
// CHECK: linalg.generic
// CHECK-SAME: ins(%[[IN]] : tensor<8x4xf32>)
// CHECK-SAME: outs(%[[OUT]] : tensor<8xf32>)
// CHECK-NOT: bufferization.to_tensor %arg1 : memref<8xf32> to tensor<8xf32>

module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f32, dense<32> : vector<2xi64>>, #dlti.dl_entry<i64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i32, dense<32> : vector<2xi64>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi64>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i64>>, llvm.data_layout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128", llvm.target_triple = "aarch64-unknown-linux-gnu"} {
  func.func @main(%A: memref<8x4xf32>, %B: memref<8xf32>) {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index
    %c8 = arith.constant 8 : index
    arts_sde.cu_region <parallel> scope(<local>) {
      arts_sde.su_iterate(%c0) to(%c8) step(%c1) classification(<reduction>) {
      ^bb0(%i: index):
        scf.for %k = %c0 to %c4 step %c1 {
          %a = memref.load %A[%i, %k] : memref<8x4xf32>
          memref.store %a, %B[%i] : memref<8xf32>
        }

        linalg.generic
            {indexing_maps = [affine_map<(d0, d1) -> (d0, d1)>,
                              affine_map<(d0, d1) -> (d0)>],
             iterator_types = ["parallel", "reduction"]}
            ins(%A : memref<8x4xf32>)
            outs(%B : memref<8xf32>) {
          ^bb0(%in: f32, %out: f32):
            linalg.yield %in : f32
        }

        arts_sde.yield
      }
      arts_sde.yield
    }
    return
  }
}
