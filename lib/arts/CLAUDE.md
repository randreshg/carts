# lib/arts/ — Core Compiler Implementation

## Directory Layout

```
dialect/                 MLIR dialects (IREE-style per-dialect structure)
  core/
    Conversion/
      ArtsToLLVM/        ConvertArtsToLLVM pass + core LLVM patterns
      OmpToArts/         OpenMP-to-ARTS conversion
    Transforms/          All ARTS-structural passes (EDT, DB, epoch, loop, codegen, general)
  rt/
    IR/                  arts_rt dialect (RtDialect.cpp, RtOps.cpp)
    Conversion/
      ArtsToRt/          EDT/epoch lowering to arts_rt ops
      RtToLLVM/          arts_rt LLVM conversion patterns
    Transforms/          Post-lowering optimizations (DataPtrHoisting, GuidRangCallOpt)
  sde/
    IR/                  sde dialect (SdeDialect.cpp, SdeOps.cpp)
    Conversion/
      SdeToArts/         SDE-to-ARTS conversion patterns
    Transforms/          SDE-level passes (CollectMetadata, ConvertOpenMPToSde)
analysis/                Analysis framework
  graphs/                DB and EDT dependency graphs
  heuristics/            Partitioning and distribution decision logic
passes/
  verify/                Verification barrier passes
utils/                   Shared utilities (attribute names, debug macros, value helpers)
```
