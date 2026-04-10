# include/arts/ — Public C++ Headers

## Directory Layout

```
dialect/           MLIR dialect definitions
  core/
    Analysis/      Analysis interfaces (AnalysisManager.h, graphs/, heuristics/)
    Conversion/    ArtsToLLVM, ArtsToRt internal headers
    IR/            Core dialect TableGen + generated headers
    Transforms/    Transform library headers (db/, edt/, kernel/, loop/, dep/)
  rt/
    IR/            RtDialect.h, RtOps.td
    Transforms/    DataPtrHoistingInternal.h, Passes.h/td
  sde/
    IR/            SdeDialect.h, SdeOps.td
passes/            Pass declarations (Passes.h, Passes.td)
utils/             Utilities
  OperationAttributes.h   AttrNames::Operation — centralized attribute name strings
  StencilAttributes.h     AttrNames::Operation::Stencil — stencil attribute names
  Debug.h                 ARTS_DEBUG/ARTS_INFO/ARTS_WARN macros
  ValueAnalysis.h         SSA value inspection helpers
```
