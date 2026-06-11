// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-memory-unit-realization)' \
// RUN:   | %FileCheck %s

func.func @folds_mu_alloc_null_check() -> i1 {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c128 = arith.constant 128 : index
  %v = arith.constant 1.000000e+00 : f32
  %null = llvm.mlir.zero : !llvm.ptr
  %A = memref.alloc() : memref<128xf32>
  %p = polygeist.memref2pointer %A : memref<128xf32> to !llvm.ptr
  %isnull = llvm.icmp "eq" %p, %null : !llvm.ptr
  sde.su_iterate (%c0) to (%c128) step (%c1) classification(<elementwise>) {
  ^bb0(%i: index):
    sde.cu_region <single> {
      memref.store %v, %A[%i] : memref<128xf32>
      sde.yield
    }
  }
  memref.dealloc %A : memref<128xf32>
  return %isnull : i1
}

func.func private @consume_ptr(!llvm.ptr)

func.func @preserves_non_null_pointer_use() -> i1 {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c128 = arith.constant 128 : index
  %v = arith.constant 2.000000e+00 : f32
  %null = llvm.mlir.zero : !llvm.ptr
  %A = memref.alloc() : memref<128xf32>
  %p = polygeist.memref2pointer %A : memref<128xf32> to !llvm.ptr
  func.call @consume_ptr(%p) : (!llvm.ptr) -> ()
  %nonnull = llvm.icmp "ne" %p, %null : !llvm.ptr
  sde.su_iterate (%c0) to (%c128) step (%c1) classification(<elementwise>) {
  ^bb0(%i: index):
    sde.cu_region <single> {
      memref.store %v, %A[%i] : memref<128xf32>
      sde.yield
    }
  }
  memref.dealloc %A : memref<128xf32>
  return %nonnull : i1
}

// CHECK-LABEL: func.func @folds_mu_alloc_null_check
// CHECK: %[[MU:.*]] = sde.mu_alloc : memref<128xf32>
// CHECK-NOT: polygeist.memref2pointer %[[MU]]
// CHECK-NOT: llvm.icmp
// CHECK: %[[FALSE:.*]] = arith.constant false
// CHECK: return %[[FALSE]] : i1

// CHECK-LABEL: func.func @preserves_non_null_pointer_use
// CHECK: %[[MU:.*]] = sde.mu_alloc : memref<128xf32>
// CHECK: %[[PTR:.*]] = polygeist.memref2pointer %[[MU]] : memref<128xf32> to !llvm.ptr
// CHECK: call @consume_ptr(%[[PTR]])
// CHECK: llvm.icmp "ne" %[[PTR]]
