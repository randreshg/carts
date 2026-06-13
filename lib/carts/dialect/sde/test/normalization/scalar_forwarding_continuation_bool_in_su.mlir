// RUN: %carts-compile %s --pass-pipeline='builtin.module(scalar-forwarding,sde-dead-state-cleanup)' 2>&1 | %FileCheck %s

// Polygeist lowers some structured returns through a rank-0 boolean memref.
// SDE input normalization promotes that control state back to SSA before the
// OpenMP loop is converted to SDE scheduling structure.

module {
func.func @continuation_flag(%failed: i1, %A: memref<16xf32>) -> i1 {
    %true = arith.constant true
    %false = arith.constant false
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c16 = arith.constant 16 : index
    %flag = memref.alloca() : memref<i1>
    memref.store %true, %flag[] : memref<i1>
    scf.if %failed {
      memref.store %false, %flag[] : memref<i1>
    } else {
      omp.parallel {
        omp.wsloop schedule(static) {
          omp.loop_nest (%i) : index = (%c0) to (%c16) step (%c1) {
            memref.store %true, %flag[] : memref<i1>
            omp.yield
          }
        }
        omp.terminator
      }
      memref.store %true, %flag[] : memref<i1>
    }
    %keep = memref.load %flag[] : memref<i1>
    return %keep : i1
  }
}

// CHECK-LABEL: func.func @continuation_flag
// CHECK-NOT: memref.alloca() : memref<i1>
// CHECK-NOT: memref.store {{.*}} : memref<i1>
// CHECK-NOT: memref.load {{.*}} : memref<i1>
// CHECK: %[[STATE:.*]] = arith.select %arg0, %false, %true : i1
// CHECK: return %[[STATE]] : i1

func.func @branch_local_same_constant(%failed: i1) -> i1 {
  %false = arith.constant false
  %flag = memref.alloca() : memref<i1>
  memref.store %false, %flag[] : memref<i1>
  scf.if %failed {
    %then_true = arith.constant true
    memref.store %then_true, %flag[] : memref<i1>
  } else {
    %else_true = arith.constant true
    memref.store %else_true, %flag[] : memref<i1>
  }
  %keep = memref.load %flag[] : memref<i1>
  return %keep : i1
}

// CHECK-LABEL: func.func @branch_local_same_constant
// CHECK-NOT: memref.load
// CHECK: %[[TRUE:.*]] = arith.constant true
// CHECK: return %[[TRUE]] : i1

func.func @immediate_scratch_index(%src: memref<64xf32>, %dst: memref<64xf32>,
                                   %i: index) {
  %c1 = arith.constant 1 : index
  %idx = memref.alloca() : memref<index>
  %next = arith.addi %i, %c1 : index
  memref.store %next, %idx[] : memref<index>
  %side = arith.addi %i, %i : index
  %loaded = memref.load %idx[] : memref<index>
  %v = memref.load %src[%loaded] : memref<64xf32>
  %unused = arith.addi %side, %c1 : index
  memref.store %v, %dst[%loaded] : memref<64xf32>
  return
}

// CHECK-LABEL: func.func @immediate_scratch_index
// CHECK: %[[NEXT:.*]] = arith.addi %arg2, %c1 : index
// CHECK-NOT: memref.load {{.*}} : memref<index>
// CHECK: memref.load %arg0[%[[NEXT]]] : memref<64xf32>
// CHECK: memref.store {{.*}}, %arg1[%[[NEXT]]] : memref<64xf32>
