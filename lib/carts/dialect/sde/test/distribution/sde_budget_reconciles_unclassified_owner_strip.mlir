// RUN: %carts-compile %s --O3 --arts-config %inputs_dir/arts_64t.cfg --pipeline=sde-planning | %FileCheck %s --implicit-check-not=budgetBlockShape --implicit-check-not="memref<2x50000x30xf32>"

// CHECK-LABEL: func.func @unclassified_owner_strip_budget_reconciles_to_worker_grain
// CHECK-DAG: %[[A:.*]] = sde.mu_alloc : memref<64x1563x30xf32>
// CHECK-DAG: %[[C:.*]] = sde.mu_alloc : memref<64x1563x30xf32>
// CHECK: sde.su_iterate {{.*}} step (%{{.*}})
// CHECK: sde.array_layout_root write %[[C]] : memref<64x1563x30xf32> array_id(0)
// CHECK: sde.array_layout_root read %[[A]] : memref<64x1563x30xf32> array_id(1)
// CHECK: memref.load %[[A]][{{.*}}] : memref<64x1563x30xf32>
// CHECK: memref.store %{{.*}}, %[[C]][{{.*}}] : memref<64x1563x30xf32>
// CHECK: arrayId = 0 : i64, blockShape = [1563, 30], kind = "block_parallel", muBlockCount = 64 : i64, ownerDims = [0], role = "write"
// CHECK: arrayId = 1 : i64, blockShape = [1563, 30], kind = "block_parallel", muBlockCount = 64 : i64, ownerDims = [0], role = "read"

func.func @unclassified_owner_strip_budget_reconciles_to_worker_grain() {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c30 = arith.constant 30 : index
  %c100000 = arith.constant 100000 : index
  %zero = arith.constant 0.0 : f32
  %A = sde.mu_alloc {arrayId = 1 : i64} : memref<100000x30xf32>
  %C = sde.mu_alloc {arrayId = 0 : i64} : memref<100000x30xf32>

  sde.su_iterate (%c0) to (%c100000) step (%c1) {
  ^bb0(%i: index):
    sde.array_layout_root write %C : memref<100000x30xf32> array_id(0)
    sde.array_layout_root read %A : memref<100000x30xf32> array_id(1)
    sde.cu_region <parallel> {
      %scratch = memref.alloca() : memref<30xf32>
      scf.for %j = %c0 to %c30 step %c1 {
        %v = memref.load %A[%i, %j] : memref<100000x30xf32>
        %sum = arith.addf %v, %zero : f32
        memref.store %sum, %scratch[%j] : memref<30xf32>
      }
      scf.for %j = %c0 to %c30 step %c1 {
        %v = memref.load %scratch[%j] : memref<30xf32>
        memref.store %v, %C[%i, %j] : memref<100000x30xf32>
      }
      sde.yield
    }
    sde.yield
  } {arrayLayout = [
      {arrayId = 0 : i64, blockShape = [50000, 30],
       budgetBlockShape = [16667, 30], kind = "block_parallel",
       muBlockCount = 2 : i64, ownerDims = [0], role = "write"},
      {arrayId = 1 : i64, blockShape = [50000, 30],
       budgetBlockShape = [16667, 30], kind = "block_parallel",
       muBlockCount = 2 : i64, ownerDims = [0], role = "read"}]}

  return
}
