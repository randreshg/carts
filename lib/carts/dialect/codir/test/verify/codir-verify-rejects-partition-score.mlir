// RUN: not %carts-compile %s --pass-pipeline='builtin.module(verify-codir)' 2>&1 | %FileCheck %s

module {
  func.func @codir_bad_partition_score_type(%dep: memref<4xf32>) {
    codir.codelet deps(%dep : memref<4xf32>)
      attributes {dep_modes = [#codir.access_mode<read>],
                  partition_score = 7 : i64} {
    ^bb0(%dep_arg: memref<4xf32>):
      codir.yield
    }
    return
  }

  func.func @codir_bad_partition_score_field(%dep: memref<4xf32>) {
    codir.codelet deps(%dep : memref<4xf32>)
      attributes {dep_modes = [#codir.access_mode<read>],
                  partition_score = {targetLogicalWorkers = 0 : i64}} {
    ^bb0(%dep_arg: memref<4xf32>):
      codir.yield
    }
    return
  }

  func.func @codir_bad_partition_score_missing_concurrency(%dep: memref<4xf32>) {
    codir.codelet deps(%dep : memref<4xf32>)
      attributes {dep_modes = [#codir.access_mode<read>],
                  partition_score = {chosenCuCount = 4 : i64}} {
    ^bb0(%dep_arg: memref<4xf32>):
      codir.yield
    }
    return
  }
}

// CHECK: partition_score must be a dictionary attribute
// CHECK: partition_score.targetLogicalWorkers must be a positive integer attribute
// CHECK: partition_score must contain targetLogicalWorkers or exposedCuCount
