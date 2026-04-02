---
paths:
  - "lib/arts/analysis/**/*"
  - "include/arts/analysis/**/*"
---

- Always use analysis interfaces: `AM->getDbAnalysis()`, `AM->getEdtAnalysis()`, `AM->getLoopAnalysis()`
- Get graphs via analysis: `AM->getDbAnalysis().getOrCreateGraph(func)`, not directly
- Node lookup: `AM->getDbAnalysis().getDbAcquireNode(acquire)`, `AM->getEdtAnalysis().getEdtNode(edt)`
- The `getDbGraph()`/`getEdtGraph()` shortcuts were removed — do NOT re-add them
- Analysis invalidation calls must be serialized (not parallel-safe)
