// RUN: %carts-compile --print-pipeline-tokens-json | %FileCheck %s

// CHECK: "pipeline": [{{.*}}"canonicalize-memrefs"{{.*}}"arts-to-llvm"{{.*}}"complete"{{.*}}]
// CHECK: "start_from": [{{.*}}"canonicalize-memrefs"{{.*}}"arts-to-llvm"{{.*}}]
// CHECK: "pipeline_sequence": [{{.*}}"canonicalize-memrefs"{{.*}}"arts-to-llvm"{{.*}}]
// CHECK-NOT: "start_from": [{{.*}}"complete"{{.*}}]
// CHECK-NOT: "pipeline_sequence": [{{.*}}"complete"{{.*}}]
// CHECK-NOT: "aliases"
