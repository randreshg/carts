// RUN: not %carts-compile %s --pass-pipeline='builtin.module(canonicalize)' 2>&1 | %FileCheck %s

// CHECK: arrayLayout entry #0 must include non-negative integer arrayId
func.func @malformed_array_layout_entry() {
  %c0 = arith.constant 0 : index
  %c8 = arith.constant 8 : index
  %c1 = arith.constant 1 : index
  sde.su_iterate (%c0) to (%c8) step (%c1) {
  ^bb0(%i: index):
    sde.yield
  } {arrayLayout = [{kind = "block_parallel", ownerDims = [0], blockShape = [4], role = "write"}]}
  return
}
