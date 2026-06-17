// RUN: %carts-compile %s --O3 --arts-config %inputs_dir/arts_64t.cfg --pipeline=sde-planning | %FileCheck %s --implicit-check-not=budgetBlockShape

// CHECK-LABEL: func.func @matmul_layout_choice_commits_worker_grain
// CHECK: %[[A:.*]] = sde.mu_alloc : memref<4x256x1024xf64>
// CHECK: %[[B:.*]] = sde.mu_alloc : memref<4x1024x256xf64>
// CHECK: %[[C:.*]] = sde.mu_alloc : memref<8x8x128x128xf64>
// CHECK: sde.su_iterate {{.*}} step (%{{.*}}, %{{.*}}) nowait classification(<matmul>)
// CHECK: arrayLayout = [{arrayId = 0 : i64, blockShape = [128, 128], kind = "block_parallel", muBlockCount = 64 : i64, ownerDims = [0, 1], role = "write"}
// CHECK-NOT: memref<2x2x512x512xf64>

func.func @matmul_layout_choice_commits_worker_grain() {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c1024 = arith.constant 1024 : index
  %zero = arith.constant 0.0 : f64
  %beta = arith.constant 2.0 : f64
  %a = sde.mu_alloc {arrayId = 1 : i64} : memref<1024x1024xf64>
  %b = sde.mu_alloc {arrayId = 2 : i64} : memref<1024x1024xf64>
  %c = sde.mu_alloc {arrayId = 0 : i64} : memref<1024x1024xf64>

  sde.su_iterate (%c0, %c0) to (%c1024, %c1024) step (%c1, %c1) nowait
      classification(<matmul>) {
  ^bb0(%i: index, %j: index):
    sde.array_layout_root write %c : memref<1024x1024xf64> array_id(0)
    sde.array_layout_root read %a : memref<1024x1024xf64> array_id(1)
    sde.array_layout_root read %b : memref<1024x1024xf64> array_id(2)
    sde.cu_region <parallel> {
      %sum = scf.for %k = %c0 to %c1024 step %c1
          iter_args(%acc = %zero) -> (f64) {
        %av = memref.load %a[%i, %k] : memref<1024x1024xf64>
        %bv = memref.load %b[%k, %j] : memref<1024x1024xf64>
        %prod = arith.mulf %av, %bv : f64
        %next = arith.addf %acc, %prod : f64
        scf.yield %next : f64
      }
      %old = memref.load %c[%i, %j] : memref<1024x1024xf64>
      %scaled = arith.mulf %old, %beta : f64
      %next = arith.addf %sum, %scaled : f64
      memref.store %next, %c[%i, %j] : memref<1024x1024xf64>
      sde.yield
    }
    sde.yield
  }

  return
}
