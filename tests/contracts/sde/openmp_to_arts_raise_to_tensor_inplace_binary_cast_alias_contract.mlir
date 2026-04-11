// RUN: %carts-compile %s --O3 --arts-config %S/../../examples/arts.cfg --start-from openmp-to-arts --pipeline openmp-to-arts --mlir-print-ir-after-all 2>&1 | %FileCheck %s

// Verify the narrow alias-through-wrapper case for RaiseToTensor directly at
// the SDE boundary: if a memref-backed carrier reads the destination through a
// memref.cast wrapper but writes back to the base memref, SDE must still treat
// that output as read-modify-write and avoid replacing it with tensor.empty.

// CHECK-LABEL: // -----// IR Dump After RaiseToTensor (raise-to-tensor) //----- //
// CHECK: func.func @main
// CHECK: %[[BCAST:.+]] = memref.cast %arg1 : memref<16xf64> to memref<?xf64>
// CHECK: arts_sde.su_iterate(%c0) to(%c16) step(%c1) classification(<elementwise>) {
// CHECK: %[[A:.+]] = bufferization.to_tensor %arg0 : memref<16xf64> to tensor<16xf64>
// CHECK: %[[BREAD:.+]] = bufferization.to_tensor %[[BCAST]] : memref<?xf64> to tensor<?xf64>
// CHECK: %[[BWRITE:.+]] = bufferization.to_tensor %arg1 : memref<16xf64> to tensor<16xf64>
// CHECK: linalg.generic
// CHECK-SAME: ins(%[[A]], %[[BREAD]] : tensor<16xf64>, tensor<?xf64>)
// CHECK-SAME: outs(%[[BWRITE]] : tensor<16xf64>)
// CHECK-NOT: tensor.empty

module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i32, dense<32> : vector<2xi64>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi64>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i64>>, llvm.data_layout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128", llvm.target_triple = "aarch64-unknown-linux-gnu"} {
  func.func @main(%A: memref<16xf64>, %B: memref<16xf64>) {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c16 = arith.constant 16 : index
    %Bcast = memref.cast %B : memref<16xf64> to memref<?xf64>
    arts_sde.cu_region <parallel> scope(<local>) {
      arts_sde.su_iterate(%c0) to(%c16) step(%c1) classification(<elementwise>) {
      ^bb0(%iv: index):
        %a = memref.load %A[%iv] : memref<16xf64>
        %b = memref.load %B[%iv] : memref<16xf64>
        %sum = arith.addf %b, %a : f64
        memref.store %sum, %B[%iv] : memref<16xf64>

        linalg.generic
            {indexing_maps = [affine_map<(d0) -> (d0)>,
                              affine_map<(d0) -> (d0)>,
                              affine_map<(d0) -> (d0)>],
             iterator_types = ["parallel"]}
            ins(%A, %Bcast : memref<16xf64>, memref<?xf64>)
            outs(%B : memref<16xf64>) {
          ^bb0(%in0: f64, %in1: f64, %out: f64):
            %carrier = arith.addf %in1, %in0 : f64
            linalg.yield %carrier : f64
        }

        arts_sde.yield
      }
      arts_sde.yield
    }
    return
  }
}
