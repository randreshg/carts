// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-redistribute)' 2>&1 | %FileCheck %s

// A committed replicated writer already has a write fact. Redistribution must
// not reinterpret the plain logical memref type as a rank-expanded block grid
// and append a second block_parallel write fact for the same array.

// CHECK-LABEL: func.func @preserve_replicated_writer_fact
// CHECK: arrayLayout = [{arrayId = 3 : i64
// CHECK-SAME: kind = "replicated"
// CHECK-SAME: ownerDims = []
// CHECK-NOT: {arrayId = 3 : i64{{.*}}kind = "block_parallel"

func.func @preserve_replicated_writer_fact(%C: memref<8x8xf64>) {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c8 = arith.constant 8 : index
  %one = arith.constant 1.0 : f64
  sde.su_iterate (%c0) to (%c8) step (%c1) {
  ^bb0(%i: index):
    sde.array_layout_root write %C : memref<8x8xf64> array_id(3)
    sde.cu_region <parallel> {
      memref.store %one, %C[%i, %i] : memref<8x8xf64>
      sde.yield
    }
    sde.yield
  } {arrayLayout = [
    {arrayId = 3 : i64, blockShape = [8, 8], budgetBlockShape = [8, 8],
     kind = "replicated", muBlockCount = 1 : i64, ownerDims = [],
     role = "write"}]}
  return
}
