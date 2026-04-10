# lib/arts/ — Core Compiler Implementation

## Directory Layout

```
dialect/                 MLIR dialects (IREE-style per-dialect structure)
  core/                  THE COMPILER — all passes, analysis, transforms
    Analysis/            All analysis (db, edt, graphs, heuristics, loop, metadata)
    Conversion/
      ArtsToLLVM/        ConvertArtsToLLVM pass + core LLVM patterns
      ArtsToRt/          EDT/epoch lowering (core → rt conversion)
      OmpToArts/         OpenMP-to-ARTS conversion
      SdeToArts/         SDE-to-ARTS conversion patterns
    IR/                  Core dialect definition
    Transforms/          All passes + transform libraries (db/, edt/, loop/, kernel/, dep/)
  rt/                    THIN — dialect def + post-lowering optimizations
    IR/                  arts_rt dialect (RtDialect.cpp, RtOps.cpp)
    Conversion/
      RtToLLVM/          arts_rt → LLVM conversion patterns
    Transforms/          DataPtrHoisting, GuidRangCallOpt, RuntimeCallOpt
  sde/                   SEMANTIC — metadata collection + OMP conversion
    IR/                  sde dialect (SdeDialect.cpp, SdeOps.cpp)
    Transforms/          CollectMetadata, ConvertOpenMPToSde, RaiseToLinalg
codegen/                 Shared lowering infra (ArtsCodegen)
passes/
  verify/                Verification barrier passes
utils/                   Shared utilities (attrs, debug, loop, value, metadata)
```
