// RUN: not %carts-compile %s --pass-pipeline='builtin.module(verify-sde)' 2>&1 \
// RUN:   | %FileCheck %s --check-prefix=REJECT
// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-cu-normalization)' \
// RUN:   | %FileCheck %s --check-prefix=NORM
// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-cu-normalization,verify-sde)'

// A sequential non-OpenMP reduction in the memref-accumulator form the real
// pipeline leaves behind (Parallelize does not parallelize the loop-carried
// scalar accumulator). The accumulator alloca, its zero-init, the reduction
// loop, and the result store form one self-contained scalar span: all values
// defined in the span are consumed inside it, so it wraps cleanly into one
// conservative cu_region<single>.

module {
  func.func @host_reduction(%A: memref<8xf32>, %out: memref<f32>) {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c8 = arith.constant 8 : index
    %B = sde.mu_alloc : memref<8xf32>
    %z = arith.constant 0.000000e+00 : f32
    %acc = memref.alloca() : memref<f32>
    memref.store %z, %acc[] : memref<f32>
    scf.for %i = %c0 to %c8 step %c1 {
      %v = memref.load %A[%i] : memref<8xf32>
      %s = memref.load %acc[] : memref<f32>
      %a = arith.addf %s, %v : f32
      memref.store %a, %acc[] : memref<f32>
    }
    %r = memref.load %acc[] : memref<f32>
    memref.store %r, %out[] : memref<f32>
    return
  }
}

// REJECT: source executable work outside any CU

// NORM-LABEL: func @host_reduction
// NORM:         sde.mu_alloc
// NORM:         sde.cu_region <single> {
// NORM:           memref.alloca
// NORM:           scf.for
// NORM:             arith.addf
// NORM:           memref.store {{.*}} memref<f32>
// NORM:         }
// NORM-NOT:    sde.cu_region <single>
