---
paths:
  - "lib/arts/passes/**/*"
  - "include/arts/passes/**/*"
---

- Access DB/EDT analysis ONLY through `AM->getDbAnalysis()` and `AM->getEdtAnalysis()` — never access graphs directly
- Use `AttrNames::Operation` for attribute names — never hardcode strings
- DB passes use `Db` prefix, EDT passes use `Edt` prefix
- Every new pass needs: source in `lib/arts/passes/`, declaration in `Passes.h`, registration in `Compile.cpp`, lit test in `tests/contracts/`
- Debug channel: `snake_case(PassFilename)` (e.g., `DbPartitioning.cpp` -> `db_partitioning`)
- All passes must be thread-safe — no global/static mutable state
- Use `signalPassFailure()` for failures, prefer early exits over deep nesting
