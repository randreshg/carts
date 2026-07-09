// RUN: not %carts-compile %s --pass-pipeline='builtin.module()' 2>&1 | %FileCheck %s

// CHECK: 'arts.reduce_tree' op input type must match reduce_tree result type

module {
  func.func @arts_reduce_tree_rejects_input_type_mismatch(%lhs: f64, %rhs: f32) -> f64 {
    %sum = arts.reduce_tree <tree> (%lhs, %rhs : f64, f32)
        attributes {fan_in = 2 : i64, leaf_count = 2 : i64} {
      arts.yield %lhs : f64
    } : f64
    return %sum : f64
  }
}
