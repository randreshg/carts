// RUN: %carts-compile %s --O3 --arts-config %inputs_dir/arts_64t.cfg --pipeline=sde-planning | %FileCheck %s --implicit-check-not=budgetBlockShape

// CHECK-LABEL: func.func @matmul_budget_grain_rank_expand
// CHECK: %[[A:.*]] = sde.mu_alloc : memref<4x256x1024xf64>
// CHECK: %[[B:.*]] = sde.mu_alloc : memref<4x1024x256xf64>
// CHECK: %[[C:.*]] = sde.mu_alloc : memref<8x8x128x128xf64>
// CHECK: sde.su_iterate
// CHECK-SAME: classification(<matmul>)
// CHECK: sde.array_layout_root write %[[C]]
// CHECK: sde.array_layout_root read %[[A]]
// CHECK: sde.array_layout_root read %[[B]]
// CHECK: memref.store {{.*}}, %[[C]][{{.*}}] : memref<8x8x128x128xf64>
// CHECK: memref.load %[[A]][{{.*}}] : memref<4x256x1024xf64>
// CHECK: memref.load %[[B]][{{.*}}] : memref<4x1024x256xf64>
// CHECK: memref.load %[[C]][{{.*}}] : memref<8x8x128x128xf64>
// CHECK: memref.store {{.*}}, %[[C]][{{.*}}] : memref<8x8x128x128xf64>
// CHECK-LABEL: func.func @matmul_scaled_in_place_init_reorders_k_before_columns
// CHECK: %[[A:.*]] = sde.mu_alloc : memref<4x256x1024xf64>
// CHECK: %[[B:.*]] = sde.mu_alloc : memref<4x1024x256xf64>
// CHECK: %[[C:.*]] = sde.mu_alloc : memref<8x8x128x128xf64>
// CHECK: scf.for %[[ROW:.*]] =
// CHECK: scf.for %[[INIT_COL:.*]] =
// CHECK: memref.load %[[C]]
// CHECK: memref.store {{.*}}, %[[C]]
// CHECK: scf.for %[[K:.*]] =
// CHECK: scf.for %[[COL:.*]] =
// CHECK: memref.load %[[A]]
// CHECK: memref.load %[[B]]
// CHECK: memref.load %[[C]]
// CHECK: memref.store {{.*}}, %[[C]]

func.func @matmul_budget_grain_rank_expand() {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c1024 = arith.constant 1024 : index
  %zero = arith.constant 0.0 : f64
  %a = sde.mu_alloc {arrayId = 4 : i64} : memref<1024x1024xf64>
  %b = sde.mu_alloc {arrayId = 5 : i64} : memref<1024x1024xf64>
  %c = sde.mu_alloc {arrayId = 3 : i64} : memref<1024x1024xf64>

  sde.su_iterate (%c0, %c0) to (%c1024, %c1024) step (%c1, %c1) nowait
      classification(<matmul>) {
  ^bb0(%i: index, %j: index):
    sde.array_layout_root write %c : memref<1024x1024xf64> array_id(0)
    sde.array_layout_root read %a : memref<1024x1024xf64> array_id(1)
    sde.array_layout_root read %b : memref<1024x1024xf64> array_id(2)
    sde.cu_region <parallel> {
      memref.store %zero, %c[%i, %j] : memref<1024x1024xf64>
      scf.for %k = %c0 to %c1024 step %c1 {
        %av = memref.load %a[%i, %k] : memref<1024x1024xf64>
        %bv = memref.load %b[%k, %j] : memref<1024x1024xf64>
        %prod = arith.mulf %av, %bv : f64
        %old = memref.load %c[%i, %j] : memref<1024x1024xf64>
        %next = arith.addf %old, %prod : f64
        memref.store %next, %c[%i, %j] : memref<1024x1024xf64>
      }
      sde.yield
    }
    sde.yield
  } {arrayLayout = [{arrayId = 0 : i64, blockShape = [512, 512], kind = "block_parallel", muBlockCount = 4 : i64, ownerDims = [0, 1], role = "write"}, {arrayId = 1 : i64, blockShape = [512, 1024], budgetBlockShape = [256, 1024], kind = "block_parallel", muBlockCount = 2 : i64, ownerDims = [0], role = "read"}, {arrayId = 2 : i64, blockShape = [1024, 512], budgetBlockShape = [1024, 256], kind = "block_parallel", muBlockCount = 2 : i64, ownerDims = [1], role = "read"}]}

  return
}

func.func @matmul_scaled_in_place_init_reorders_k_before_columns() {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c1024 = arith.constant 1024 : index
  %beta = arith.constant 2.0 : f64
  %a = sde.mu_alloc {arrayId = 1 : i64} : memref<1024x1024xf64>
  %b = sde.mu_alloc {arrayId = 2 : i64} : memref<1024x1024xf64>
  %c = sde.mu_alloc {arrayId = 0 : i64} : memref<1024x1024xf64>

  sde.su_iterate (%c0, %c0) to (%c1024, %c1024) step (%c1, %c1) nowait
      classification(<matmul>) {
  ^bb0(%i: index, %j: index):
    sde.array_layout_root read %a : memref<1024x1024xf64> array_id(4)
    sde.array_layout_root read %b : memref<1024x1024xf64> array_id(5)
    sde.array_layout_root write %c : memref<1024x1024xf64> array_id(3)
    sde.cu_region <parallel> {
      %old0 = memref.load %c[%i, %j] : memref<1024x1024xf64>
      %scaled = arith.mulf %old0, %beta : f64
      memref.store %scaled, %c[%i, %j] : memref<1024x1024xf64>
      scf.for %k = %c0 to %c1024 step %c1 {
        %av = memref.load %a[%i, %k] : memref<1024x1024xf64>
        %bv = memref.load %b[%k, %j] : memref<1024x1024xf64>
        %prod = arith.mulf %av, %bv : f64
        %old = memref.load %c[%i, %j] : memref<1024x1024xf64>
        %next = arith.addf %old, %prod : f64
        memref.store %next, %c[%i, %j] : memref<1024x1024xf64>
      }
      sde.yield
    }
    sde.yield
  } {arrayLayout = [{arrayId = 3 : i64, blockShape = [512, 512], kind = "block_parallel", muBlockCount = 4 : i64, ownerDims = [0, 1], role = "write"}, {arrayId = 4 : i64, blockShape = [512, 1024], budgetBlockShape = [256, 1024], kind = "block_parallel", muBlockCount = 2 : i64, ownerDims = [0], role = "read"}, {arrayId = 5 : i64, blockShape = [1024, 512], budgetBlockShape = [1024, 256], kind = "block_parallel", muBlockCount = 2 : i64, ownerDims = [1], role = "read"}]}

  return
}
