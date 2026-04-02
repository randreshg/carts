// RUN: %carts-compile --print-pipeline-manifest-json | %FileCheck %s

// CHECK: "pipeline": [{{.*}}"raise-memref-dimensionality"{{.*}}"late-concurrency-cleanup"{{.*}}"arts-to-llvm"{{.*}}"complete"{{.*}}]
// CHECK: "start_from": [{{.*}}"raise-memref-dimensionality"{{.*}}"post-distribution-cleanup"{{.*}}"late-concurrency-cleanup"{{.*}}"arts-to-llvm"{{.*}}]
// CHECK: "pipeline_sequence": [{{.*}}"raise-memref-dimensionality"{{.*}}"edt-distribution"{{.*}}"post-distribution-cleanup"{{.*}}"db-partitioning"{{.*}}"post-db-refinement"{{.*}}"late-concurrency-cleanup"{{.*}}"epochs"{{.*}}"arts-to-llvm"{{.*}}]
// CHECK-NOT: "start_from": [{{.*}}"complete"{{.*}}]
// CHECK-NOT: "pipeline_sequence": [{{.*}}"complete"{{.*}}]
// CHECK-NOT: "aliases"
// CHECK: "pipeline_steps": [
// CHECK: {"name": "raise-memref-dimensionality", "passes": [{{.*}}"RaiseMemRefDimensionality"{{.*}}]}
// CHECK: {"name": "create-dbs", "passes": [{{.*}}"CreateDbs"{{.*}}"CSE (bridge cleanup, conditional)"{{.*}}"VerifyDbCreated"{{.*}}]}
// CHECK: {"name": "db-partitioning", "passes": [{{.*}}"DbPartitioning"{{.*}}"DbDistributedOwnership (conditional)"{{.*}}"DbTransforms"{{.*}}]}
// CHECK: {"name": "post-db-refinement", "passes": [{{.*}}"DbModeTightening"{{.*}}"ContractValidation"{{.*}}]}
// CHECK: {"name": "late-concurrency-cleanup", "passes": [{{.*}}"BlockLoopStripMining(func)"{{.*}}"Hoisting"{{.*}}"Mem2Reg"{{.*}}]}
// CHECK: {"name": "epochs", "passes": [{{.*}}"CreateEpochs"{{.*}}"EpochOpt[scheduling] (conditional)"{{.*}}]}
// CHECK: {"name": "arts-to-llvm", "passes": [{{.*}}"ConvertArtsToLLVM"{{.*}}"VerifyLowered"{{.*}}]}
// CHECK: "epilogue_steps": [
// CHECK: {"name": "post-o3-opt", "passes": [{{.*}}"LICM"{{.*}}]}
// CHECK: {"name": "llvm-ir-emission", "passes": [{{.*}}"ConvertPolygeistToLLVM"{{.*}}"LoopVectorizationHints"{{.*}}]}
