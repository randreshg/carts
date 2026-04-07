# opt/loop/ -- Loop Optimization Passes

Passes that optimize loop structures before distribution.

| Pass | Stage | Purpose |
|------|-------|---------|
| LoopFusion | pattern-pipeline | Fuse compatible loop nests |
| LoopNormalization | pattern-pipeline | Normalize loop bounds and steps |
| LoopReordering | pattern-pipeline | Reorder loop dimensions for locality |
| StencilBoundaryPeeling | pattern-pipeline | Peel stencil boundary iterations |
