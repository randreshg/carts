---
paths:
  - "tests/**/*"
---

- MLIR lit tests go in `tests/contracts/` using FileCheck
- RUN line: `// RUN: %carts-compile %s --O3 --arts-config %S/../examples/arts.cfg --pipeline <stage> | %FileCheck %s`
- Integration tests go in `tests/examples/` with source, `arts.cfg`, and expected output
- Run: `carts lit <file>.mlir` (single) or `carts test` (all)
- Cover both positive and negative cases; use `CHECK-NOT` / `CHECK-DAG` as needed
