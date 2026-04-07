# opt/general/ -- General Optimization Passes

Domain-agnostic optimization passes.

| Pass | Stage | Purpose |
|------|-------|---------|
| RaiseMemRefDimensionality | raise-memref-dimensionality | Raise nested pointers to N-D memrefs |
| Inliner | initial-cleanup | Inline small functions |
| DeadCodeElimination | initial-cleanup | Remove dead operations |
| HandleDeps | edt-transforms | Handle OpenMP dependency lowering |
