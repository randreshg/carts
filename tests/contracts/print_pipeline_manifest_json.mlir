// RUN: %carts-compile --print-pipeline-manifest-json | %FileCheck %s

// CHECK: "pipeline": [{{.*}}"canonicalize-memrefs"{{.*}}"arts-to-llvm"{{.*}}"complete"{{.*}}]
// CHECK: "start_from": [{{.*}}"canonicalize-memrefs"{{.*}}"arts-to-llvm"{{.*}}]
// CHECK: "pipeline_sequence": [{{.*}}"canonicalize-memrefs"{{.*}}"arts-to-llvm"{{.*}}]
// CHECK-NOT: "start_from": [{{.*}}"complete"{{.*}}]
// CHECK-NOT: "pipeline_sequence": [{{.*}}"complete"{{.*}}]
// CHECK-NOT: "aliases"
// CHECK: "pipeline_steps": [
// CHECK: {"name": "canonicalize-memrefs", "passes": [{{.*}}"CanonicalizeMemrefs"{{.*}}]}
// CHECK: {"name": "create-dbs", "passes": [{{.*}}"CreateDbs"{{.*}}"CSE (bridge cleanup, conditional)"{{.*}}"VerifyDbCreated"{{.*}}]}
// CHECK: {"name": "epochs", "passes": [{{.*}}"CreateEpochs"{{.*}}"EpochContinuationPrep (conditional)"{{.*}}]}
// CHECK: {"name": "arts-to-llvm", "passes": [{{.*}}"ConvertArtsToLLVM"{{.*}}"VerifyLowered"{{.*}}]}
