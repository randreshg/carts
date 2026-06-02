// RUN: %carts-compile %s --O3 --arts-config %inputs_dir/arts_64t.cfg --start-from sde-planning --pipeline sde-to-codir --mlir-print-ir-after-all 2>&1 | %FileCheck %s
// RUN: %carts-compile %s --O3 --arts-config %inputs_dir/arts_multinode_4x64.cfg --start-from sde-planning --pipeline sde-to-codir | %FileCheck %s --check-prefix=NODE4

// SDE must expose all independent owner dimensions in an ND elementwise update
// instead of stopping at a 2-D tile. CODIR then consumes the committed ND tile
// facts and emits one flat launch ordinal over the full owner-tile space.

// CHECK-LABEL: // -----// IR Dump After DistributionPlanning (distribution-planning) //----- //
// CHECK-LABEL: func.func @elementwise_inplace_3d_owner_tile
// CHECK: sde.su_distribute <blocked>
// CHECK: sde.su_iterate
// CHECK-SAME: classification(<elementwise>)
// CHECK: arrayLayout = [{arrayId = 0 : i64, blockShape = [64, 64, 64]
// CHECK-SAME: kind = "block_parallel"
// CHECK-SAME: ownerDims = [0, 1, 2]
// CHECK-SAME: role = "write"
// CHECK-SAME: partitionGraph = [
// CHECK-SAME: blockShape = [32, 32, 32]
// CHECK-SAME: muBlockCount = 64 : i64
// CHECK-SAME: partitionScore = {blockShape = [32, 32, 32]
// CHECK-SAME: exposedCuCount = 64 : i64
// CHECK-SAME: physicalBlockShape = [32, 32, 32]
// CHECK-SAME: physicalOwnerDims = [0, 1, 2]
// CHECK-LABEL: func.func @elementwise_inplace_3d_transposed_owner_tile
// CHECK: sde.su_distribute <blocked>
// CHECK: sde.su_iterate
// CHECK-SAME: classification(<elementwise>)
// CHECK: partitionGraph = [{blockShape = [32, 32, 32]
// CHECK-SAME: muBlockCount = 64 : i64
// CHECK-SAME: ownerDims = [2, 1, 0]
// CHECK: partitionScore = {blockShape = [32, 32, 32]
// CHECK-SAME: exposedCuCount = 64 : i64
// CHECK-SAME: muBlockCount = 64 : i64
// CHECK-SAME: ownerDims = [2, 1, 0]
// CHECK: physicalBlockShape = [32, 32, 32]
// CHECK-SAME: physicalOwnerDims = [2, 1, 0]
// CHECK-LABEL: func.func @elementwise_outofplace_3d_owner_tile
// CHECK: sde.su_distribute <blocked>
// CHECK: sde.su_iterate
// CHECK-SAME: classification(<elementwise>)
// CHECK: partitionGraph = [{blockShape = [32, 32, 32]
// CHECK-SAME: muBlockCount = 64 : i64
// CHECK-SAME: ownerDims = [0, 1, 2]
// CHECK: partitionScore = {blockShape = [32, 32, 32]
// CHECK-SAME: exposedCuCount = 64 : i64
// CHECK-SAME: muBlockCount = 64 : i64
// CHECK-SAME: ownerDims = [0, 1, 2]
// CHECK: physicalBlockShape = [32, 32, 32]
// CHECK-SAME: physicalOwnerDims = [0, 1, 2]
// CHECK-LABEL: func.func @elementwise_outofplace_3d_transposed_owner_tile
// CHECK: sde.su_distribute <blocked>
// CHECK: sde.su_iterate
// CHECK-SAME: classification(<elementwise>)
// CHECK: partitionGraph = [{blockShape = [32, 32, 32]
// CHECK-SAME: muBlockCount = 64 : i64
// CHECK-SAME: ownerDims = [2, 1, 0]
// CHECK: partitionScore = {blockShape = [32, 32, 32]
// CHECK-SAME: exposedCuCount = 64 : i64
// CHECK-SAME: muBlockCount = 64 : i64
// CHECK-SAME: ownerDims = [2, 1, 0]
// CHECK: physicalBlockShape = [32, 32, 32]
// CHECK-SAME: physicalOwnerDims = [2, 1, 0]
// CHECK-LABEL: // -----// IR Dump After ConvertSdeToCodir
// CHECK-LABEL: func.func @elementwise_inplace_3d_owner_tile
// CHECK: scf.for %[[ORD0:.*]] = %{{.*}} to %{{.*}} step %{{.*}} {
// CHECK: arith.remui %[[ORD0]]
// CHECK: arith.divui %[[ORD0]]
// CHECK: arith.remui
// CHECK: arith.divui
// CHECK: arith.remui
// CHECK: codir.codelet {{.*}}params(%{{.*}}, %{{.*}}, %{{.*}}, %{{.*}}, %{{.*}}, %{{.*}} : index, index, index, index, index, index)
// CHECK-SAME: array_layout = [{arrayId = 0 : i64, blockShape = [64, 64, 64]
// CHECK-SAME: kind = "block_parallel"
// CHECK-SAME: ownerDims = [0, 1, 2]
// CHECK-SAME: distribution_kind = #codir.distribution_kind<blocked>
// CHECK-SAME: partition_score = {
// CHECK-SAME: exposedCuCount = 64 : i64
// CHECK-SAME: tile_owner_dims = [0, 1, 2]
// CHECK-SAME: tile_shape = [32, 32, 32]
// CHECK-LABEL: func.func @elementwise_inplace_3d_transposed_owner_tile
// CHECK: scf.for %[[ORD1:.*]] = %{{.*}} to %{{.*}} step %{{.*}} {
// CHECK: arith.remui %[[ORD1]]
// CHECK: arith.divui %[[ORD1]]
// CHECK: arith.remui
// CHECK: arith.divui
// CHECK: arith.remui
// CHECK: codir.codelet {{.*}}params(%{{.*}}, %{{.*}}, %{{.*}}, %{{.*}}, %{{.*}}, %{{.*}} : index, index, index, index, index, index)
// CHECK-SAME: partition_score = {
// CHECK-SAME: exposedCuCount = 64 : i64
// CHECK-SAME: tile_owner_dims = [2, 1, 0]
// CHECK-SAME: tile_shape = [32, 32, 32]
// CHECK-LABEL: func.func @elementwise_outofplace_3d_owner_tile
// CHECK: codir.codelet
// CHECK-SAME: partition_score = {
// CHECK-SAME: exposedCuCount = 64 : i64
// CHECK-SAME: tile_owner_dims = [0, 1, 2]
// CHECK-SAME: tile_shape = [32, 32, 32]
// CHECK-LABEL: func.func @elementwise_outofplace_3d_transposed_owner_tile
// CHECK: codir.codelet
// CHECK-SAME: partition_score = {
// CHECK-SAME: exposedCuCount = 64 : i64
// CHECK-SAME: tile_owner_dims = [2, 1, 0]
// CHECK-SAME: tile_shape = [32, 32, 32]

// NODE4: arts.runtime_total_workers = 256
// NODE4-LABEL: func.func @elementwise_inplace_3d_owner_tile
// NODE4: codir.codelet
// NODE4-SAME: logical_worker_slice = [26, 26, 13]
// NODE4-SAME: partition_graph = [{blockShape = [13, 13, 13]
// NODE4-SAME: cuGroupCount = 250 : i64
// NODE4-SAME: cuGroupSize = 4 : i64
// NODE4-SAME: muBlockCount = 1000 : i64
// NODE4-SAME: ownerDims = [0, 1, 2]
// NODE4-SAME: partition_score = {
// NODE4-SAME: exposedCuCount = 256 : i64
// NODE4-SAME: targetLogicalWorkers = 256 : i64
// NODE4-SAME: tile_owner_dims = [0, 1, 2]
// NODE4-SAME: tile_shape = [13, 13, 13]
// NODE4-LABEL: func.func @elementwise_inplace_3d_transposed_owner_tile
// NODE4: codir.codelet
// NODE4-SAME: logical_worker_slice = [13, 26, 26]
// NODE4-SAME: partition_graph = [{blockShape = [13, 13, 13]
// NODE4-SAME: cuGroupCount = 250 : i64
// NODE4-SAME: cuGroupSize = 4 : i64
// NODE4-SAME: muBlockCount = 1000 : i64
// NODE4-SAME: ownerDims = [2, 1, 0]
// NODE4-SAME: partition_score = {
// NODE4-SAME: exposedCuCount = 256 : i64
// NODE4-SAME: targetLogicalWorkers = 256 : i64
// NODE4-SAME: tile_owner_dims = [2, 1, 0]
// NODE4-SAME: tile_shape = [13, 13, 13]
// NODE4-LABEL: func.func @elementwise_outofplace_3d_owner_tile
// NODE4: codir.codelet
// NODE4-SAME: logical_worker_slice = [33, 16, 16]
// NODE4-SAME: partition_graph = [{blockShape = [11, 16, 16]
// NODE4-SAME: cuGroupCount = 256 : i64
// NODE4-SAME: cuGroupSize = 3 : i64
// NODE4-SAME: muBlockCount = 768 : i64
// NODE4-SAME: ownerDims = [0, 1, 2]
// NODE4-SAME: partition_score = {
// NODE4-SAME: exposedCuCount = 256 : i64
// NODE4-SAME: targetLogicalWorkers = 256 : i64
// NODE4-SAME: tile_owner_dims = [0, 1, 2]
// NODE4-SAME: tile_shape = [11, 16, 16]
// NODE4-LABEL: func.func @elementwise_outofplace_3d_transposed_owner_tile
// NODE4: codir.codelet
// NODE4-SAME: logical_worker_slice = [16, 16, 33]
// NODE4-SAME: partition_graph = [{blockShape = [16, 16, 11]
// NODE4-SAME: cuGroupCount = 256 : i64
// NODE4-SAME: cuGroupSize = 3 : i64
// NODE4-SAME: muBlockCount = 768 : i64
// NODE4-SAME: ownerDims = [2, 1, 0]
// NODE4-SAME: partition_score = {
// NODE4-SAME: exposedCuCount = 256 : i64
// NODE4-SAME: targetLogicalWorkers = 256 : i64
// NODE4-SAME: tile_owner_dims = [2, 1, 0]
// NODE4-SAME: tile_shape = [16, 16, 11]

module attributes {
  dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f32, dense<32> : vector<2xi64>>, #dlti.dl_entry<i64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i32, dense<32> : vector<2xi64>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi64>>, #dlti.dl_entry<"dlti.endianness", "little">>,
  llvm.data_layout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128",
  llvm.target_triple = "aarch64-unknown-linux-gnu"
} {
  func.func @elementwise_inplace_3d_owner_tile(%A: memref<128x128x128xf32>, %B: memref<128x128x128xf32>) {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c128 = arith.constant 128 : index
    sde.cu_region <parallel> {
      sde.su_iterate (%c0, %c0, %c0) to (%c128, %c128, %c128) step (%c1, %c1, %c1) classification(<elementwise>) {
      ^bb0(%i: index, %j: index, %k: index):
        %old = memref.load %A[%i, %j, %k] : memref<128x128x128xf32>
        %bias = memref.load %B[%i, %j, %k] : memref<128x128x128xf32>
        %next = arith.addf %old, %bias : f32
        memref.store %next, %A[%i, %j, %k] : memref<128x128x128xf32>
        sde.yield
      }
      sde.yield
    }
    return
  }

  func.func @elementwise_inplace_3d_transposed_owner_tile(%A: memref<128x128x128xf32>, %B: memref<128x128x128xf32>) {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c128 = arith.constant 128 : index
    sde.cu_region <parallel> {
      sde.su_iterate (%c0, %c0, %c0) to (%c128, %c128, %c128) step (%c1, %c1, %c1) classification(<elementwise>) {
      ^bb0(%i: index, %j: index, %k: index):
        %old = memref.load %A[%k, %j, %i] : memref<128x128x128xf32>
        %bias = memref.load %B[%k, %j, %i] : memref<128x128x128xf32>
        %next = arith.addf %old, %bias : f32
        memref.store %next, %A[%k, %j, %i] : memref<128x128x128xf32>
        sde.yield
      }
      sde.yield
    }
    return
  }

  func.func @elementwise_outofplace_3d_owner_tile(%A: memref<128x128x128xf32>, %B: memref<128x128x128xf32>, %C: memref<128x128x128xf32>) {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c128 = arith.constant 128 : index
    sde.cu_region <parallel> {
      sde.su_iterate (%c0, %c0, %c0) to (%c128, %c128, %c128) step (%c1, %c1, %c1) classification(<elementwise>) {
      ^bb0(%i: index, %j: index, %k: index):
        %lhs = memref.load %A[%i, %j, %k] : memref<128x128x128xf32>
        %rhs = memref.load %B[%i, %j, %k] : memref<128x128x128xf32>
        %next = arith.addf %lhs, %rhs : f32
        memref.store %next, %C[%i, %j, %k] : memref<128x128x128xf32>
        sde.yield
      }
      sde.yield
    }
    return
  }

  func.func @elementwise_outofplace_3d_transposed_owner_tile(%A: memref<128x128x128xf32>, %B: memref<128x128x128xf32>, %C: memref<128x128x128xf32>) {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c128 = arith.constant 128 : index
    sde.cu_region <parallel> {
      sde.su_iterate (%c0, %c0, %c0) to (%c128, %c128, %c128) step (%c1, %c1, %c1) classification(<elementwise>) {
      ^bb0(%i: index, %j: index, %k: index):
        %lhs = memref.load %A[%k, %j, %i] : memref<128x128x128xf32>
        %rhs = memref.load %B[%k, %j, %i] : memref<128x128x128xf32>
        %next = arith.addf %lhs, %rhs : f32
        memref.store %next, %C[%k, %j, %i] : memref<128x128x128xf32>
        sde.yield
      }
      sde.yield
    }
    return
  }
}
