# opt/codegen/ -- Code Generation Optimization Passes

Late-stage passes that improve generated code quality.

| Pass | Stage | Purpose |
|------|-------|---------|
| Hoisting | late-concurrency-cleanup | Hoist invariant ops out of EDT regions |
| DataPtrHoisting | late-concurrency-cleanup | Hoist DB data pointer loads |
| AliasScopeGen | late-concurrency-cleanup | Generate alias scope metadata |
| GuidRangCallOpt | late-concurrency-cleanup | Optimize GUID range calls |
| RuntimeCallOpt | late-concurrency-cleanup | Optimize ARTS runtime call sequences |
| LoopVectorizationHints | late-concurrency-cleanup | Emit loop vectorization metadata |
| ScalarReplacement | late-concurrency-cleanup | Scalar replacement of aggregates |

`DataPtrHoistingSupport.cpp` contains extracted helpers.
