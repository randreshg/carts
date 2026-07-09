// RUN: %carts-compile %s --O3 --arts-config %inputs_dir/arts_64t.cfg --pipeline=sde-planning | %FileCheck %s --implicit-check-not="kind = \"replicated\""

// CHECK-LABEL: func.func @symmetric_self_gram_row_owner_split
// CHECK: %[[DATA:.*]] = sde.mu_alloc
// CHECK: %[[OUT:.*]] = sde.mu_alloc : memref<64x16x1024xf64>
// CHECK: sde.array_layout_root write %[[OUT]] : memref<64x16x1024xf64>
// CHECK: arrayLayout =
// CHECK-SAME: muBlockCount = 64 : i64
// CHECK: sde.su_barrier
// CHECK-SAME: barrierReason = #sde.barrier_reason<unknown_required>
// CHECK: sde.su_iterate
// CHECK-SAME: classification(<elementwise>)
// CHECK: sde.array_layout_root read %[[DATA]]
// CHECK: sde.array_layout_root write %[[OUT]] : memref<64x16x1024xf64>
// CHECK: arrayLayout =
// CHECK-SAME: muBlockCount = 64 : i64
func.func @symmetric_self_gram_row_owner_split() {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c1024 = arith.constant 1024 : index
  %zero = arith.constant 0.0 : f64
  %one = arith.constant 1.0 : f64
  %data = sde.mu_alloc {arrayId = 0 : i64} : memref<1024x1024xf64>
  %out = sde.mu_alloc {arrayId = 1 : i64} : memref<1024x1024xf64>

  sde.su_iterate (%c0) to (%c1024) step (%c1) nowait {
  ^bb0(%i: index):
    sde.cu_region <parallel> {
      memref.store %one, %out[%i, %i] : memref<1024x1024xf64>
      %start = arith.addi %i, %c1 : index
      scf.for %j = %start to %c1024 step %c1 {
        %sum = scf.for %k = %c0 to %c1024 step %c1
            iter_args(%acc = %zero) -> (f64) {
          %lhs = memref.load %data[%i, %k] : memref<1024x1024xf64>
          %rhs = memref.load %data[%j, %k] : memref<1024x1024xf64>
          %prod = arith.mulf %lhs, %rhs : f64
          %next = arith.addf %acc, %prod : f64
          scf.yield %next : f64
        }
        memref.store %sum, %out[%i, %j] : memref<1024x1024xf64>
        memref.store %sum, %out[%j, %i] : memref<1024x1024xf64>
      }
      sde.yield
    }
    sde.yield
  }

  return
}
