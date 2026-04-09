# opt/edt/ -- EDT Optimization Passes

Passes that optimize Event-Driven Task structure and execution.

| Pass | Stage | Purpose |
|------|-------|---------|
| EdtStructuralOpt | edt-transforms | Structural EDT optimizations |
| EdtAllocaSinking | edt-transforms | Sink allocas into EDT regions |
| EdtICM | edt-transforms | EDT invariant code motion |
| EdtTransformsPass | edt-transforms | Composite EDT optimization driver |
| StructuredKernelPlanPass | edt-opt | Plan structured kernel launch patterns |
| EdtOrchestrationOpt | edt-distribution | Stamp repeated-wave orchestration contracts |
| PersistentStructuredRegion | edt-opt | Persistent structured region cost gating |
