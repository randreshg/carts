# ARTS Dialect

ARTS is the direct isolation and abstract ARTS-object dialect. It materializes
SDE plan facts into isolated task bodies, DBs, EDTs, dependencies, epochs, and
resource-binding objects without exposing runtime ABI calls.

ARTS must not rediscover source semantics, owner dims, dependency-window
legality, or implicit task captures.

Primary docs:

- [`analysis.md`](./analysis.md)
- [`optimizations.md`](./optimizations.md)
