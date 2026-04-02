// RUN: not %carts-compile --print-stage-manifest-json 2>&1 | %FileCheck %s

// CHECK: Unknown command line argument '--print-stage-manifest-json'
// CHECK: Did you mean '--print-pipeline-manifest-json'?
