// RUN: not %carts-compile %s --pass-pipeline='builtin.module(verify-codir)' 2>&1 | %FileCheck %s

module {
  func.func @codir_bad_dep_collective_count(%dep0: memref<4xf32>, %dep1: memref<4xf32>) {
    codir.codelet deps(%dep0, %dep1 : memref<4xf32>, memref<4xf32>)
      attributes {dep_collectives = [#codir.collective<none>],
                  dep_modes = [#codir.access_mode<read>, #codir.access_mode<read>]} {
    ^bb0(%dep0_arg: memref<4xf32>, %dep1_arg: memref<4xf32>):
      codir.yield
    }
    return
  }

  func.func @codir_bad_dep_collective_attr(%dep: memref<4xf32>) {
    codir.codelet deps(%dep : memref<4xf32>)
      attributes {dep_collectives = [#codir.access_mode<read>],
                  dep_modes = [#codir.access_mode<read>]} {
    ^bb0(%dep_arg: memref<4xf32>):
      codir.yield
    }
    return
  }
}

// CHECK: expects dep_collectives entry count (1) to match dependency operand count (2)
// CHECK: dep_collectives entry #0 must be a CODIR collective attribute
