# include/arts/ — Public C++ Headers

## Directory Layout

```
dialect/           MLIR dialect definitions (ArtsDialect.h, ArtsOps.h, ArtsTypes.h)
passes/            Pass declarations (Passes.h, Passes.td)
analysis/          Analysis interfaces (AnalysisManager.h, graphs/, heuristics/)
utils/             Utilities
  OperationAttributes.h   AttrNames::Operation — centralized attribute name strings
  StencilAttributes.h     AttrNames::Operation::Stencil — stencil attribute names
  Debug.h                 ARTS_DEBUG/ARTS_INFO/ARTS_WARN macros
  ValueUtils.h            SSA value inspection helpers
```
