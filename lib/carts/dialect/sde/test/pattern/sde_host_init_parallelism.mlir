// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-cu-normalization,sde-parallelize)' \
// RUN:   | %FileCheck %s --check-prefix=RAISE
// RUN: %carts-compile %s --O3 --arts-config %inputs_dir/arts_64t.cfg \
// RUN:   --start-from sde-planning --pipeline sde-planning \
// RUN:   --mlir-print-ir-after-all 2>&1 | %FileCheck %s --check-prefix=PLAN

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
// RAISE:           sde.su_iterate
// RAISE:             arith.index_cast
// RAISE:             memref.store %{{.*}}, %{{.*}}[%{{.*}}] : memref<8xf32>
// RAISE-NOT:       memref.load %{{.*}}[] : memref<i32>

// RAISE-LABEL: func.func @init_3d
// RAISE:         sde.cu_region <parallel> {
// RAISE:           sde.su_iterate
// RAISE:             scf.for
// RAISE:               scf.for
// RAISE:                 memref.store %{{.*}}, %{{.*}}[%{{.*}}, %{{.*}}, %{{.*}}] : memref<4x5x6xf32>

// RAISE-LABEL: func.func @multi_store_readonly
// RAISE:         sde.cu_region <parallel> {
// RAISE:           sde.su_iterate
// RAISE:             memref.load %{{.*}}[%{{.*}}] : memref<16xf32>
// RAISE:             memref.store %{{.*}}, %{{.*}}[%{{.*}}] : memref<16xf32>
// RAISE:             memref.store %{{.*}}, %{{.*}}[%{{.*}}] : memref<16xf32>

// PLAN-LABEL: // -----// IR Dump After PatternAnalysis
// PLAN-LABEL: func.func @init_3d
// PLAN:       sde.su_iterate (%{{.*}}, %{{.*}}, %{{.*}}) to (%{{.*}}, %{{.*}}, %{{.*}}) step (%{{.*}}, %{{.*}}, %{{.*}}) classification(<elementwise>)
