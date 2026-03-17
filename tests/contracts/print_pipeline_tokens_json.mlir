// RUN: %carts-compile --print-pipeline-tokens-json | %FileCheck %s

// CHECK: "pipeline": [{{.*}}"raise-memref-dimensionality"{{.*}}"arts-to-llvm"{{.*}}"complete"{{.*}}]
// CHECK: "start_from": [{{.*}}"raise-memref-dimensionality"{{.*}}"arts-to-llvm"{{.*}}]
// CHECK: "pipeline_sequence": [{{.*}}"raise-memref-dimensionality"{{.*}}"arts-to-llvm"{{.*}}]
// CHECK-NOT: "start_from": [{{.*}}"complete"{{.*}}]
// CHECK-NOT: "pipeline_sequence": [{{.*}}"complete"{{.*}}]
// CHECK-NOT: "aliases"
