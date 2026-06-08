// Auxiliary input for ops-codelet-invalid.mlir (yield value check).
// Memref codelets mutate through token block arguments and must not yield
// replacement values.
module {
  func.func @yield_value_from_memref_codelet(%m: memref<8xi32>) {
    %token = sde.mu_token <readwrite> %m
      : memref<8xi32> -> !sde.token<memref<8xi32>>
    sde.cu_work (%token : !sde.token<memref<8xi32>>) {
    ^bb0(%arg: memref<8xi32>):
      sde.yield %arg : memref<8xi32>
    }
    func.return
  }
}
