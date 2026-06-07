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
                  partition_score = {}} {
    ^bb0(%dep_arg: memref<4xf32>):
      codir.yield
    }
    return
  }

  func.func @codir_bad_partition_score_mu_blocks_unconsumed(%dep: memref<4xf32>) {
    codir.codelet deps(%dep : memref<4xf32>)
      attributes {dep_modes = [#codir.access_mode<read>],
                  partition_score = {muBlockCount = 0 : i64,
                                     targetLogicalWorkers = 1 : i64}} {
    ^bb0(%dep_arg: memref<4xf32>):
      codir.yield
    }
    return
  }

  func.func @codir_bad_partition_score_unconsumed_key(%dep: memref<4xf32>) {
    codir.codelet deps(%dep : memref<4xf32>)
      attributes {dep_modes = [#codir.access_mode<read>],
                  partition_score = {objective = "max_concurrency_comm_aware",
                                     targetLogicalWorkers = 1 : i64}} {
    ^bb0(%dep_arg: memref<4xf32>):
      codir.yield
    }
    return
  }

  func.func @codir_bad_partition_score_cu_group_size_unconsumed(%dep: memref<4xf32>) {
    codir.codelet deps(%dep : memref<4xf32>)
      attributes {dep_modes = [#codir.access_mode<read>],
                  partition_score = {cuGroupSize = 0 : i64,
                                     targetLogicalWorkers = 1 : i64}} {
    ^bb0(%dep_arg: memref<4xf32>):
      codir.yield
    }
    return
  }
}

// CHECK: partition_score must be a dictionary attribute
// CHECK: partition_score.targetLogicalWorkers must be a positive integer attribute
// CHECK: partition_score must contain targetLogicalWorkers or exposedCuCount
// CHECK: partition_score.muBlockCount is not consumed by the CODIR-to-ARTS boundary
// CHECK: partition_score.objective is not consumed by the CODIR-to-ARTS boundary
// CHECK: partition_score.cuGroupSize is not consumed by the CODIR-to-ARTS boundary
