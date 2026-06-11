// RUN: %carts-compile %s --O3 --arts-config %inputs_dir/arts_64t.cfg --start-from sde-planning --pipeline sde-planning --mlir-print-ir-after-all 2>&1 | %FileCheck %s
// SDE must expose all independent owner dimensions in an ND elementwise update
// instead of stopping at a 2-D tile.

// CHECK-LABEL: // -----// IR Dump After DistributionPlanning (distribution-planning) //----- //
// CHECK-LABEL: func.func @elementwise_inplace_3d_owner_tile
// CHECK: sde.su_distribute <blocked>
// CHECK: sde.su_iterate
// CHECK-SAME: classification(<elementwise>)
// CHECK: arrayLayout = [{arrayId = 0 : i64, blockShape = [64, 64, 64]
// CHECK-SAME: kind = "block_parallel"
// CHECK-SAME: role = "write"
// CHECK-SAME: physicalBlockShape = [32, 32, 32]
// CHECK-SAME: physicalOwnerDims = [0, 1, 2]
// CHECK-LABEL: func.func @elementwise_inplace_3d_transposed_owner_tile
// CHECK: sde.su_distribute <blocked>
// CHECK: sde.su_iterate
// CHECK-SAME: classification(<elementwise>)
// CHECK: physicalBlockShape = [32, 32, 32]
// CHECK-SAME: physicalOwnerDims = [2, 1, 0]
// CHECK-LABEL: func.func @elementwise_outofplace_3d_owner_tile
// CHECK: sde.su_distribute <blocked>
// CHECK: sde.su_iterate
// CHECK-SAME: classification(<elementwise>)
// CHECK: physicalBlockShape = [32, 32, 32]
// CHECK-SAME: physicalOwnerDims = [0, 1, 2]
// CHECK-LABEL: func.func @elementwise_outofplace_3d_transposed_owner_tile
// CHECK: sde.su_distribute <blocked>
// CHECK: sde.su_iterate
// CHECK-SAME: classification(<elementwise>)
// CHECK: physicalBlockShape = [32, 32, 32]
// CHECK-SAME: physicalOwnerDims = [2, 1, 0]
module attributes {
  dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f32, dense<32> : vector<2xi64>>, #dlti.dl_entry<i64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i32, dense<32> : vector<2xi64>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi64>>, #dlti.dl_entry<"dlti.endianness", "little">>,
  llvm.data_layout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128",
  llvm.target_triple = "aarch64-unknown-linux-gnu"
} {
  func.func @elementwise_inplace_3d_owner_tile(%A: memref<128x128x128xf32>, %B: memref<128x128x128xf32>) {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c128 = arith.constant 128 : index
    sde.su_iterate (%c0, %c0, %c0) to (%c128, %c128, %c128) step (%c1, %c1, %c1) classification(<elementwise>) {
    ^bb0(%i: index, %j: index, %k: index):
      sde.cu_region <single> {
        %old = memref.load %A[%i, %j, %k] : memref<128x128x128xf32>
        %bias = memref.load %B[%i, %j, %k] : memref<128x128x128xf32>
        %next = arith.addf %old, %bias : f32
        memref.store %next, %A[%i, %j, %k] : memref<128x128x128xf32>
        sde.yield
      }
    }
    return
  }

  func.func @elementwise_inplace_3d_transposed_owner_tile(%A: memref<128x128x128xf32>, %B: memref<128x128x128xf32>) {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c128 = arith.constant 128 : index
    sde.su_iterate (%c0, %c0, %c0) to (%c128, %c128, %c128) step (%c1, %c1, %c1) classification(<elementwise>) {
    ^bb0(%i: index, %j: index, %k: index):
      sde.cu_region <single> {
        %old = memref.load %A[%k, %j, %i] : memref<128x128x128xf32>
        %bias = memref.load %B[%k, %j, %i] : memref<128x128x128xf32>
        %next = arith.addf %old, %bias : f32
        memref.store %next, %A[%k, %j, %i] : memref<128x128x128xf32>
        sde.yield
      }
    }
    return
  }

  func.func @elementwise_outofplace_3d_owner_tile(%A: memref<128x128x128xf32>, %B: memref<128x128x128xf32>, %C: memref<128x128x128xf32>) {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c128 = arith.constant 128 : index
    sde.su_iterate (%c0, %c0, %c0) to (%c128, %c128, %c128) step (%c1, %c1, %c1) classification(<elementwise>) {
    ^bb0(%i: index, %j: index, %k: index):
      sde.cu_region <single> {
        %lhs = memref.load %A[%i, %j, %k] : memref<128x128x128xf32>
        %rhs = memref.load %B[%i, %j, %k] : memref<128x128x128xf32>
        %next = arith.addf %lhs, %rhs : f32
        memref.store %next, %C[%i, %j, %k] : memref<128x128x128xf32>
        sde.yield
      }
    }
    return
  }

  func.func @elementwise_outofplace_3d_transposed_owner_tile(%A: memref<128x128x128xf32>, %B: memref<128x128x128xf32>, %C: memref<128x128x128xf32>) {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c128 = arith.constant 128 : index
    sde.su_iterate (%c0, %c0, %c0) to (%c128, %c128, %c128) step (%c1, %c1, %c1) classification(<elementwise>) {
    ^bb0(%i: index, %j: index, %k: index):
      sde.cu_region <single> {
        %lhs = memref.load %A[%k, %j, %i] : memref<128x128x128xf32>
        %rhs = memref.load %B[%k, %j, %i] : memref<128x128x128xf32>
        %next = arith.addf %lhs, %rhs : f32
        memref.store %next, %C[%k, %j, %i] : memref<128x128x128xf32>
        sde.yield
      }
    }
    return
  }
}
