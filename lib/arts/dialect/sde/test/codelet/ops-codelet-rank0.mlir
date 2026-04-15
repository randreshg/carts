// RUN: %carts-compile %s --arts-config %arts_config --pipeline initial-cleanup --start-from initial-cleanup 2>&1 | %FileCheck %s

// Phase 0 verification: rank-0 tensor (scalar) support through
// mu_data / mu_token / cu_codelet. This is a prerequisite for the
// tensor-first pipeline — cu_region <single> bodies operate on scalars
// like `sum += 1000` which become tensor<i32> after raising.

module {
  // Rank-0 mu_data roundtrip.
  // CHECK-LABEL: func.func @mu_data_rank0
  func.func @mu_data_rank0() -> tensor<i32> {
    // CHECK: %[[D:.*]] = arts_sde.mu_data shared : tensor<i32>
    %d = arts_sde.mu_data shared : tensor<i32>
    // CHECK: return %[[D]] : tensor<i32>
    func.return %d : tensor<i32>
  }

  // Rank-0 whole-tensor token + codelet roundtrip: readwrite scalar.
  // CHECK-LABEL: func.func @codelet_rank0_readwrite
  // CHECK-SAME: %[[T:.*]]: tensor<i32>
  func.func @codelet_rank0_readwrite(%t: tensor<i32>) -> tensor<i32> {
    // CHECK: %[[TOK:.*]] = arts_sde.mu_token <readwrite> %[[T]] : tensor<i32> -> !arts_sde.token<tensor<i32>>
    %token = arts_sde.mu_token <readwrite> %t
      : tensor<i32> -> !arts_sde.token<tensor<i32>>

    // CHECK: %[[R:.*]] = arts_sde.cu_codelet(%[[TOK]] : !arts_sde.token<tensor<i32>>) -> (tensor<i32>)
    %r = arts_sde.cu_codelet (%token : !arts_sde.token<tensor<i32>>)
        -> (tensor<i32>) {
    // CHECK: ^bb0(%[[ARG:.*]]: tensor<i32>)
    ^bb0(%arg: tensor<i32>):
      %val = tensor.extract %arg[] : tensor<i32>
      %c1000 = arith.constant 1000 : i32
      %added = arith.addi %val, %c1000 : i32
      %result = tensor.insert %added into %arg[] : tensor<i32>
      arts_sde.yield %result : tensor<i32>
    }

    // CHECK: return %[[R]] : tensor<i32>
    func.return %r : tensor<i32>
  }

  // Rank-0 read-only codelet: no result.
  // CHECK-LABEL: func.func @codelet_rank0_read_only
  func.func @codelet_rank0_read_only(%t: tensor<i32>) {
    // CHECK: arts_sde.mu_token <read>
    %token = arts_sde.mu_token <read> %t
      : tensor<i32> -> !arts_sde.token<tensor<i32>>

    // CHECK: arts_sde.cu_codelet
    arts_sde.cu_codelet (%token : !arts_sde.token<tensor<i32>>) {
    ^bb0(%arg: tensor<i32>):
      %val = tensor.extract %arg[] : tensor<i32>
      %buf = memref.alloca() : memref<1xi32>
      %c0 = arith.constant 0 : index
      memref.store %val, %buf[%c0] : memref<1xi32>
      arts_sde.yield
    }
    func.return
  }
}
