---
paths:
  - "tools/compile/**/*"
---

- `Compile.cpp` is the single source of truth for pipeline stage ordering
- Use `carts pipeline --json` to verify stage ordering after modifications
- New passes must be registered in the correct pipeline stage
- Never bypass the pipeline ordering — stages have strict dependencies
