// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-cu-normalization,sde-parallelize)' \
// RUN:   | %FileCheck %s --check-prefix=RAISE
// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-cu-normalization,sde-parallelize,sde-loop-pattern-facts)' \
// RUN:   | %FileCheck %s --check-prefix=PLAN

module {
  func.func @affine_counter_init() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c8 = arith.constant 8 : index
    %i7 = arith.constant 7 : i32
    %i1 = arith.constant 1 : i32
    %A = sde.mu_alloc : memref<8xf32>
    %counter = memref.alloca() : memref<i32>
    memref.store %i7, %counter[] : memref<i32>
    scf.for %i = %c0 to %c8 step %c1 {
      %n = memref.load %counter[] : memref<i32>
      %v = arith.sitofp %n : i32 to f32
      memref.store %v, %A[%i] : memref<8xf32>
      %next = arith.addi %n, %i1 : i32
      memref.store %next, %counter[] : memref<i32>
    }
    return
  }

  func.func @init_3d() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index
    %c5 = arith.constant 5 : index
    %c6 = arith.constant 6 : index
    %one = arith.constant 1.000000e+00 : f32
    %A = sde.mu_alloc : memref<4x5x6xf32>
    scf.for %i = %c0 to %c4 step %c1 {
      scf.for %j = %c0 to %c5 step %c1 {
        scf.for %k = %c0 to %c6 step %c1 {
          memref.store %one, %A[%i, %j, %k] : memref<4x5x6xf32>
        }
      }
    }
    return
  }

  func.func @init_2d_scalar_prefix() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index
    %c6 = arith.constant 6 : index
    %A = sde.mu_alloc : memref<4x6xf32>
    scf.for %i = %c0 to %c4 step %c1 {
      %ii = arith.index_cast %i : index to i32
      %fi = arith.sitofp %ii : i32 to f32
      scf.for %j = %c0 to %c6 step %c1 {
        %jj = arith.index_cast %j : index to i32
        %fj = arith.sitofp %jj : i32 to f32
        %v = arith.addf %fi, %fj : f32
        memref.store %v, %A[%i, %j] : memref<4x6xf32>
      }
    }
    return
  }

  func.func @multi_store_readonly(%input: memref<16xf32>) {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c16 = arith.constant 16 : index
    %one = arith.constant 1.000000e+00 : f32
    %gamma = sde.mu_alloc : memref<16xf32>
    %beta = sde.mu_alloc : memref<16xf32>
    scf.for %i = %c0 to %c16 step %c1 {
      %x = memref.load %input[%i] : memref<16xf32>
      %g = arith.addf %x, %one : f32
      memref.store %g, %gamma[%i] : memref<16xf32>
      memref.store %x, %beta[%i] : memref<16xf32>
    }
    return
  }
}

// RAISE-LABEL: func.func @affine_counter_init
// RAISE:         sde.cu_region <single> {
// RAISE:           memref.alloca
// RAISE:           memref.store
// RAISE:         sde.su_iterate
// RAISE:           sde.cu_region <single> {
// RAISE:             arith.index_cast
// RAISE:             memref.store %{{.*}}, %{{.*}}[%{{.*}}] : memref<8xf32>
// RAISE-NOT:       memref.load %{{.*}}[] : memref<i32>

// RAISE-LABEL: func.func @init_3d
// RAISE:         sde.su_iterate
// RAISE:           sde.cu_region <single> {
// RAISE:             scf.for
// RAISE:               scf.for
// RAISE:                 memref.store %{{.*}}, %{{.*}}[%{{.*}}, %{{.*}}, %{{.*}}] : memref<4x5x6xf32>

// RAISE-LABEL: func.func @init_2d_scalar_prefix
// RAISE:         sde.su_iterate
// RAISE:           sde.cu_region <single> {
// RAISE:             arith.sitofp
// RAISE:             scf.for
// RAISE:               memref.store %{{.*}}, %{{.*}}[%{{.*}}, %{{.*}}] : memref<4x6xf32>

// RAISE-LABEL: func.func @multi_store_readonly
// RAISE:         sde.su_iterate
// RAISE:           sde.cu_region <single> {
// RAISE:             memref.load %{{.*}}[%{{.*}}] : memref<16xf32>
// RAISE:             memref.store %{{.*}}, %{{.*}}[%{{.*}}] : memref<16xf32>
// RAISE:             memref.store %{{.*}}, %{{.*}}[%{{.*}}] : memref<16xf32>

// PLAN-LABEL: func.func @init_3d
// PLAN:       sde.su_iterate (%{{.*}}, %{{.*}}, %{{.*}}) to (%{{.*}}, %{{.*}}, %{{.*}}) step (%{{.*}}, %{{.*}}, %{{.*}}) classification(<elementwise>)
