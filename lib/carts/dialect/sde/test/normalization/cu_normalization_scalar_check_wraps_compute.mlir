// RUN: not %carts-compile %s --pass-pipeline='builtin.module(verify-sde)' 2>&1 \
// RUN:   | %FileCheck %s --check-prefix=REJECT
// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-cu-normalization)' \
// RUN:   | %FileCheck %s --check-prefix=NORM
// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-cu-normalization,verify-sde)'

// Non-OpenMP scalar verification/check code (load, compare, store a flag) is
// scalar-effect source work outside any CU. The whole contiguous scalar span is
// wrapped into one conservative cu_region<single>; the loaded value, threshold
// constant, comparison, and flag store all stay self-contained inside it.

module {
  func.func @scalar_check(%A: memref<8xf32>, %flag: memref<i1>) {
    %c0 = arith.constant 0 : index
    %B = sde.mu_alloc : memref<8xf32>
    %v0 = memref.load %A[%c0] : memref<8xf32>
    %thr = arith.constant 1.000000e+00 : f32
    %ok = arith.cmpf ogt, %v0, %thr : f32
    memref.store %ok, %flag[] : memref<i1>
    return
  }
}

// REJECT: source executable work outside any CU

// NORM-LABEL: func @scalar_check
// NORM:         sde.mu_alloc
// NORM:         sde.cu_region <single> {
// NORM:           memref.load
// NORM:           arith.cmpf
// NORM:           memref.store
// NORM:         }
// NORM-NOT:    sde.cu_region <single>
