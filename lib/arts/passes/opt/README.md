# opt/ -- Optimization Passes

Optimization passes organized by domain:

| Directory | Domain | Key passes |
|-----------|--------|------------|
| `db/` | DataBlock optimizations | Partitioning, mode tightening, scratch elimination |
| `edt/` | EDT optimizations | Structural opt, alloca sinking, ICM, kernel planning |
| `epoch/` | Epoch optimizations | CPS chain formation, scheduling, structural cleanup |
| `codegen/` | Code generation | Pointer hoisting, alias scopes, vectorization hints |
| `loop/` | Loop optimizations | Fusion, normalization, reordering, stencil peeling |
| `kernel/` | Kernel transforms | Kernel-level pattern transforms |
| `general/` | General optimizations | Inlining, DCE, memref raising, dependency handling |
