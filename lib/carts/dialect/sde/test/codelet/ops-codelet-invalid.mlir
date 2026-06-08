// Invalid-IR tests for the memref-native SDE codelet surface. Each RUN line
// invokes `carts-compile` on IR that violates a verifier rule; `not` flips the
// exit code so the run succeeds iff compilation fails.
//
// The rules exercised here are:
//   RANK_MISMATCH     — `mu_token` offsets/sizes must match source rank.
//   BLOCK_ARG_TYPE    — `cu_work` block arguments must match token slice types.
//   YIELD_VALUES      — memref codelets must not yield replacement values.
//   NONSCALAR_CAPTURE — `cu_work` captures must be scalar values.
//
// RUN: not %carts-compile %s --arts-config %arts_config --pipeline initial-cleanup --start-from initial-cleanup 2>&1 | %FileCheck %s --check-prefix=RANK_MISMATCH
// RUN: not %carts-compile %S/Inputs/codelet-invalid-block-arg-type.mlir --arts-config %arts_config --pipeline initial-cleanup --start-from initial-cleanup 2>&1 | %FileCheck %s --check-prefix=BLOCK_ARG_TYPE
// RUN: not %carts-compile %S/Inputs/codelet-invalid-yield-values.mlir --arts-config %arts_config --pipeline initial-cleanup --start-from initial-cleanup 2>&1 | %FileCheck %s --check-prefix=YIELD_VALUES
// RUN: not %carts-compile %S/Inputs/codelet-invalid-nonscalar-capture.mlir --arts-config %arts_config --pipeline initial-cleanup --start-from initial-cleanup 2>&1 | %FileCheck %s --check-prefix=NONSCALAR_CAPTURE

// RANK_MISMATCH: 'sde.mu_token' op expects offsets/sizes count (2) to match source rank (1)

// BLOCK_ARG_TYPE: 'sde.cu_work' op block argument #0 type ('memref<8xi32>') does not match token slice type ('memref<4xi32>')

// YIELD_VALUES: 'sde.cu_work' op expects memref compute-unit yield to carry no values

// NONSCALAR_CAPTURE: 'sde.cu_work' op capture operand #0 must be an integer, index, or float scalar

module {
  func.func @rank_mismatch(%m: memref<8xi32>) {
    %c0 = arith.constant 0 : index
    %c4 = arith.constant 4 : index
    %token = sde.mu_token <read> %m [%c0, %c0] size [%c4, %c4]
      : memref<8xi32> -> !sde.token<memref<4xi32>>
    func.return
  }
}
