// RUN: %carts-compile %s --O3 --arts-config %inputs_dir/arts_64t.cfg --start-from sde-planning --pipeline sde-planning --mlir-print-ir-after-all 2>&1 | %FileCheck %s

// Regression: a sibling matmul intermediate may be consumed on a transposed
// contraction window. SDE must derive the block_contraction owner position from
// the actual access map, not assume the reduction IV indexes physical dim 0.

// CHECK-LABEL: // -----// IR Dump After LayoutAssignment (sde-layout-assignment) //----- //
// CHECK-LABEL: func.func @transposed_contraction_input
// CHECK: arrayLayout = [{arrayId = [[FID:[0-9]+]] : i64, {{.*}}kind = "block_contraction", muBlockCount = 2 : i64, ownerDims = [1], role = "write"}
// CHECK: arrayLayout = [
// CHECK-SAME: {arrayId = [[FID]] : i64, {{.*}}kind = "block_contraction", muBlockCount = 2 : i64, ownerDims = [1], role = "read"}

module attributes {
  dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f32, dense<32> : vector<2xi64>>, #dlti.dl_entry<i64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i32, dense<32> : vector<2xi64>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi64>>, #dlti.dl_entry<"dlti.endianness", "little">>,
  llvm.data_layout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128",
  llvm.target_triple = "aarch64-unknown-linux-gnu"
} {
  func.func @transposed_contraction_input(%C: memref<1024x1024xf32>, %D: memref<1024x1024xf32>,
                                          %E: memref<1024x1024xf32>, %F: memref<1024x1024xf32>,
                                          %G: memref<1024x1024xf32>) {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c1024 = arith.constant 1024 : index
    // F = C * D
    sde.su_iterate (%c0, %c0) to (%c1024, %c1024) step (%c1, %c1) {
    ^bb0(%i: index, %j: index):
      sde.cu_region <single> {
        scf.for %k = %c0 to %c1024 step %c1 {
          %c = memref.load %C[%i, %k] : memref<1024x1024xf32>
          %d = memref.load %D[%k, %j] : memref<1024x1024xf32>
          %f = memref.load %F[%i, %j] : memref<1024x1024xf32>
          %p = arith.mulf %c, %d : f32
          %s = arith.addf %f, %p : f32
          memref.store %s, %F[%i, %j] : memref<1024x1024xf32>
      }
        sde.yield
      }
    }

    // G = E * F^T, so the contraction IV indexes F's physical dim 1.
    sde.su_iterate (%c0, %c0) to (%c1024, %c1024) step (%c1, %c1) {
    ^bb0(%i: index, %j: index):
      sde.cu_region <single> {
        scf.for %k = %c0 to %c1024 step %c1 {
          %e = memref.load %E[%i, %k] : memref<1024x1024xf32>
          %f = memref.load %F[%j, %k] : memref<1024x1024xf32>
          %g = memref.load %G[%i, %j] : memref<1024x1024xf32>
          %p = arith.mulf %e, %f : f32
          %s = arith.addf %g, %p : f32
          memref.store %s, %G[%i, %j] : memref<1024x1024xf32>
      }
        sde.yield
      }
    }
    return
  }
}
