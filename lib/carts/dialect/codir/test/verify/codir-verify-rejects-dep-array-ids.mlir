// RUN: not %carts-compile %s --pass-pipeline='builtin.module(verify-codir)' 2>&1 | %FileCheck %s

module {
  func.func @codir_bad_dep_array_id_count(%dep0: memref<4xf32>, %dep1: memref<4xf32>) {
    codir.codelet deps(%dep0, %dep1 : memref<4xf32>, memref<4xf32>)
      attributes {array_layout = [{arrayId = 0 : i64}],
                  dep_array_ids = [0],
                  dep_modes = [#codir.access_mode<read>, #codir.access_mode<read>]} {
    ^bb0(%dep0_arg: memref<4xf32>, %dep1_arg: memref<4xf32>):
      codir.yield
    }
    return
  }

  func.func @codir_bad_dep_array_id_attr(%dep: memref<4xf32>) {
    codir.codelet deps(%dep : memref<4xf32>)
      attributes {array_layout = [{arrayId = 0 : i64}],
                  dep_array_ids = [#codir.access_mode<read>],
                  dep_modes = [#codir.access_mode<read>]} {
    ^bb0(%dep_arg: memref<4xf32>):
      codir.yield
    }
    return
  }

  func.func @codir_bad_dep_array_id_negative(%dep: memref<4xf32>) {
    codir.codelet deps(%dep : memref<4xf32>)
      attributes {array_layout = [{arrayId = 0 : i64}],
                  dep_array_ids = [-2],
                  dep_modes = [#codir.access_mode<read>]} {
    ^bb0(%dep_arg: memref<4xf32>):
      codir.yield
    }
    return
  }

  func.func @codir_bad_dep_array_id_missing_layout(%dep: memref<4xf32>) {
    codir.codelet deps(%dep : memref<4xf32>)
      attributes {array_layout = [{arrayId = 0 : i64}],
                  dep_array_ids = [9],
                  dep_modes = [#codir.access_mode<read>]} {
    ^bb0(%dep_arg: memref<4xf32>):
      codir.yield
    }
    return
  }
}

// CHECK: expects dep_array_ids entry count (1) to match dependency operand count (2)
// CHECK: dep_array_ids entry #0 must be an integer attribute
// CHECK: dep_array_ids entry #0 must be -1 or a non-negative array id
// CHECK: dep_array_ids entry #0 references arrayId 9 not present in array_layout
