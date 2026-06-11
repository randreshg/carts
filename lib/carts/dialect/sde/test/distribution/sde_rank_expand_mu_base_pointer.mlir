// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-rank-expand-mu,verify-sde-mu-layout)' 2>&1 | %FileCheck %s

// Base-pointer comparisons preserve allocation identity across rank expansion.

// CHECK-LABEL: func.func @rank_expand_with_null_check
// The base-pointer use follows the expanded block-grid MU.
// CHECK: %[[E:.*]] = sde.mu_alloc : memref<8x16x64xf32>
// CHECK: polygeist.memref2pointer %[[E]] : memref<8x16x64xf32> to !llvm.ptr
// CHECK: llvm.icmp "eq"
// CHECK: arith.divui
// CHECK: arith.remui
// CHECK: memref.load %[[E]][%{{.*}}, %{{.*}}, %{{.*}}] : memref<8x16x64xf32>

func.func @rank_expand_with_null_check() -> i1 {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c64 = arith.constant 64 : index
  %c128 = arith.constant 128 : index
  %null = llvm.mlir.zero : !llvm.ptr
  %A = sde.mu_alloc : memref<128x64xf32>
  %p = polygeist.memref2pointer %A : memref<128x64xf32> to !llvm.ptr
  %isnull = llvm.icmp "eq" %p, %null : !llvm.ptr
  sde.su_iterate (%c0) to (%c128) step (%c1) classification(<elementwise>) {
  ^bb0(%i: index):
    sde.cu_region <single> {
      scf.for %j = %c0 to %c64 step %c1 {
        %v = memref.load %A[%i, %j] : memref<128x64xf32>
        memref.store %v, %A[%i, %j] : memref<128x64xf32>
    }
      sde.yield
    }
  } {physicalOwnerDims = [0], physicalBlockShape = [16, 64]}
  return %isnull : i1
}
