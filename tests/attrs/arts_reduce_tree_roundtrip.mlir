// RUN: %carts-compile %s --pass-pipeline='builtin.module()' | %FileCheck %s

// CHECK-LABEL: func.func @arts_reduce_tree_roundtrip
// CHECK: %[[SUM:.*]] = arts.reduce_tree <tree>(%{{.*}}, %{{.*}} : f64, f64) attributes {fan_in = 2 : i64, leaf_count = 2 : i64}
// CHECK: arts.yield %[[ADD:.*]] : f64
// CHECK: return %[[SUM]] : f64

module {
  func.func @arts_reduce_tree_roundtrip(%lhs: f64, %rhs: f64) -> f64 {
    %sum = arts.reduce_tree <tree> (%lhs, %rhs : f64, f64)
        attributes {fan_in = 2 : i64, leaf_count = 2 : i64} {
      %combined = arith.addf %lhs, %rhs : f64
      arts.yield %combined : f64
    } : f64
    return %sum : f64
  }
}
