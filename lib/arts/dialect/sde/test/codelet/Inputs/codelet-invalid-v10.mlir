// Auxiliary input for ops-codelet-invalid.mlir (V10 check).
// V10: a <read> token cannot participate in the codelet yield.
//     Here we attempt to yield through two read tokens (no writable tokens),
//     triggering the V7 result-count gate whose diagnostic explicitly calls
//     out that <read> tokens must not have yielded counterparts.
module {
  func.func @v10_yield_through_read(%a: tensor<8xi32>, %b: tensor<8xi32>)
      -> (tensor<8xi32>, tensor<8xi32>) {
    %c0 = arith.constant 0 : index
    %c4 = arith.constant 4 : index
    %ta = arts_sde.mu_token <read> %a [%c0] size [%c4]
      : tensor<8xi32> -> !arts_sde.token<tensor<4xi32>>
    %tb = arts_sde.mu_token <read> %b [%c0] size [%c4]
      : tensor<8xi32> -> !arts_sde.token<tensor<4xi32>>
    %ra, %rb = arts_sde.cu_codelet
        (%ta, %tb : !arts_sde.token<tensor<4xi32>>, !arts_sde.token<tensor<4xi32>>)
        -> (tensor<8xi32>, tensor<8xi32>) {
    ^bb0(%arga: tensor<4xi32>, %argb: tensor<4xi32>):
      arts_sde.yield %arga, %argb : tensor<4xi32>, tensor<4xi32>
    }
    func.return %ra, %rb : tensor<8xi32>, tensor<8xi32>
  }
}
