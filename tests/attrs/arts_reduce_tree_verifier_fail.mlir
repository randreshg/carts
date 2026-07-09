// RUN: not %carts-compile %s --pass-pipeline='builtin.module()' 2>&1 | %FileCheck %s

// CHECK: 'arts.reduce_tree' op body must yield exactly one value

module {
  func.func @arts_reduce_tree_rejects_missing_yield_value(%lhs: f64, %rhs: f64) -> f64 {
    %sum = arts.reduce_tree <tree> (%lhs, %rhs : f64, f64)
        attributes {fan_in = 2 : i64, leaf_count = 2 : i64} {
      arts.yield
    } : f64
    return %sum : f64
  }
}
