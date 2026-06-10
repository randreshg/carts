// RUN: not %carts-compile %s \
// RUN:   --pass-pipeline='builtin.module(verify-sde-partition-plan)' 2>&1 \
// RUN:   | %FileCheck %s

// A committed partition_graph whose primary-MU (owner_block) blockShape no
// longer matches the committed physicalBlockShape: a later SDE pass must
// preserve the committed CU/MU partition evidence, so the stale graph grain
// fails closed.
// CHECK: no longer matches the SDE physical plan
func.func @partition_graph_block_diverges(%A: memref<10240x10240xf64>) {
  %c0 = arith.constant 0 : index
  %c10240 = arith.constant 10240 : index
  %c1 = arith.constant 1 : index
  %cst = arith.constant 0.000000e+00 : f64
  sde.su_iterate (%c0, %c0) to (%c10240, %c10240) step (%c1, %c1)
      classification(<elementwise>) {
  ^bb0(%i: index, %j: index):
    sde.cu_region <single> {
      memref.store %cst, %A[%i, %j] : memref<10240x10240xf64>
      sde.yield
    }
    sde.yield
  } {physicalBlockShape = [512, 512],
     physicalOwnerDims = [0, 1],
     partitionGraph = [{blockShape = [256, 512], cuGroupCount = 1 : i64,
                        cuGroupSize = 1 : i64, edgeClass = "aligned",
                        edgeCommBytes = 0 : i64, layoutKind = "owner_block",
                        muBlockCount = 1 : i64, muId = 0 : i64,
                        ownerDims = [0, 1], role = "write",
                        tilePayloadBytes = 0 : i64}],
     partitionScore = {exposedCuCount = 8 : i64, targetLogicalWorkers = 8 : i64}}
  return
}

// -----

// Partition evidence must stay runtime-neutral: a stray non-SDE value (a
// collective family) leaking into the partition graph fails closed.
// CHECK: must stay runtime-neutral
func.func @partition_graph_not_runtime_neutral(%A: memref<10240x10240xf64>) {
  %c0 = arith.constant 0 : index
  %c10240 = arith.constant 10240 : index
  %c1 = arith.constant 1 : index
  %cst = arith.constant 0.000000e+00 : f64
  sde.su_iterate (%c0, %c0) to (%c10240, %c10240) step (%c1, %c1)
      classification(<elementwise>) {
  ^bb0(%i: index, %j: index):
    sde.cu_region <single> {
      memref.store %cst, %A[%i, %j] : memref<10240x10240xf64>
      sde.yield
    }
    sde.yield
  } {physicalBlockShape = [512, 512],
     physicalOwnerDims = [0, 1],
     partitionGraph = [{blockShape = [512, 512], cuGroupCount = 1 : i64,
                        cuGroupSize = 1 : i64, edgeClass = "all_gather",
                        edgeCommBytes = 0 : i64, layoutKind = "owner_block",
                        muBlockCount = 1 : i64, muId = 0 : i64,
                        ownerDims = [0, 1], role = "write",
                        tilePayloadBytes = 0 : i64}],
     partitionScore = {exposedCuCount = 8 : i64, targetLogicalWorkers = 8 : i64}}
  return
}
