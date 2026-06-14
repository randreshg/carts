// RUN: not %carts-compile %s --pass-pipeline='builtin.module(sde-rank-expand-mu,verify-sde-mu-access-window-sync)' 2>&1 | %FileCheck %s

// The producer writes %A and the consumer reads %B: no shared MU state crosses
// the barrier, so the ordering is redundant once both phases are fully
// described by query-derived access windows.

// CHECK: error: {{.*}}redundant
func.func @sync_redundant() {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c8 = arith.constant 8 : index
  %c4 = arith.constant 4 : index
  %zero = arith.constant 0.0 : f64
  %A = sde.mu_alloc {arrayId = 0 : i64} : memref<8x4xf64>
  %B = sde.mu_alloc {arrayId = 1 : i64} : memref<8x4xf64>
  sde.su_iterate (%c0, %c0) to (%c8, %c4) step (%c1, %c1) classification(<elementwise>) {
  ^bb0(%i: index, %j: index):
    sde.array_layout_root write %A : memref<8x4xf64> array_id(0)
    sde.cu_region <parallel> {
      memref.store %zero, %A[%i, %j] : memref<8x4xf64>
    }
    sde.yield
  } {arrayLayout = [{arrayId = 0 : i64, kind = "block_parallel", ownerDims = [0], blockShape = [4], muBlockCount = 8 : i64, role = "write"}]}
  sde.su_barrier
  sde.su_iterate (%c0, %c0) to (%c8, %c4) step (%c1, %c1) classification(<elementwise>) {
  ^bb0(%i: index, %j: index):
    sde.array_layout_root read %B : memref<8x4xf64> array_id(1)
    sde.cu_region <parallel> {
      %v = memref.load %B[%i, %j] : memref<8x4xf64>
      %ignore = arith.addf %v, %zero : f64
    }
    sde.yield
  } {arrayLayout = [{arrayId = 1 : i64, kind = "block_parallel", ownerDims = [0], blockShape = [4], muBlockCount = 8 : i64, role = "read"}]}
  return
}
