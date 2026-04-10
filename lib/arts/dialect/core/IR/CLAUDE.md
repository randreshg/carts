# dialect/core/IR/ — Core ARTS Dialect Implementation

## Layout

```
Dialect.cpp            ArtsDialect::initialize() + verifiers + builders
```

## TableGen Files (NOT in this directory)

The core dialect's TableGen definitions and public header remain at their
original locations due to generated include path dependencies:

```
include/arts/
  Dialect.td         → generates arts/OpsDialect.h.inc
  Ops.td             → generates arts/Ops.h.inc, arts/Ops.cpp.inc
  Attributes.td      → generates arts/OpsAttributes.h.inc
  Types.td           → generates arts/OpsTypes.h.inc
  Dialect.h          → public header (includes all generated .h.inc files)
```

Moving these would change the generated include paths (`arts/Ops.h.inc` →
`arts/dialect/core/IR/ArtsOps.h.inc`), breaking 135+ consumer files.
The `.td` files logically belong to this dialect but physically stay at
`include/arts/` for build system stability.
