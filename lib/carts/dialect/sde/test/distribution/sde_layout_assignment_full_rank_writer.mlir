// RUN: %carts-compile %s --O3 --arts-config %inputs_dir/arts_64t.cfg --start-from sde-planning --pipeline sde-to-codir --mlir-print-ir-after-all 2>&1 | %FileCheck %s

// Layout assignment must seed a full-root array layout from full-rank write
// support, not from the first write in program order. A boundary writer such as
// A[0, j] participates in the chosen layout, but it cannot choose the canonical
// owner dim for the whole A root.

// CHECK-LABEL: // -----// IR Dump After LayoutAssignment (sde-layout-assignment) //----- //
// CHECK-LABEL: func.func @partial_then_full_writer
// CHECK: sde.su_iterate (%c0) to (%c64) step (%c1)
// CHECK: } {arrayLayout = [{arrayId = [[A:[0-9]+]] : i64
// CHECK-SAME: kind = "block_parallel"
// CHECK-SAME: ownerDims = [0, 1]
// CHECK-SAME: role = "write"
// CHECK: sde.su_iterate (%c0, %c0) to (%c32, %c64) step (%c1, %c1)
// CHECK: } {arrayLayout = [{arrayId = [[A]] : i64
// CHECK-SAME: kind = "block_parallel"
// CHECK-SAME: ownerDims = [0, 1]
// CHECK-SAME: role = "write"

// CHECK-LABEL: func.func @partial_then_full_nested_writer
// CHECK: sde.su_iterate (%c0) to (%c64) step (%c1)
// CHECK: } {arrayLayout = [{arrayId = [[N:[0-9]+]] : i64
// CHECK-SAME: kind = "block_parallel"
// CHECK-SAME: ownerDims = [0]
// CHECK-SAME: role = "write"
// CHECK: sde.su_iterate (%c0) to (%c32) step (%c1)
// CHECK: } {arrayLayout = [{arrayId = [[N]] : i64
// CHECK-SAME: kind = "block_parallel"
// CHECK-SAME: ownerDims = [0]
// CHECK-SAME: role = "write"

// CHECK-LABEL: func.func @partial_full_nested_then_halo_reader
// CHECK: sde.su_iterate (%c0) to (%c64) step (%c1)
// CHECK: } {arrayLayout = [{arrayId = [[H:[0-9]+]] : i64
// CHECK-SAME: kind = "block_parallel"
// CHECK-SAME: ownerDims = [0, 1]
// CHECK-SAME: role = "write"
// CHECK: sde.su_iterate (%c0, %c0) to (%c32, %c64) step (%c1, %c1)
// CHECK: } {arrayLayout = [{arrayId = [[H]] : i64
// CHECK-SAME: kind = "block_parallel"
// CHECK-SAME: ownerDims = [0, 1]
// CHECK-SAME: role = "write"
// CHECK: sde.su_iterate (%c0) to (%c31) step (%c1)
// CHECK: } {accessMaxOffsets =
// CHECK-SAME: arrayLayout = [{arrayId = [[H]] : i64
// CHECK-SAME: kind = "block_parallel"
// CHECK-SAME: ownerDims = [0, 1]
// CHECK-SAME: role = "read"

// CHECK-LABEL: func.func @full_writer_two_stencil_readers
// CHECK: sde.su_iterate (%c0, %c0) to (%c32, %c64) step (%c1, %c1)
// CHECK: } {arrayLayout = [{arrayId = [[S:[0-9]+]] : i64
// CHECK-SAME: kind = "block_parallel"
// CHECK-SAME: ownerDims = [0, 1]
// CHECK-SAME: role = "write"
// CHECK: sde.su_iterate (%c0) to (%c31) step (%c1)
// CHECK: } {accessMaxOffsets =
// CHECK-SAME: arrayLayout = [{arrayId = [[S]] : i64
// CHECK-SAME: kind = "block_parallel"
// CHECK-SAME: ownerDims = [0, 1]
// CHECK-SAME: role = "read"
// CHECK: sde.su_iterate (%c0) to (%c63) step (%c1)
// CHECK: } {accessMaxOffsets =
// CHECK-SAME: arrayLayout = [{arrayId = [[S]] : i64
// CHECK-SAME: kind = "block_parallel"
// CHECK-SAME: ownerDims = [0, 1]
// CHECK-SAME: role = "read"

// CHECK-LABEL: func.func @partial_only_writer
// CHECK: sde.su_iterate (%c0) to (%c64) step (%c1)
// CHECK: } {arrayLayout = [{arrayId = [[P:[0-9]+]] : i64
// CHECK-SAME: kind = "replicated"
// CHECK-SAME: ownerDims = []
// CHECK-SAME: role = "write"

// CHECK-LABEL: // -----// IR Dump After DistributionPlanning (distribution-planning) //----- //
// CHECK-LABEL: func.func @mixed_same_su_partial_full_writer
// CHECK: sde.su_iterate (%c0, %c0) to (%c32, %c64) step (
// CHECK: } {arrayLayout = [{arrayId = [[M:[0-9]+]] : i64
// CHECK-SAME: kind = "block_parallel"
// CHECK-SAME: ownerDims = [0, 1]
// CHECK-SAME: role = "write"
// CHECK-SAME: pattern = #sde.pattern<uniform>
// CHECK-NOT: physicalOwnerDims
// CHECK-LABEL: func.func @full_writer_two_stencil_readers

// CHECK-LABEL: // -----// IR Dump After ConvertSdeToCodir (convert-sde-to-codir) //----- //
// CHECK-LABEL: func.func @partial_full_nested_then_halo_reader
// CHECK: codir.codelet
// CHECK-SAME: array_layout = [{arrayId = [[H]] : i64
// CHECK-SAME: ownerDims = [0, 1]
// CHECK-SAME: role = "write"
// CHECK: codir.codelet
// CHECK-SAME: array_layout = [{arrayId = [[H]] : i64
// CHECK-SAME: ownerDims = [0, 1]
// CHECK-SAME: role = "read"

module attributes {
  dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i32, dense<32> : vector<2xi64>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi64>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i64>>,
  llvm.data_layout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128",
  llvm.target_triple = "aarch64-unknown-linux-gnu"
} {
  func.func @partial_then_full_writer(%A: memref<32x64xf64>, %B: memref<32x64xf64>) {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c32 = arith.constant 32 : index
    %c64 = arith.constant 64 : index
    %zero = arith.constant 0.000000e+00 : f64
    sde.cu_region <parallel> {
      sde.su_iterate (%c0) to (%c64) step (%c1) {
      ^bb0(%j: index):
        memref.store %zero, %A[%c0, %j] : memref<32x64xf64>
        sde.yield
      }
      sde.su_iterate (%c0, %c0) to (%c32, %c64) step (%c1, %c1) {
      ^bb0(%i: index, %j: index):
        %v = memref.load %B[%i, %j] : memref<32x64xf64>
        memref.store %v, %A[%i, %j] : memref<32x64xf64>
        sde.yield
      }
      sde.yield
    }
    return
  }

  func.func @partial_then_full_nested_writer(%A: memref<32x64xf64>, %B: memref<32x64xf64>) {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c32 = arith.constant 32 : index
    %c64 = arith.constant 64 : index
    %zero = arith.constant 0.000000e+00 : f64
    sde.cu_region <parallel> {
      sde.su_iterate (%c0) to (%c64) step (%c1) {
      ^bb0(%j: index):
        memref.store %zero, %A[%c0, %j] : memref<32x64xf64>
        sde.yield
      }
      sde.su_iterate (%c0) to (%c32) step (%c1) {
      ^bb0(%i: index):
        scf.for %j = %c0 to %c64 step %c1 {
          %v = memref.load %B[%i, %j] : memref<32x64xf64>
          memref.store %v, %A[%i, %j] : memref<32x64xf64>
        }
        sde.yield
      }
      sde.yield
    }
    return
  }

  func.func @partial_full_nested_then_halo_reader(%A: memref<32x64xf64>, %B: memref<32x64xf64>, %C: memref<32x64xf64>) {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c31 = arith.constant 31 : index
    %c32 = arith.constant 32 : index
    %c64 = arith.constant 64 : index
    %zero = arith.constant 0.000000e+00 : f64
    sde.cu_region <parallel> {
      sde.su_iterate (%c0) to (%c64) step (%c1) {
      ^bb0(%j: index):
        memref.store %zero, %A[%c0, %j] : memref<32x64xf64>
        sde.yield
      }
      sde.su_iterate (%c0) to (%c32) step (%c1) {
      ^bb0(%i: index):
        scf.for %j = %c0 to %c64 step %c1 {
          %v = memref.load %B[%i, %j] : memref<32x64xf64>
          memref.store %v, %A[%i, %j] : memref<32x64xf64>
        }
        sde.yield
      }
      sde.su_iterate (%c0) to (%c31) step (%c1) {
      ^bb0(%i: index):
        scf.for %j = %c0 to %c64 step %c1 {
          %ip1 = arith.addi %i, %c1 : index
          %a0 = memref.load %A[%i, %j] : memref<32x64xf64>
          %a1 = memref.load %A[%ip1, %j] : memref<32x64xf64>
          %sum = arith.addf %a0, %a1 : f64
          memref.store %sum, %C[%i, %j] : memref<32x64xf64>
        }
        sde.yield
      }
      sde.yield
    }
    return
  }

  func.func @mixed_same_su_partial_full_writer(%A: memref<32x64xf64>, %B: memref<32x64xf64>) {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c32 = arith.constant 32 : index
    %c64 = arith.constant 64 : index
    %zero = arith.constant 0.000000e+00 : f64
    sde.cu_region <parallel> {
      sde.su_iterate (%c0, %c0) to (%c32, %c64) step (%c1, %c1) {
      ^bb0(%i: index, %j: index):
        memref.store %zero, %A[%c0, %j] : memref<32x64xf64>
        %v = memref.load %B[%i, %j] : memref<32x64xf64>
        memref.store %v, %A[%i, %j] : memref<32x64xf64>
        sde.yield
      }
      sde.yield
    }
    return
  }

  func.func @full_writer_two_stencil_readers(%A: memref<32x64xf64>, %B: memref<32x64xf64>, %C: memref<32x64xf64>, %D: memref<32x64xf64>) {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c31 = arith.constant 31 : index
    %c32 = arith.constant 32 : index
    %c63 = arith.constant 63 : index
    %c64 = arith.constant 64 : index
    sde.cu_region <parallel> {
      sde.su_iterate (%c0, %c0) to (%c32, %c64) step (%c1, %c1) {
      ^bb0(%i: index, %j: index):
        %v = memref.load %B[%i, %j] : memref<32x64xf64>
        memref.store %v, %A[%i, %j] : memref<32x64xf64>
        sde.yield
      }
      sde.su_iterate (%c0) to (%c31) step (%c1) {
      ^bb0(%i: index):
        scf.for %j = %c0 to %c64 step %c1 {
          %ip1 = arith.addi %i, %c1 : index
          %a0 = memref.load %A[%i, %j] : memref<32x64xf64>
          %a1 = memref.load %A[%ip1, %j] : memref<32x64xf64>
          %sum = arith.addf %a0, %a1 : f64
          memref.store %sum, %C[%i, %j] : memref<32x64xf64>
        }
        sde.yield
      }
      sde.su_iterate (%c0) to (%c63) step (%c1) {
      ^bb0(%j: index):
        scf.for %i = %c0 to %c32 step %c1 {
          %jp1 = arith.addi %j, %c1 : index
          %a0 = memref.load %A[%i, %j] : memref<32x64xf64>
          %a1 = memref.load %A[%i, %jp1] : memref<32x64xf64>
          %sum = arith.addf %a0, %a1 : f64
          memref.store %sum, %D[%i, %j] : memref<32x64xf64>
        }
        sde.yield
      }
      sde.yield
    }
    return
  }

  func.func @partial_only_writer(%A: memref<32x64xf64>) {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c64 = arith.constant 64 : index
    %zero = arith.constant 0.000000e+00 : f64
    sde.cu_region <parallel> {
      sde.su_iterate (%c0) to (%c64) step (%c1) {
      ^bb0(%j: index):
        memref.store %zero, %A[%c0, %j] : memref<32x64xf64>
        sde.yield
      }
      sde.yield
    }
    return
  }
}
